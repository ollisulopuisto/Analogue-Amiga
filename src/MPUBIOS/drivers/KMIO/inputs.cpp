/*
 * Copyright 2022 Murray Aickin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */




#include "inputs.h"
#include "pocket_shortcuts.h"
#include "minimig_config.h"
#include "osd_popup.h"
int interupt_amount_timer = 0;
unsigned int mouse_counter={0};
signed short x_count = 0;
signed short y_count = 0;
bool mouse_enabled = 0;
bool mouse_enabled_press = 0;
bool mouse_enabled_press_old = 0;
bool disk_swap_press_old = 0;
bool next_disk_press_old = 0;
volatile bool fdd_next_disk_request = 0; // L + Start, handled in the main loop

uint8_t keyboard_buffer[6];
bool    MOD_LALT;
bool    MOD_RALT;
bool    MOD_LSHIFT;
bool    MOD_RSHIFT;
bool    MOD_LWIN;
bool    MOD_CTRL;
bool    MOD_CAPS_LOCK;
bool    keyboard_osd_showing = 0;
// static uint8_t timing_delay;

void input_allow_osd(bool keyboard_active){
  keyboard_osd_showing = keyboard_active;
  mainprintf ("Active %d\r\n", keyboard_osd_showing);
}

void update_mouse_inputs (){
  // Start toggles the mouse; L + Start is next disk
  mouse_enabled_press = ((CONTROLLER_KEY_REG(1) >> 15) & 0x1) && !((CONTROLLER_KEY_REG(1) >> 8) & 0x1); //|| ( ((CONTROLLER_KEY_REG(1)>>28) == 0x3) && ((CONTROLLER_KEY_REG(1) >> 15) & 0x1))
  if (keyboard_osd_showing == 1) {
    mouse_enabled = 0;
  } else if (mouse_enabled_press && !mouse_enabled_press_old) {
    mouse_enabled = !mouse_enabled;
    osd_popup_cancel(); // the mouse message takes over the OSD
    
    OsdUpdate(0);
    OsdClear();
    if (mouse_enabled){
      uint32_t tmp = CORE_OUTPUT_REGISTOR();
      tmp = tmp | 0x00000030;
      CORE_OUTPUT_REGISTOR() = tmp;
      OsdWrite("Mouse Enabled");
      OsdUpdate(0);
      OsdClear();
    } else {
      uint32_t tmp = CORE_OUTPUT_REGISTOR();
      tmp = tmp & 0xFFFFFFCF;
      CORE_OUTPUT_REGISTOR() = tmp;
      OsdWrite("Mouse Disabled");
    }
    
    OsdUpdate(1);
    OsdClear();
    OsdUpdate(2);
    OsdClear();
  } 
  mouse_enabled_press_old = mouse_enabled_press;
  if (keyboard_osd_showing == 0){
    if (mouse_enabled_press){
      InfoEnable(10, 10, 32, 3, 291, 31744); // Had to use the info as some of the PCE outputs are smaller
    } else if (!osd_popup_active()) {
      OsdDisable();
    }
  }
  uint16_t speed = (AFP_REGISTOR(5) & 0xffff);
  if (((CONTROLLER_KEY_REG(4)>>28) == 0x5) && (mouse_counter != (CONTROLLER_KEY_REG(4) & 0x0000FFFF))){
    // the normal mouse access via the dock
    // Right now this is just a shifter Will change this to something more accurate as this causes an issue with the emulated mouse
    signed short x = (short)((CONTROLLER_JOY_REG(4) & 0x0000FF00));
    signed short y = (short)((CONTROLLER_TRIG_REG(4) & 0x0000FF00));
    x = x / (speed << 5);
    y = y / (speed << 5);
    x_count = x_count + ((x < -127) ? -127 : (x > 127) ? 127 : x);
    y_count = y_count + ((y < -127) ? -127 : (y > 127) ? 127 : y);
    HPS_spi_uio_cmd8(UIO_MOUSE_X, x_count);
    HPS_spi_uio_cmd8(UIO_MOUSE_Y, y_count);
    HPS_spi_uio_cmd8(UIO_MOUSE_BTN, ((CONTROLLER_JOY_REG(4)>>16) & 0x7));
    mouse_counter = (CONTROLLER_KEY_REG(4) & 0x0000FFFF);


  } else if (((CONTROLLER_KEY_REG(1)>>28) == 0x3)){
    // // the normal joypad via the dock still needs work
    signed short xtemp = (signed short)(CONTROLLER_JOY_REG(1) & 0x000000FF);
    signed short ytemp = (signed short)((CONTROLLER_JOY_REG(1)>>8) & 0x000000FF);
    signed short x = (signed short)(CONTROLLER_JOY_REG(1) & 0x000000FF);
    signed short y = (signed short)((CONTROLLER_JOY_REG(1)>>8) & 0x000000FF);

    uint32_t x_left = 0x80 - x;
    uint32_t x_right = x - 0x80;
    x_left = x_left * speed;
    x_right = x_right * speed;
    x_left = x_left >> 6;
    x_right = x_right >> 6;
    uint32_t y_up = 0x80 - y;
    uint32_t y_down = y - 0x80;
    y_up = y_up * speed;
    y_down = y_down * speed;
    y_up = y_up >> 6;
    y_down = y_down >> 6;
    // mainprintf ("Joystick start 1 X %0.4x %0.4x %0.4x Y %0.4x \r\n", x, x_left, x_right, y);

    // mainprintf ("Joystick Access 1 X %0.4x Y %0.4x\r\n", x, y);

    x_count = ((x < 0x70) ? x_count + x_right : (x > 0x90) ? x_count - x_left : x_count);
    y_count = ((y < 0x70) ? y_count + y_down :  (y > 0x90) ? y_count - y_up : y_count);
    mainprintf ("Joystick Access count X %0.4x Y %0.4x\r\n", x_count, y_count);
    HPS_spi_uio_cmd8(UIO_MOUSE_X, x_count);
    HPS_spi_uio_cmd8(UIO_MOUSE_Y, y_count);
    HPS_spi_uio_cmd8(UIO_MOUSE_BTN, ((CONTROLLER_KEY_REG(1)>>8) & 0x3));


  } else {
    uint32_t c1_raw = CONTROLLER_KEY_REG(1);
    bool r_held = ((c1_raw & (1 << 9)) != 0);
    // Control Mode: 0 = Standard, 1 = Platformer, 2 = Driving, 3 = Mouse
    // interact.json writes Control Mode to 0xf0000018 (AFP_REGISTOR(6))
    uint8_t control_mode = (AFP_REGISTOR(6) & 0x3);
    bool is_mouse_mode = (control_mode == 3);
    bool active_mouse = mouse_enabled || r_held || is_mouse_mode;

    if (active_mouse) {
      uint32_t tmp = CORE_OUTPUT_REGISTOR();
      tmp = tmp | 0x00000030; // Suppress joystick in FPGA
      CORE_OUTPUT_REGISTOR() = tmp;

      if ((c1_raw & 0x1) == 1)       y_count = y_count - speed;
      if (((c1_raw >> 1) & 0x1) == 1) y_count = y_count + speed;
      if (((c1_raw >> 2) & 0x1) == 1) x_count = x_count - speed;
      if (((c1_raw >> 3) & 0x1) == 1) x_count = x_count + speed;

      // Mouse buttons:
      // Face A (bit 4) = Left click (bit 0)
      // Face B (bit 5) = Right click (bit 1)
      // Face X (bit 6) = Middle click (bit 2)
      uint8_t m_btn = 0;
      if (c1_raw & (1 << 4)) m_btn |= (1 << 0);
      if (c1_raw & (1 << 5)) m_btn |= (1 << 1);
      if (c1_raw & (1 << 6)) m_btn |= (1 << 2);

      HPS_spi_uio_cmd8(UIO_MOUSE_X, x_count);
      HPS_spi_uio_cmd8(UIO_MOUSE_Y, y_count);
      HPS_spi_uio_cmd8(UIO_MOUSE_BTN, m_btn);
    } else {
      HPS_spi_uio_cmd8(UIO_MOUSE_BTN, 0);
      uint32_t tmp = CORE_OUTPUT_REGISTOR();
      tmp = tmp & 0xFFFFFFCF;
      CORE_OUTPUT_REGISTOR() = tmp;
    }
  }
  return;
};

void update_keyboard_inputs (){
  // printf("%0.4x %0.4x %0.4x \r\n", CONTROLLER_KEY_REG(3), CONTROLLER_TRIG_REG(3), CONTROLLER_JOY_REG(3));
  uint8_t i = 0;
  uint8_t j = 0;
  uint8_t old_buf_len = 6;
  uint8_t tmp;

  uint8_t keyboard_buffer_updated[6]={0};
  uint8_t keyboard_buffer_input[6]={0};
  bool failed_key;

  for (i = 0; i < old_buf_len; i++) {
    keyboard_buffer_updated[i] = 0;
    keyboard_buffer_input[i] = 0;
  };
  // For we check for pressed down and currently pressed keys
  do {
  keyboard_buffer_input[0] = ((CONTROLLER_JOY_REG(3)>>24) & 0xff);
  keyboard_buffer_input[1] = ((CONTROLLER_JOY_REG(3)>>16) & 0xff);
  keyboard_buffer_input[2] = ((CONTROLLER_JOY_REG(3)>>8) & 0xff);
  keyboard_buffer_input[3] = (CONTROLLER_JOY_REG(3) & 0xff);
  keyboard_buffer_input[4] = ((CONTROLLER_TRIG_REG(3)>>8) & 0xff);
  keyboard_buffer_input[5] = (CONTROLLER_TRIG_REG(3) & 0xff);
    // check if a key is new to the buffer or current
    i = 0;
    do {
      failed_key = true;

      if ((keyboard_buffer_input[j] == keyboard_buffer[i]) && (keyboard_buffer_input[j] != 0)) {
        failed_key = false;
        break;
      } else if (keyboard_buffer_input[j] == 0){
        failed_key = false;
        break;
      }
      i++;
    } while (i < old_buf_len);
    keyboard_buffer_updated[j] = keyboard_buffer_input[j];  // Store the Pressed keys and the current keys down still to a temp buffer for now.
    if (failed_key) {
      tmp = mapHidToAmiga[keyboard_buffer_input[j]]; // Convert to the Amiga standard
      HPS_spi_uio_cmd8(UIO_KEYBOARD, tmp); // we send the new keys to the HPS bus
    }

    j++;
  } while (j < old_buf_len);

  // Now we test what keys have been un-pressed
  j = 0;
  do {
    i = 0;
    do {
      failed_key = true;
      // check if a key is removed
      if ((keyboard_buffer[j] == keyboard_buffer_updated[i]) && (keyboard_buffer[j] != 0)) {
        failed_key = false;
        break;
      } else if (keyboard_buffer[j] == 0){
        failed_key = false;
        break;
      }
      i++;
    } while (i < old_buf_len);
    if (failed_key) {
      tmp = mapHidToAmiga[keyboard_buffer[j]]; // To unpress we send the MSB to 1 and the same code back to say unpressed
      tmp = 0x80 | tmp;
      HPS_spi_uio_cmd8(UIO_KEYBOARD, tmp);
    }

    j++;
  } while (j < old_buf_len);
  for (i = 0; i < old_buf_len; i++) keyboard_buffer[i] = keyboard_buffer_updated[i];
  // Now for the controll keys
  bool    MOD_LALT_TMP    = ((CONTROLLER_KEY_REG(3)>>10) & 0x01);
  bool    MOD_RALT_TMP    = ((CONTROLLER_KEY_REG(3)>>14) & 0x01);
  bool    MOD_LSHIFT_TMP  = ((CONTROLLER_KEY_REG(3)>>9) & 0x01);
  bool    MOD_RSHIFT_TMP  = ((CONTROLLER_KEY_REG(3)>>13) & 0x01);
  bool    MOD_LWIN_TMP    = ((CONTROLLER_KEY_REG(3)>>11) & 0x01);
  bool    MOD_LCTRL_TMP   = ((CONTROLLER_KEY_REG(3)>>8)  & 0x01);
  bool    MOD_RCTRL_TMP   = ((CONTROLLER_KEY_REG(3)>>12) & 0x01);
  // Left Alt Key
  if (MOD_LALT_TMP && !MOD_LALT) {
    MOD_LALT = 1;
    HPS_spi_uio_cmd8(UIO_KEYBOARD, AMIGA_LALT);
  } else if (!MOD_LALT_TMP && MOD_LALT){
    MOD_LALT = 0;
    HPS_spi_uio_cmd8(UIO_KEYBOARD, (AMIGA_LALT| 0x80));
  }
// Right  Alt Key
  if (MOD_RALT_TMP && !MOD_RALT) {
    MOD_RALT = 1;
    HPS_spi_uio_cmd8(UIO_KEYBOARD, AMIGA_RSHIFT);
  } else if (!MOD_RALT_TMP && MOD_RALT){
    MOD_RALT = 0;
    HPS_spi_uio_cmd8(UIO_KEYBOARD, (AMIGA_RSHIFT| 0x80));
  }
  // Left Shift Key
  if (MOD_LSHIFT_TMP && !MOD_LSHIFT) {
    MOD_LSHIFT = 1;
    HPS_spi_uio_cmd8(UIO_KEYBOARD, AMIGA_LSHIFT);
  } else if (!MOD_LSHIFT_TMP && MOD_LSHIFT){
    MOD_LSHIFT = 0;
    HPS_spi_uio_cmd8(UIO_KEYBOARD, (AMIGA_LSHIFT| 0x80));
  }
  // Right Shift Key
  if (MOD_RSHIFT_TMP && !MOD_RSHIFT) {
    MOD_RSHIFT = 1;
    HPS_spi_uio_cmd8(UIO_KEYBOARD, AMIGA_RSHIFT);
  } else if (!MOD_RSHIFT_TMP && MOD_RSHIFT){
    MOD_RSHIFT = 0;
    HPS_spi_uio_cmd8(UIO_KEYBOARD, (AMIGA_RSHIFT| 0x80));
  }
  // Left Winkey Key
  if (MOD_LWIN_TMP && !MOD_LWIN) {
    MOD_LWIN = 1;
    HPS_spi_uio_cmd8(UIO_KEYBOARD, AMIGA_LAMIGA);
  } else if (!MOD_LWIN_TMP && MOD_LWIN){
    MOD_LWIN = 0;
    HPS_spi_uio_cmd8(UIO_KEYBOARD, (AMIGA_LAMIGA| 0x80));
  }
  // Control Key, But this does both of them on the keybopard
  if ((MOD_LCTRL_TMP | MOD_RCTRL_TMP) && !MOD_CTRL) {
    MOD_CTRL = 1;
    HPS_spi_uio_cmd8(UIO_KEYBOARD, AMIGA_CTRL);
  } else if (!MOD_LWIN_TMP && MOD_CTRL){
    MOD_CTRL = 0;
    HPS_spi_uio_cmd8(UIO_KEYBOARD, (AMIGA_CTRL| 0x80));
  }
  return;
}

// THis is the placeholder for the IO updates and makes sure we are polling every 50 milliseconds
void input_update() {
    timer1Interrupts_reset();
    if (((CONTROLLER_KEY_REG(3)>>28) == 0x4)) update_keyboard_inputs(); // We want to update the keyboard 6 time every
    if ((((AFP_REGISTOR(5)>>24)) == 0x80) | ((AFP_REGISTOR(5)>>24) == 0xa0)) {
      update_mouse_inputs();
    }
    bool is_cd32 = (((AFP_REGISTOR(5) >> 28) & 0x2) != 0);
    uint32_t c1 = CONTROLLER_KEY_REG(1);
    // Shortcut for disk swap: Hold L (bit 8) and press Select (bit 14) while virtual keyboard is not showing
    bool mod_l = (c1 & (1 << 8)) != 0;
    bool btn_select = (c1 & (1 << 14)) != 0;
    bool disk_swap_press = mod_l && btn_select && !keyboard_osd_showing;
    if (disk_swap_press && !disk_swap_press_old) {
      minimig_fdd_swap_disks();
    }
    disk_swap_press_old = disk_swap_press;

    // Next disk: hold L and press Start. Also in Driving mode, where L is the brake.
    // Opening the file needs host commands and waits, so the main loop does it.
    bool btn_start = (c1 & (1 << 15)) != 0;
    bool next_disk_press = mod_l && btn_start && !keyboard_osd_showing;
    if (next_disk_press && !next_disk_press_old) {
      fdd_next_disk_request = 1;
    }
    next_disk_press_old = next_disk_press;

    uint8_t control_mode = (AFP_REGISTOR(6) & 0x3);
    bool is_driving = (control_mode == 2);
    pocket_amiga_shortcuts_update(c1, is_cd32, keyboard_osd_showing, is_driving);
    if (pocket_amiga_shortcuts_suppress_joystick() || (mod_l && !is_driving)) {
      CORE_OUTPUT_REGISTOR() |= 0x00000010;
    } else if (!mouse_enabled && !keyboard_osd_showing && (control_mode != 3)) {
      CORE_OUTPUT_REGISTOR() &= ~0x00000010;
    }
    return;
  }

void input_setup() {
    timer1Interrupts_setup(2);
  }

void input_reg_update(){
  // Get the JOY setup sorted
  HPS_spi_uio_cmd8(UIO_MM2_JOY , (AFP_REGISTOR(5)>>28));
}
