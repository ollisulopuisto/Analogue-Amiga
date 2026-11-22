// Host-side test that exercises the REAL input_update() from
// src/MPUBIOS/drivers/KMIO/inputs.cpp against mock APF registers
// (tests/firmware_support/), pinning the per-poll MMIO read counts of the
// shared controller snapshots:
//
//   - CONTROLLER_KEY_REG(1) is read exactly once per poll in mouse mode, its
//     snapshot shared between update_mouse_inputs() and the shortcut checks
//     (previously two reads per poll).
//   - CONTROLLER_JOY_REG(1) is read exactly once per poll in analog-stick
//     mouse mode, so both axes come from the same sample (previously two).
//
// Both checks fail on the pre-snapshot code. See test_input_update_firmware.cpp
// for the AFP_REGISTOR(5) poll-count pin.
//
// Built and run by `make -C src/MPUBIOS/drivers/KMIO/test test`.

#include <cstdint>
#include <cstdio>

static uint32_t s_afp_regs[8] = {0};
static uint32_t s_key[4] = {0};
static uint32_t s_joy[4] = {0};
static uint32_t s_trig[4] = {0};
static uint64_t s_key1_reads = 0;
static uint64_t s_joy1_reads = 0;

uint32_t mock_afp_reg_read(int idx) { return s_afp_regs[idx]; }
uint32_t mock_controller_key_read(int idx) { if (idx == 1) s_key1_reads++; return s_key[idx - 1]; }
uint32_t mock_controller_joy_read(int idx) { if (idx == 1) s_joy1_reads++; return s_joy[idx - 1]; }
uint32_t mock_controller_trig_read(int idx) { return s_trig[idx - 1]; }
static uint32_t s_core = 0;
uint32_t mock_core_output_read(void) { return s_core; }
void mock_core_output_write(uint32_t v) { s_core = v; }

void timer1Interrupts_reset(void) {}
void timer1Interrupts_setup(int ms) { (void)ms; }
uint8_t HPS_spi_uio_cmd8(uint8_t cmd, uint8_t parm) { (void)cmd; (void)parm; return 0; }
void OsdUpdate(int n) { (void)n; }
void OsdClear() {}
void OsdWrite(const char* s) { (void)s; }
void OsdDisable() {}
void InfoEnable(int a, int b, int c, int d, int e, int f) { (void)a;(void)b;(void)c;(void)d;(void)e;(void)f; }
void minimig_fdd_swap_disks() {}
extern "C" void pocket_amiga_shortcuts_init(void) {}
extern "C" void pocket_amiga_shortcuts_update(uint32_t c1, bool cd32, bool osd) { (void)c1;(void)cd32;(void)osd; }
extern "C" bool pocket_amiga_shortcuts_suppress_joystick(void) { return false; }
unsigned int keytable[16] = {0};

#ifndef INPUTS_CPP_UNDER_TEST
#define INPUTS_CPP_UNDER_TEST "../src/MPUBIOS/drivers/KMIO/inputs.cpp"
#endif
#include INPUTS_CPP_UNDER_TEST

static int failures = 0;
#define CHECK(cond) do { \
  if (!(cond)) { failures++; printf("FAIL line %d: %s\n", __LINE__, #cond); } \
} while (0)

static void reset_state() {
  for (int i = 0; i < 8; i++) s_afp_regs[i] = 0;
  for (int i = 0; i < 4; i++) { s_key[i] = 0; s_joy[i] = 0; s_trig[i] = 0; }
  s_key1_reads = 0;
  s_joy1_reads = 0;
  s_core = 0;
  mouse_enabled = 0;
  mouse_enabled_press = 0;
  mouse_enabled_press_old = 0;
  keyboard_osd_showing = 0;
}

int main() {
  const int N = 500;

  // Persistent mouse mode (afp5 top byte 0x80): one KEY_REG(1) read per poll.
  reset_state();
  s_afp_regs[5] = 0x80000000 | 5;
  for (int i = 0; i < N; i++) input_update();
  CHECK(s_key1_reads == (uint64_t)N);

  // Analog-stick mouse mode (c1 top nibble 0x3, stick deflected): one
  // KEY_REG(1) and one JOY_REG(1) read per poll.
  reset_state();
  s_afp_regs[5] = 0x80000000 | 5;
  s_key[0] = 0x30000000;
  s_joy[0] = 0x0000A0A0;
  for (int i = 0; i < N; i++) input_update();
  CHECK(s_key1_reads == (uint64_t)N);
  CHECK(s_joy1_reads == (uint64_t)N);

  // The mouse path still functions through the shared snapshot: pressing the
  // mouse-enable toggle (bit 15) flips mouse_enabled on the rising edge.
  reset_state();
  s_afp_regs[5] = 0x80000000 | 5;
  s_key[0] = (1u << 15);
  input_update();
  CHECK(mouse_enabled == 1);
  s_key[0] = 0;
  input_update();
  CHECK(mouse_enabled == 1);
  s_key[0] = (1u << 15);
  input_update();
  CHECK(mouse_enabled == 0);

  if (failures == 0) {
    printf("test_input_poll_snapshots: all checks passed\n");
    return 0;
  }
  printf("test_input_poll_snapshots: %d failure(s)\n", failures);
  return 1;
}
