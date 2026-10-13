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

# sdram_ctrl (clk_114) samples the clk_sys chip and CPU buses only in its RAS
# slot. sdram_state is reset to 0 one clk_114 cycle after c1 rises on a
# clk_sys edge, so the state-0 registers load two clk_114 cycles after the
# last clk_sys edge: a two-cycle setup, hold kept at the launch edge.
set clk_sys [get_clocks {ic|pll|pll_inst|altera_pll_i|general[1].gpll~PLL_OUTPUT_COUNTER|divclk}]
foreach r {datawr casaddr cas_dqm cas_sd_we cas_sd_cas sd_addr sd_ba sd_ras sd_cas sd_we
           slot_type chipWE write_ack rcnt snoop_adr snoop_dat snoop_bs} {
 set_multicycle_path -from $clk_sys -to [get_registers "*|sdram_ctrl:ram1|${r}*"] -setup -end 2
 set_multicycle_path -from $clk_sys -to [get_registers "*|sdram_ctrl:ram1|${r}*"] -hold -end 1
}

# The cache answers the CPU (cpu_ack, cpu_dat_r) only on the second clk_114
# edge after a clk_sys edge (hits; reads start in the cycle that follows a
# clk_sys edge) or two cycles before one (fills, sdram_state 7). The CPU
# samples on the next clk_sys edge: two clk_114 cycles at least. They are the
# cache's only outputs to clk_sys; register retiming renames them, so the
# whole cache is the source.
set cache_regs [get_registers {*|sdram_ctrl:ram1|cpu_cache_new:cpu_cache|*}]
set_multicycle_path -from $cache_regs -to $clk_sys -setup -start 2
set_multicycle_path -from $cache_regs -to $clk_sys -hold -start 1
