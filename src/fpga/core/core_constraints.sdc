#
# user core constraints
#
# put your clock groups in here as well as any net assignments
#

# All PLL outputs (clk_114, clk_sys, video clocks) are one synchronous
# family; they are matched by their real names, not counter[N].
set pll_clks [get_clocks {ic|pll|pll_inst|altera_pll_i|*|divclk}]

set_clock_groups -asynchronous \
 -group { bridge_spiclk } \
 -group { clk_74a } \
 -group { clk_74b } \
 -group $pll_clks

set dram_chip_clk "ic|pll|pll_inst|altera_pll_i|counter[0].gpll~PLL_OUTPUT_COUNTER|divclk"
set dram_cont_clk "ic|pll|pll_inst|altera_pll_i|counter[1].gpll~PLL_OUTPUT_COUNTER|divclk"

set_input_delay -clock $dram_chip_clk -reference_pin [get_ports {dram_clk}] -max 5.9 [get_ports dram_dq[*]]
set_input_delay -clock $dram_chip_clk -reference_pin [get_ports {dram_clk}] -min 0.9 [get_ports dram_dq[*]] 

set_output_delay -clock $dram_chip_clk -reference_pin [get_ports {dram_clk}] -max 2.0 [get_ports {dram_cke dram_a* dram_ba* dram_cas_n dram_ras_n dram_we_n}]
set_output_delay -clock $dram_chip_clk -reference_pin [get_ports {dram_clk}] -min -1.0 [get_ports {dram_cke dram_a* dram_ba* dram_cas_n dram_ras_n dram_we_n}]
set_multicycle_path -from $dram_chip_clk -to $dram_cont_clk -setup -end 2
