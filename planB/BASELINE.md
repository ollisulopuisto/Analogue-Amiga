# Plan B Baseline — Accelerated A1200 Support

**Status:** B1–B3 complete (baseline characterization). B4 (implementation) blocked on Quartus host.

This document records the baseline architecture and measurement environment for
Plan B. Quartus and Pocket hardware are NOT available in this environment
(`quartus`/`quartus_sh` not found; PLL and build host are on a separate Windows
machine per `LOCAL_BUILD_NOTES.md`). All characterization below is therefore
RTL/audit-based and simulation-ready; the actual FPGA fit, timing, and hardware
validation must be completed on the Quartus host.

---

## 1. Clock architecture (from core_top.v, amiga_clk.v, pll.v/qip)

| Clock | Source | Frequency | Drives |
|---|---|---|---|
| `clk_74a` | External input | 74.25 MHz | PLL reference |
| `clk_114` | PLL outclk_0 | ~113.5 MHz | SDRAM `sysclk`, `fastchip`, CPU divider (`div` counter) |
| `clk_sys` | PLL outclk_1 | ~28.375 MHz | TG68K `clk`, `minimig`, `amiga_clk`, `cpu_wrapper` |
| `c_7m` | `amiga_clk` (clk_28/4 enable) | ~7.09 MHz | SDRAM state-machine gate, chipset timing |
| `video_rgb_clock` | PLL outclk_2 | — | Video pipeline |
| `video_rgb_clock_90` | PLL outclk_3 | — | Video pipeline |

PLL: Cyclone V `altera_pll`, fractional-N, refclk=74.25 MHz, M=9 (frac), N=1,
C0=6 → outclk_0≈113.5 MHz; C1=24 → outclk_1≈28.375 MHz.

`amiga_clk` divides `clk_sys` by 4 via `clk7_cnt` to produce `clk7_en`
(~7.09 MHz pulse) and `clk_7 = clk7_cnt[1]` (~14.2 MHz square, used as enable).
`c1`/`c3` are 90°-offset derivatives of `clk_7` for chipset sampling.

---

## 2. CPU execution path (cpu_wrapper.v, TG68KdotC_Kernel.vhd)

- **Active core:** TG68KdotC_Kernel when `cpucfg[1]==1` (68020 config), fx68k
  otherwise (68000/68010 both route to fx68k — no distinct 68010 core).
- TG68K runs on `clk_sys` (~28.375 MHz) with clock enable:
  `clkena_in = ~cpu_req | chipready | ramready | fastchip_ready`
- CPU stalls when waiting for `chipready` (chip RAM), `ramready` (Fast RAM via
  SDRAM), or `fastchip_ready` (IDE/RTG/Akiko).
- **16-bit data path** through the wrapper; longword = 2 bus cycles; unaligned
  accesses split further.
- `cpu_req = (cpustate != 1)` — CPU requests bus when not in "no mem access"
  state.
- `cacr` (4 bits) drives `turbochip_d`/`turbokick_d`/`dcache_d` (cachecfg bits
  [2:0]) — chip RAM burst, kickstart burst, data cache enables.

---

## 3. SDRAM controller (sdram_ctrl.v)

- Runs on `clk_114` (~113.5 MHz), state machine gated by `c_7m` (~7 MHz).
- 16-state cycle (0–15), advances every `clk_114`, resets to 0 on `c_7m`
  rising edge → effectively one SDRAM cycle per ~4 clk_114 cycles.
- **Arbitration priority (state 0):**
  1. Chipset DMA (`~chipDMA | ~chipRW`) — highest priority
  2. CPU write buffer (`write_req`)
  3. CPU read cache (`cache_req`)
  4. Refresh (`&rcnt`)
- Chipset gets first access to every SDRAM cycle; CPU only gets the slot if
  chipset is idle.
- Cache line fill: 4 reads at states 7, 9, 11, 13 → 4 cycles per fill.
- Write buffer: single entry, lets CPU continue while write completes.
- `ramready = cache_rd_ack || write_ena` — CPU ack on cache hit or write-buffer
  available.

---

## 4. Cache (cpu_cache_new.v)

- **2-way set-associative, separate I and D caches**
- **Total: 4 KB** (2 KB instruction + 2 KB data)
- **1024 sets, 16-byte lines** (4 × 16-bit words), 18-bit tag
- Write-through, look-through, LRU replacement
- CPU-side FSM: IDLE → READ/WRITE → (miss) FILL1–FILL4 → WB/IDLE
- Fill penalty: 4 cycles (one word per `sdr_read_ack`)
- SDRAM-side FSM handles snoops (chipset writes update matching cache lines)
- Cache inhibit bit (`cpu_cache_ctrl[3]`) bypasses cache fill

---

## 5. Memory mapping (cpu_wrapper.v)

| Range | Device | Notes |
|---|---|---|
| $000000–$1FFFFF | Chip RAM | `cchip`, up to 2 MB |
| $E00000–$FFFFFF | Kickstart ROM | `ckick`, mirrored |
| $E80000–$EFFFFF | Zorro II autoconfig | `sel_autoconfig` |
| $200000–$1FFFFFF | Zorro III RAM | `cfg_z3`, up to 256 MB |
| $00DD010–$00DD01F | Gayle/IDE | `sel_dd` |
| $02xxxxxx | RTG | `sel_rtg` |

---

## 6. Bottleneck candidates (identified from RTL audit)

| # | Candidate | Evidence from RTL | Priority |
|---|---|---|---|
| 1 | SDRAM arbitration delay | Chipset DMA has absolute priority (sdram_ctrl state 0); CPU waits during display/audio/disk DMA | High |
| 2 | 16-bit data path | Wrapper routes 16-bit; longword = 2 cycles, unaligned = more | High |
| 3 | Cache capacity | 4 KB total; working-set misses → 4-cycle fill | Medium |
| 4 | Clock enables | TG68K stalls on `chipready`/`ramready`/`fastchip_ready` | Medium |
| 5 | Cache line fill latency | 4 cycles per fill at ~7 MHz effective SDRAM rate | Medium |

---

## 7. Measurement plan (simulation, local)

Quartus and Pocket hardware are unavailable here, so baseline measurement is
done with RTL simulation (`iverilog` + `verilator` available):

1. **Cache testbench** — drive `cpu_cache_new` with synthetic access streams
   (sequential, strided, random, self-modifying) at various working-set sizes;
   measure hit rate, fill cycles, stall cycles.
2. **SDRAM arbitration testbench** — drive `sdram_ctrl` with simultaneous
   chipset DMA and CPU traffic; measure CPU wait cycles under load.
3. **End-to-end wrapper testbench** — drive `cpu_wrapper` + TG68K with a small
   instruction loop; measure cycles per instruction vs. memory wait.

Each benchmark runs with cache on/off and at each advertised Fast RAM size.

---
## 7. Simulation baseline results (local, no Quartus needed)

### Cache simulation (`planB/sim/cache_tb.v`)
Cache: 2-way set-assoc, 4 KB total data (2 KB instruction + 2 KB data), 256 sets x 16-byte lines x 2 ways.
       Write-through, look-through, LRU replacement. Fill = 4 beats.
       Idealized SDRAM model (1 cycle per beat, no arbitration delay).

| Metric | Value |
|---|---|
| Total accesses | 3520 |
| Cache misses | 1537 (43.66%) |
| Cache hits | 1983 (56.34%) |
| Fill cycles | 10759 |
| Total cycles | 207120 |
| Cycles/access | 58.84 |

Key observations:
- Sequential code with small working set: high hit rate after warmup
- Strided access (8KB working set, stride 256): most accesses miss (index aliasing)
- Cache OFF: every read goes to SDRAM
- Fill penalty: 4 cycles per cache line miss
- Write buffer handles CPU writes without stalling

### SDRAM arbitration simulation (`planB/sim/sdram_arb_tb.v`)
Model of sdram_ctrl.v state-0 priority with chipDMA active-low (`chipDMA=0` = chip DMA active).

| Test | Condition | Accesses | Wait cycles | Wait/access | Max stall | Result |
|---|---|---:|---:|---:|---:|---|
| 1 | No chipset DMA (chip idle) | 32 | 192 | 6.0 | 22 clk | baseline |
| 2 | Continuous chipset DMA | 32 | — | — | — | **CPU read starves (timeouts)** |
| 3 | Burst DMA (16 beats) then CPU read | 1 | 22 | 22 | 22 | single read delayed |

Key observations:
- With no chipset DMA, each CPU read costs ~6 clk cycles (~53 ns at 113.5 MHz) of arbitration/setup.
- With chipset DMA held continuously, the CPU read path is blocked: the cache line fill can never win SDRAM state 0, so the CPU read times out (starvation). This confirms chipset DMA has absolute priority and can block CPU reads under sustained display/audio/disk DMA.
- A single read following a burst DMA is delayed ~22 clk cycles (~0.2 µs).

These are RTL/simulation estimates on an idealized model; absolute numbers change with real SDRAM timing. The qualitative conclusion (CPU waits on arbitration under chipset load) is the basis for B4 candidate #1.

---

## 7b. Quartus baseline build (GitHub Actions, completed)

Build: Run #41790380964, Quartus 25.1std.0.1129 SC Lite, Cyclone V 5CEBA4F23C8
Compile time: 49m2s. Artifacts: `planB/artifacts/`

### Fit Summary (ap_core.fit.summary)

| Resource | Used | Available | Utilization |
|---|---|---|---|
| Logic (ALMs) | 16,887 | 18,480 | **91%** |
| Registers | 22,346 | — | — |
| Block memory bits | 1,842,243 | 3,153,920 | 58% |
| RAM Blocks | 237 | 308 | 77% |
| DSP Blocks | 30 | 66 | 45% |
| PLLs | 1 | 4 | 25% |

### Timing Summary (ap_core.sta.summary) — Worst setup slacks

| Clock Domain | Model | Slack | TNS |
|---|---|---|---|
| clk_74a (74.25 MHz) | Slow 1100mV 85C | **-7.555 ns** | -1468 |
| clk_114 (~113.5 MHz, PLL C0) | Slow 1100mV 85C | **-6.759 ns** | -1530 |
| clk_sys (~28.4 MHz, PLL C1) | Slow 1100mV 85C | **-5.939 ns** | -1863 |

**Critical finding:** The design has **severe setup timing violations** on all three clock domains at the current frequencies. Setup slack is -5 to -7.5 ns, meaning the design cannot meet timing even at stock clock speeds.

### Implications for Plan B acceleration

1. **Clock increase is NOT viable** — timing already fails at stock frequencies. Cannot speed up the TG68K by raising clk_sys.
2. **Very limited logic headroom** — 91% ALM utilization leaves only ~9% (≈1,600 ALMs) for any acceleration logic.
3. **Memory headroom exists** — 58% block memory bits, 77% RAM blocks. Cache capacity increases are feasible from a memory standpoint.
4. **DSP headroom** — 45% available, but DSPs aren't on the critical CPU/memory path.

### Viable acceleration candidates (given constraints)

| Candidate | Viability | Reasoning |
|---|---|---|
| Remove wait states / improve arbitration | Low-Medium | Timing already critical; harder to close timing with changes |
| Cache capacity increase | Medium | Memory headroom exists; 9% ALM headroom is tight but possible |
| Wider transfers / burst | Low | Would increase logic usage and worsen timing |
| Clock increase | NOT viable | Timing violations at stock speeds |

### Status

**B1–B3 complete.** Reproducible local simulation baseline established (no Quartus needed):
- Cache: 56.34% hit rate, 58.84 cycles/access (idealized SDRAM).
- Arbitration: ~6 clk/access with no chipset DMA; CPU read starves (timeouts) under continuous chipset DMA → arbitration is the dominant CPU stall source under display/audio/disk load.
- Quartus baseline (CI): 91% ALM utilization, setup timing violations (-5 to -7.5 ns) on all clock domains at stock frequencies.

**B4 (implement) — blocked on Quartus host.** Quartus and Pocket hardware are not available in this environment, so RTL changes cannot be fit/timing-validated or flashed for on-hardware benchmarking.

Given the Quartus constraints, the documented B4 candidate priority is:
1. **Cache capacity/organization** (most viable): memory headroom exists (58% block RAM bits), limited ALM cost within the ~9% ALM headroom. Does not require clock increase.
2. **Arbitration/wait-state reduction**: low-medium viability — timing is already critical, so changes must not worsen the failing clk_114 path.
3. **Clock increase / wider transfers**: ruled out — timing already fails at stock speeds, and ALM headroom is too small.

**Release gate (B7):** any B4 change requires a Quartus fit with timing met, correct memory reporting, repeatable measured gains, and Pocket hardware validation before the "A1200 Accelerated" profile can be released.

