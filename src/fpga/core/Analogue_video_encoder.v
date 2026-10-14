

/***************************************************************************


	Video Core - THis does the decoding of the Vs and HS signals and the coding
	for the interlacing.

	Picture window
	--------------
	The DE window sent to the Pocket is exactly the size of the scaler slot
	selected at the end of each line (see video.json). Interlaced screens are
	sent as progressive fields (each field on the same rows), so every slot is
	one field high.

	   slot  mode           width x lines   aspect
	   0     NTSC Fit       704 x 240       44:35   (pixel aspect 6:7)
	   1     NTSC Stretch   704 x 240       10:9
	   2     NTSC Fill      568 x 220       ~10:9
	   3     PAL Fit        704 x 288       275:216 (pixel aspect 25:24)
	   4     PAL Stretch    704 x 288       10:9
	   5     PAL Fill 1     608 x 288       ~10:9 (5 rows per line)
	   6     PAL Fill 2     560 x 264       ~10:9
	   7     PAL Fill 3     512 x 240       10:9  (6 rows per line)

	One hires pixel is two video clocks (DE high every clock, SKIP on every
	other one), so the DE window is 2*width clocks long.

	Position: the window is centred on the Amiga display window (Agnus DIW:
	hde = horizontal window of the frame, vdiwena = vertical window),
	measured every frame. A new position is only taken once the measurement
	has been identical for AUTO_STABLE frames and differs from the one in use
	by more than one lores pixel / one line, so the picture does not jump.
	Until the first measurement a standard 320x256 / 320x200 screen is
	assumed.


*****************************************************************************/
module Analogue_video_encoder(
	input						clk_74a,
	input 					bridge_wr,
	input						bridge_rd,
	input [31:0] 			bridge_addr,
	input [31:0] 			bridge_wr_data,
	output reg [31:0]		vga_bridge_rd_data,

	input 					video_rgb_clock,
	input 					hs,
	input 					vs,
	input 					hblank_i,
	input 					vblank_i,
	input [7:0]				r,
	input [7:0]				g,
	input [7:0]				b,
	input 					field1,
	input						lace,
	input [1:0]				res,
	input 					ce_pix,
	input 					clk7_en,
	input 					clk7n_en,
	input 					diw_h,
	input 					diw_v,


	input 	 				LED,
	input 					light_enable,
	input 					ide_c_led,
	input 					ide_f_led,

	output reg [1:0]		control_mode,
	output reg [23:0] 	video_rgb_reg,
	output reg 				video_hs_i_reg,
	output reg 				video_vs_i_reg,
	output reg 				video_de_reg,
	output 					video_skip
);

// frames the measured window must stay the same before Auto moves the picture
parameter AUTO_STABLE = 5'd24;
// usable clocks of an active line (hblank is 377 lores pixels = 1508 clocks)
localparam [10:0] LINE_CLKS = 11'd1500;
localparam [8:0]  Y_MIN     = 9'd4;

reg [23:0] rgb_reg;

always @(posedge video_rgb_clock) begin
	if (ce_pix)rgb_reg <= {r, g, b};
end

reg hs_reg, vs_reg, hblank_i_reg;
reg hs_delay0, hs_delay1, hs_delay2, hs_delay3;
reg 				video_de_reg_i;

// ---------------------------------------------------------------------------
// Menu registers (clk_74a)
// ---------------------------------------------------------------------------
reg [1:0] pic_mode = 2'd0;          // 0 = Fit, 1 = Fill, 2 = Stretch
reg [1:0] fill_crop = 2'd2;         // Fill crop level 1..3
wire [1:0] pic_mode_s;
wire [1:0] fill_crop_s;
reg [31:0] vga_bridge_rd_data_reg;

always @(posedge clk_74a) begin
	if (bridge_wr && bridge_addr[31:8] == 24'hA00000) begin
		case (bridge_addr[7:0])
			8'h08 : control_mode <= bridge_wr_data[1:0];
			8'h0C : pic_mode     <= bridge_wr_data[1:0];
			8'h10 : fill_crop    <= bridge_wr_data[1:0];
		endcase
	end
end

always @(posedge clk_74a) begin
	if (bridge_rd) begin
		case (bridge_addr[7:0])
			8'h0C : vga_bridge_rd_data_reg <= pic_mode;
			8'h10 : vga_bridge_rd_data_reg <= fill_crop;
		endcase
	end
	vga_bridge_rd_data <= vga_bridge_rd_data_reg;
end

synch_3 #(.WIDTH(2)) fill_crop_sync(fill_crop, fill_crop_s, video_rgb_clock);
synch_3 #(.WIDTH(2)) pic_mode_sync(pic_mode, pic_mode_s, video_rgb_clock);

// ---------------------------------------------------------------------------
// Beam counters in the video clock
// ---------------------------------------------------------------------------
reg [10:0] hcnt = 0;        // clocks since the end of horizontal blanking
reg [8:0]  yc = 0;          // hsyncs since vsync
wire hs_fall  = ~hs && hs_reg;
wire vs_fall  = ~vs && vs_reg;
wire hb_fall  = ~hblank_i && hblank_i_reg;

// ---------------------------------------------------------------------------
// Display window measurement (Agnus DIW)
// ---------------------------------------------------------------------------
reg [2:0] diw_h_sr, diw_v_sr;
wire diw_h_s  = diw_h_sr[1];
wire diw_h_p  = diw_h_sr[2];
wire diw_v_s  = diw_v_sr[1];

reg [10:0] xr_line, xr_frame, xf_frame;
reg [8:0]  y_first, y_last;
reg        h_seen = 0, v_seen = 0;
reg        meas_tick = 0, meas_ok = 0;
reg [11:0] m_cx;        // xr + xf  (twice the centre, in clocks)
reg [9:0]  m_cy;        // first + last line (twice the centre)

always @(posedge video_rgb_clock) begin
	diw_h_sr <= {diw_h_sr[1:0], diw_h};
	diw_v_sr <= {diw_v_sr[1:0], diw_v};
	meas_tick <= 1'b0;

	if (hb_fall) xr_line <= 11'd0;
	else if (~hblank_i) begin
		if (diw_h_s && ~diw_h_p) xr_line <= hcnt;
		if (~diw_h_s && diw_h_p) begin
			xr_frame <= xr_line;
			xf_frame <= hcnt;
			h_seen   <= 1'b1;
		end
	end

	if (hb_fall && diw_v_s) begin
		if (~v_seen) y_first <= yc;
		y_last <= yc;
		v_seen <= 1'b1;
	end

	if (vs_fall) begin
		meas_tick <= 1'b1;
		meas_ok   <= h_seen & v_seen;
		m_cx      <= {1'b0, xr_frame} + {1'b0, xf_frame};
		m_cy      <= {1'b0, y_first} + {1'b0, y_last};
		h_seen    <= 1'b0;
		v_seen    <= 1'b0;
	end
end

// ---------------------------------------------------------------------------
// Hysteresis
// ---------------------------------------------------------------------------
reg [11:0] p_cx = 0, a_cx = 0;
reg [9:0]  p_cy = 0, a_cy = 0;
reg [4:0]  stable = 0;
reg        a_valid = 1'b0;
reg        apply_tick = 0;
wire signed [12:0] d_cx = {1'b0, p_cx} - {1'b0, a_cx};
wire signed [10:0] d_cy = {1'b0, p_cy} - {1'b0, a_cy};
// more than one lores pixel (4 clocks, 8 in the doubled centre) / one line
wire moved = (d_cx > 13'sd8) || (d_cx < -13'sd8) || (d_cy > 11'sd2) || (d_cy < -11'sd2);

always @(posedge video_rgb_clock) begin
	apply_tick <= 1'b0;
	if (meas_tick) begin
		if (meas_ok && m_cx == p_cx && m_cy == p_cy) begin
			if (stable != AUTO_STABLE) stable <= stable + 1'b1;
		end
		else stable <= 5'd0;
		p_cx <= m_cx;
		p_cy <= m_cy;
		apply_tick <= 1'b1;
	end
	if (apply_tick && stable == AUTO_STABLE && (~a_valid || moved)) begin
		a_cx    <= p_cx;
		a_cy    <= p_cy;
		a_valid <= 1'b1;
	end
end

// ---------------------------------------------------------------------------
// Window position (registered pipeline, latched at vsync)
// ---------------------------------------------------------------------------
reg        mode_pal = 1'b1;
reg [2:0]  mode_slot = 3'd3;                   // slot of the window being sent
reg [10:0] w_clks;                             // 2 * width
reg [9:0]  w_half;                             // width in hires px = half window in clocks
reg [8:0]  h_lines;                            // lines per field
reg [10:0] x_max;
reg [8:0]  y_max;
reg signed [12:0] x_auto;
reg signed [10:0] y_auto;
reg [10:0] x_req, x_start;
reg [8:0]  y_req, y_start;
reg [10:0] xs = 0, xe = 0;
reg [8:0]  ys = 0, ye = 0;
reg [2:0]  slot = 0;

// centre in use: the measured one, or a standard screen until the first measurement
wire [11:0] c_cx = a_valid ? a_cx : 12'd1604;
wire [9:0]  c_cy = a_valid ? a_cy : (mode_pal ? 10'd339 : 10'd283);

always @(posedge video_rgb_clock) begin
	case (mode_slot)
		3'd2:    begin w_half <= 10'd568; h_lines <= 9'd220; end
		3'd3,
		3'd4:    begin w_half <= 10'd704; h_lines <= 9'd288; end
		3'd5:    begin w_half <= 10'd608; h_lines <= 9'd288; end
		3'd6:    begin w_half <= 10'd560; h_lines <= 9'd264; end
		3'd7:    begin w_half <= 10'd512; h_lines <= 9'd240; end
		default: begin w_half <= 10'd704; h_lines <= 9'd240; end
	endcase
	w_clks  <= {w_half, 1'b0};
	x_max   <= LINE_CLKS - w_clks;
	y_max   <= (mode_pal ? 9'd310 : 9'd260) - h_lines;

	x_auto  <= $signed({2'b00, c_cx[11:1]}) - $signed({3'b000, w_half});
	y_auto  <= ($signed({1'b0, c_cy}) + 11'sd1 - $signed({2'b00, h_lines})) >>> 1;

	begin
		x_req <= x_auto[12] ? 11'd0 : (|x_auto[11:11] ? 11'h7FF : x_auto[10:0]);
		y_req <= y_auto[10] ? 9'd0  : (|y_auto[9]     ? 9'h1FF  : y_auto[8:0]);
	end

	x_start <= (x_req > x_max) ? x_max : x_req;
	y_start <= (y_req > y_max) ? y_max : ((y_req < Y_MIN) ? Y_MIN : y_req);
end

// ---------------------------------------------------------------------------
// Video output
// ---------------------------------------------------------------------------
reg [11:0] x_counter_vga;   // DE clocks in the line (LED overlay)
reg [11:0] y_counter_vga;   // DE lines in the frame (LED overlay)
reg 		 lace_reg;
reg 		 field1_reg;
reg        y_in = 0;
reg        skip_i = 0;
reg [3:0]  skip_d = 0;          // matches the 4-clock DE delay of the OSD
assign video_skip = skip_d[3];

wire x_in    = (hcnt >= xs) && (hcnt < xe);
wire de_next = ~hblank_i && y_in && x_in;

always @(posedge video_rgb_clock) begin
	video_vs_i_reg <= 1'b0;
	hs_delay0 		<= 1'b0;
	hblank_i_reg <= hblank_i;
	hs_delay1 <= hs_delay0;
	hs_delay2 <= hs_delay1;
	hs_delay3 <= hs_delay2;
	video_hs_i_reg <= hs_delay3;
	video_de_reg_i <= video_de_reg;
	hs_reg <= hs;
	vs_reg <= vs;

	if (hblank_i) hcnt <= 11'd0;
	else if (hcnt != 11'h7FF) hcnt <= hcnt + 1'b1;

	y_in <= (yc >= ys) && (yc < ye);

	if (video_de_reg) x_counter_vga <= x_counter_vga + 1'b1;
	if (video_de_reg && ~video_de_reg_i) y_counter_vga <= y_counter_vga + 1'b1;

	if (hs_fall) begin
		hs_delay0 	<= 1'b1;
		x_counter_vga <= 0;
		field1_reg <= field1;
		yc <= yc + 1'b1;
	end

	video_de_reg <= de_next;
	skip_i       <= de_next && video_de_reg && ~skip_i;
	skip_d       <= {skip_d[2:0], skip_i};

	if (vs_fall) begin
		lace_reg <= lace;
		yc <= 9'd0;
		y_counter_vga <= 0;
		video_vs_i_reg 	<= 1'b1;
		// the window and the scaler slot always describe the same size
		slot <= mode_slot;
		xs <= x_start;
		xe <= x_start + w_clks;
		ys <= y_start;
		ye <= y_start + h_lines;
		mode_pal    <= res[1];
		if (res[1])
			mode_slot <= (pic_mode_s == 2'd2) ? 3'd4 :
			             (pic_mode_s == 2'd1) ? ((fill_crop_s <= 2'd1) ? 3'd5 : (fill_crop_s == 2'd2) ? 3'd6 : 3'd7) :
			             3'd3;
		else
			mode_slot <= (pic_mode_s == 2'd2) ? 3'd1 : (pic_mode_s == 2'd1) ? 3'd2 : 3'd0;

		video_rgb_reg 	<= 24'd0;   // frame feature bits: progressive (interlaced fields are shown one by one)
	end
	else if (de_next) begin
		if ((y_counter_vga <= 15 && y_counter_vga >= 5) && (x_counter_vga <= 30 && x_counter_vga >= 10) && LED && ~light_enable)
			video_rgb_reg 	<= {8'h0, 8'hF0, 8'h0};
		else if ((y_counter_vga <= 15 && y_counter_vga >= 5) && (x_counter_vga <= 60 && x_counter_vga >= 40) && |{ide_c_led, ide_f_led} && ~light_enable)
			video_rgb_reg 	<= {8'hF0, 8'h0, 8'h0};
		else
			video_rgb_reg 	<= rgb_reg;
	end
	else if (video_de_reg) begin
		// end-of-line command on the first clock after DE: set scaler slot
		video_rgb_reg 	<= {8'd0, slot, 13'd0};
	end
	else
		video_rgb_reg 	<= 24'd0;
end
endmodule
