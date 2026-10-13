# Dump timing detail after a compile.
# Run from src/fpga: quartus_sta -t ../../.github/scripts/timing_paths.tcl
project_open ap_core
create_timing_netlist
read_sdc
update_timing_netlist

report_sdc -ignored -file output_files/timing_sdc_ignored.rpt
check_timing -file output_files/timing_check.rpt
report_clock_transfers -setup -file output_files/timing_clock_transfers.rpt
report_clock_transfers -hold -file output_files/timing_clock_transfers_hold.rpt
report_clocks -file output_files/timing_clocks.rpt


foreach corner {8_slow_1100mv_85c 8_slow_1100mv_0c MIN_fast_1100mv_85c MIN_fast_1100mv_0c} {
  set_operating_conditions $corner
  update_timing_netlist
  report_timing -setup -npaths 50 -detail summary -file output_files/timing_setup_$corner.rpt
  report_timing -hold -npaths 50 -detail summary -file output_files/timing_hold_$corner.rpt
  report_timing -recovery -npaths 20 -detail summary -file output_files/timing_recovery_$corner.rpt
  report_timing -removal -npaths 20 -detail summary -file output_files/timing_removal_$corner.rpt
  # Worst paths of every clock pair, with full path detail for the first few.
  foreach_in_collection f [all_clocks] {
    foreach_in_collection t [all_clocks] {
      foreach kind {setup hold} {
        set fn output_files/timing_pair_${kind}_${corner}.rpt
        report_timing -$kind -from_clock $f -to_clock $t \
          -npaths 15 -detail summary -append -file $fn
        report_timing -$kind -from_clock $f -to_clock $t \
          -npaths 2 -detail full_path -append -file output_files/timing_full_${kind}_${corner}.rpt
      }
    }
  }
}

delete_timing_netlist
project_close
