# Dump the worst timing paths and clock-domain transfers after a compile.
# Run from src/fpga: quartus_sta -t ../../.github/scripts/timing_paths.tcl
project_open ap_core
create_timing_netlist
read_sdc
update_timing_netlist

foreach corner {slow_1100mv_85c fast_1100mv_0c} {
  set_operating_conditions $corner
  update_timing_netlist
  report_timing -setup -npaths 50 -detail summary -file output_files/timing_setup_$corner.rpt
  report_timing -hold -npaths 50 -detail summary -file output_files/timing_hold_$corner.rpt
}

report_clock_transfers -setup -file output_files/timing_clock_transfers.rpt
report_clocks -file output_files/timing_clocks.rpt

delete_timing_netlist
project_close
