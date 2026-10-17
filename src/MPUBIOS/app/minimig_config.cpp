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



 #include "minimig_config.h"
 #include "disk_name.h"
 #include "osd_popup.h"

// Floppy config (drives and speed) last sent to the core
static uint32_t floppy_config_sent;


void minimig_bios (){
  int tmp;
  tmp = dataslot_size(210);
  if (tmp == 0x100000) {
    mainprintf("\r\n1M Rom\r\n");
    dataslot_read(210, 0x90F00000, 0, 0x100000);

  }
  else if (tmp == 0x80000){
    mainprintf("\r\n512M Rom\r\n");
    dataslot_read(210, 0x90F00000, 0, 0x80000);
    dataslot_read(210, 0x90F80000, 0, 0x80000);

  }
  else if (tmp ==  0x40000){
    mainprintf("\r\n256M Rom\r\n");
    dataslot_read(210, 0x90f00000, 0, 0x40000);
    dataslot_read(210, 0x90f40000, 0, 0x40000);
    dataslot_read(210, 0x90f80000, 0, 0x40000);
    dataslot_read(210, 0x90fc0000, 0, 0x40000);
  }
  mainprintf("DONE BIOS LOAD\r\n");
};

void minimig_restart_first() {
  // Bios size
  
		
  minimig_bios();

  // Get the Floppy Controllers sorted
  floppy_config_sent = AFP_REGISTOR(1);
  HPS_spi_uio_cmd8(UIO_MM2_FLP , floppy_config_sent);
  // Get the chip setup sorted
  HPS_spi_uio_cmd8(UIO_MM2_CHIP, AFP_REGISTOR(2));
  // Get the Cpu setup sorted
  HPS_spi_uio_cmd8(UIO_MM2_CPU , AFP_REGISTOR(3));
  // Get the MEM setup sorted
  HPS_spi_uio_cmd8(UIO_MM2_MEM , AFP_REGISTOR(4));
  // Get the JOY setup sorted
  HPS_spi_uio_cmd8(UIO_MM2_JOY , AFP_REGISTOR(5));
  // Update the Floppy controller
  minimig_fdd_update();
  minimig_hdd_update();
  mainprintf("DONE Core Setup\r\n");
};

void minimig_restart_running_core() {
  // Get the Floppy Controllers sorted
  floppy_config_sent = AFP_REGISTOR(1);
  HPS_spi_uio_cmd8(UIO_MM2_FLP , floppy_config_sent);
  // Get the chip setup sorted
  HPS_spi_uio_cmd8(UIO_MM2_CHIP, AFP_REGISTOR(2));
  // Get the Cpu setup sorted
  HPS_spi_uio_cmd8(UIO_MM2_CPU , AFP_REGISTOR(3));
  // Get the MEM setup sorted
  HPS_spi_uio_cmd8(UIO_MM2_MEM , AFP_REGISTOR(4));
  // Get the JOY setup sorted
  HPS_spi_uio_cmd8(UIO_MM2_JOY , AFP_REGISTOR(5));
  mainprintf("DONE Core Setup\r\n");
};

// The floppy speed (bit 0) takes effect as soon as the core gets the config, so
// send it when the menu changes it. The drive count (bits 3:2) is only taken by
// Paula on reset, as before.
void minimig_floppy_config_update() {
  uint32_t config = AFP_REGISTOR(1);
  if (config != floppy_config_sent) {
    floppy_config_sent = config;
    HPS_spi_uio_cmd8(UIO_MM2_FLP , config);
    mainprintf("Floppy config %0.2x\r\n", config);
  }
}

// Heuristic bootblock profile detection for Amiga games
static bool buffer_has_substr(const uint8_t *buf, int len, const char *sub) {
  int sub_len = strlen(sub);
  if (sub_len > len) return false;
  for (int i = 0; i <= len - sub_len; i++) {
    bool match = true;
    for (int j = 0; j < sub_len; j++) {
      char c1 = (char)buf[i + j];
      char c2 = sub[j];
      // Case-insensitive ASCII comparison
      if (c1 >= 'a' && c1 <= 'z') c1 -= 32;
      if (c2 >= 'a' && c2 <= 'z') c2 -= 32;
      if (c1 != c2) {
        match = false;
        break;
      }
    }
    if (match) return true;
  }
  return false;
}

void minimig_detect_game_profile() {
  if (!(df[0].status & DSK_INSERTED)) return;

  // Read Track 0 (first 5632 bytes, containing bootblock sectors 0 and 1)
  uint32_t foo1 = (uint32_t)&sector_bufferfdd1;
  while (!dataslot_ready()) {}
  if (dataslot_read(df[0].dataslot, foo1 | APF_ADDRESS_OFFSET, 0, 5632)) {
    mainprintf("Profile detection: failed to read Track 0\r\n");
    return;
  }

  const uint8_t *bb = sector_bufferfdd1;
  int scan_len = 2048; // Scan first 2KB for signatures

  // 1. Platformer profile detection (games benefiting from B Jump + X Special)
  if (buffer_has_substr(bb, scan_len, "TURRICAN") ||
      buffer_has_substr(bb, scan_len, "FACTOR 5") ||
      buffer_has_substr(bb, scan_len, "GIANA") ||
      buffer_has_substr(bb, scan_len, "RUFF") ||
      buffer_has_substr(bb, scan_len, "LIONHEART") ||
      buffer_has_substr(bb, scan_len, "SUPERFROG") ||
      buffer_has_substr(bb, scan_len, "SONIC") ||
      buffer_has_substr(bb, scan_len, "MARIO") ||
      buffer_has_substr(bb, scan_len, "MAGIC POCKETS") ||
      buffer_has_substr(bb, scan_len, "GODS") ||
      buffer_has_substr(bb, scan_len, "ZOOL") ||
      buffer_has_substr(bb, scan_len, "JAMES POND") ||
      buffer_has_substr(bb, scan_len, "CHUCK ROCK")) {
    mainprintf("Auto-detected profile: PLATFORMER (Jump=B, Special=X)\r\n");
    // Control mode: 1 = Platformer
    // We notify via mainprintf and user can see in logs
    return;
  }

  // 2. Driving profile detection (games with gas on Up, brake on Down)
  if (buffer_has_substr(bb, scan_len, "LOTUS") ||
      buffer_has_substr(bb, scan_len, "MAGNETIC FIELDS") ||
      buffer_has_substr(bb, scan_len, "SUPER CARS") ||
      buffer_has_substr(bb, scan_len, "JAGUAR") ||
      buffer_has_substr(bb, scan_len, "CRAZY CARS") ||
      buffer_has_substr(bb, scan_len, "NITRO") ||
      buffer_has_substr(bb, scan_len, "OVERDRIVE") ||
      buffer_has_substr(bb, scan_len, "ROADKILL")) {
    mainprintf("Auto-detected profile: DRIVING (Gas=R, Brake=L)\r\n");
    return;
  }

  // 3. Mouse profile detection (point & click, Workbench, managers)
  if (buffer_has_substr(bb, scan_len, "LEMMINGS") ||
      buffer_has_substr(bb, scan_len, "SETTLERS") ||
      buffer_has_substr(bb, scan_len, "MONKEY ISLAND") ||
      buffer_has_substr(bb, scan_len, "CANNON FODDER") ||
      buffer_has_substr(bb, scan_len, "SENSIBLE") ||
      buffer_has_substr(bb, scan_len, "POPULOUS") ||
      buffer_has_substr(bb, scan_len, "SYNDICATE") ||
      buffer_has_substr(bb, scan_len, "WORKBENCH")) {
    mainprintf("Auto-detected profile: MOUSE (D-pad Cursor, A/B Clicks)\r\n");
    return;
  }

  mainprintf("Auto-detected profile: STANDARD JOYSTICK\r\n");
}

// File names of the floppy data slots for the status line
#define FDD_NAME_UNKNOWN 0
#define FDD_NAME_KNOWN   1
#define FDD_NAME_NONE    2
#define FDD_STATUS_NAME_WIDTH 23 // "DF0 T79 " + name in a 31 character line
static char    fdd_slot_name[4][FDD_STATUS_NAME_WIDTH + 1];
static uint8_t fdd_slot_name_state[4];
static char    fdd_path[DATASLOT_PATH_LEN];
static char    fdd_next_path[DATASLOT_PATH_LEN];

static void fdd_set_slot_name(uint32_t dataslot, const char *path) {
  uint32_t i = dataslot - DATASLOT_FDD_BASE;
  if (i > 3) return;
  disk_name_fit(path, fdd_slot_name[i], FDD_STATUS_NAME_WIDTH);
  fdd_slot_name_state[i] = FDD_NAME_KNOWN;
}

// Set by main() when the main loop starts. While the core is running a disk
// change must be visible to trackdisk.device, which polls about once a second:
// like WinUAE (2 s) the drive reports "no disk" for 2.5 s before the new disk.
bool minimig_running;

void minimig_fdd_update(){
  bool df0_was_empty = (df[0].status == 0);
  for (int n = 0; n < 4; n++) fdd_slot_name_state[n] = FDD_NAME_UNKNOWN;
  int i = 0;
  while (i != 4)
  {
    uint32_t size = dataslot_size(i + DATASLOT_FDD_BASE);
    if (size != 0){
      UnsertFloppy(&df[i]);
      riscusleep(minimig_running ? 2500000 : 400000);
      InsertFloppy(&df[i], size, i + DATASLOT_FDD_BASE);
    }
    i++;
  }
  UpdateDriveStatus_fdd();

  // If DF0 was empty and is now loaded, detect profile.
  // The Amiga hardware natively senses the disk insertion via DSKCHANGE in UpdateDriveStatus_fdd().
  if (df0_was_empty && (df[0].status & DSK_INSERTED)) {
    mainprintf("DF0 loaded into empty drive - inspecting bootblock\r\n");
    minimig_detect_game_profile();
    mainprintf("Disk ready in DF0: - DSKCHANGE signaled to Amiga OS\r\n");
  }
}

// A disk given at launch (Recents) or by a data slot update that was folded into
// another one (the host reports only the last updated slot) is not in the drive:
// look for slots that have a file but an empty drive, twice a second, and insert
// those only. Drives that already hold a disk are left alone.
static volatile uint32_t df0_track_reads;

void minimig_fdd_watch(){
  static uint32_t next_check;
  uint32_t now = GetMilliseconds();
  if ((int)(now - next_check) < 0) return;
  next_check = now + 500;

  // A disk given at launch (instance JSON / Recents) arrives after the Amiga is
  // running and does not boot by itself, because trackdisk never saw the drive
  // empty->full change. Do what WinUAE does: report "no disk" for 2.5 s, then
  // present the disk. Non-blocking: a pending insert is completed here.
  static uint32_t present_at;
  static uint32_t df0_since;      // when DF0 was first seen holding a disk
  static bool fallback_done;
  static uint32_t first_call;
  if (!first_call) first_call = now ? now : 1;
  uint32_t up = now - first_call;  // ms since the main loop started

  if (present_at) {
    if ((int)(now - present_at) >= 0) {
      present_at = 0;
      UpdateDriveStatus_fdd();
      mainprintf("Floppy watch: disk presented\r\n");
      df0_since = now ? now : 1;
    }
    return;
  }

  // First disk after power-up (launch from the pocket menu / Recents / a disk
  // attached right after start): if the Amiga has still not read a single track
  // from DF0 10 s after the disk was there, it is stuck at the insert-disk
  // screen. Reset once; the reset presents the disk after a clean 3 s "no disk"
  // phase, which is known to boot it. Only in the first 2 minutes, only once.
  if (df[0].status & DSK_INSERTED) {
    if (!df0_since) df0_since = now ? now : 1;
    if (!fallback_done && up < 120000 && df0_track_reads == 0 && (int)(now - (df0_since + 10000)) >= 0) {
      fallback_done = true;
      AFP_REGISTOR(0) = 1;
      mainprintf("Floppy watch: disk not read, resetting the Amiga\r\n");
    }
  } else {
    df0_since = 0;
  }

  bool inserted = false;
  for (int i = 0; i < 2; i++) {
    if (df[i].status & DSK_INSERTED) continue;
    uint32_t size = dataslot_size(i + DATASLOT_FDD_BASE);
    if (size == 0 || (size % (512 * 11)) != 0 || size > (MAX_TRACKS * 512 * 11 * 2)) continue;
    InsertFloppy(&df[i], size, i + DATASLOT_FDD_BASE);
    mainprintf("Floppy watch: inserted DF%d (%d bytes)\r\n", i, size);
    inserted = true;
  }
  if (inserted) {
    // the drive keeps reporting "no disk" until present_at
    RemoveDriveStatus();
    present_at = now + 2500;
    if (!present_at) present_at = 1;
    if (df[0].status & DSK_INSERTED) minimig_detect_game_profile();
  }
}

// Swap the disks in DF0 and DF1 (the core has two floppy drives)
void minimig_fdd_swap_disks(){
  if (!(df[0].status & DSK_INSERTED) && !(df[1].status & DSK_INSERTED)) {
    mainprintf("No disks loaded to swap\r\n");
    return;
  }

  mainprintf("Swapping disks DF0 <-> DF1\r\n");
  // Temporarily signal disk ejection on all drives so Amiga trackdisk detects the disk change
  RemoveDriveStatus();
  riscusleep(400000);

  adfTYPE temp = df[0];
  df[0] = df[1];
  df[1] = temp;

  for (int d = 0; d < 2; d++) {
    df[d].track_prev = -1;
    df[d].sector_offset = 0;
  }

  UpdateDriveStatus_fdd();
  mainprintf("Disk swap complete! New DF0 dataslot: %d\r\n", df[0].dataslot);
}

static bool fdd_same_path(const char *a, const char *b) {
  while (*a && *a == *b) {
    a++;
    b++;
  }
  return (*a == *b);
}

static void fdd_message(const char *text, const char *path) {
  char msg[32];
  int o = disk_name_append(msg, 0, sizeof(msg), text);
  if (path) disk_name_fit(path, msg + o, (int)sizeof(msg) - 1 - o);
  osd_popup_show(msg, 3000, OSD_POPUP_MESSAGE);
}

// Next disk (L + Start): open the next file of a multi-disk set into the data slot
// of DF0 ("Game (Disk 1 of 3).adf" -> "Game (Disk 2 of 3).adf"), back to disk 1
// after the last one, and change the disk in DF0. Runs in the main loop.
void minimig_fdd_next_disk(){
  if (!(df[0].status & DSK_INSERTED)) {
    fdd_message("DF0: no disk inserted", 0);
    return;
  }
  uint32_t dataslot = df[0].dataslot; // L + Select may have rotated another slot into DF0
  uint32_t old_size = df[0].size;

  uint32_t result = dataslot_get_filename(dataslot, fdd_path);
  if (result || !fdd_path[0]) {
    fdd_message("DF0: no file name from host", 0);
    return;
  }
  if (!disk_name_step(fdd_path, fdd_next_path, sizeof(fdd_next_path), 0)) {
    fdd_message("DF0: no disk number in name", 0);
    return;
  }
  result = dataslot_open_file(dataslot, fdd_next_path);
  if (result == DATASLOT_OPEN_NOT_FOUND) {
    // Past the last disk of the set: back to disk 1
    if (disk_name_step(fdd_path, fdd_next_path, sizeof(fdd_next_path), 1) && !fdd_same_path(fdd_next_path, fdd_path)) {
      result = dataslot_open_file(dataslot, fdd_next_path);
    }
  }
  if (result != DATASLOT_OPEN_OK && result != DATASLOT_OPEN_CREATED) {
    // Keep the disk that is in the drive
    uint32_t reopen = dataslot_open_file(dataslot, fdd_path);
    if (reopen != DATASLOT_OPEN_OK) fdd_message("DF0: disk change failed", 0);
    else if (result == DATASLOT_OPEN_NOT_FOUND) fdd_message("DF0: no next disk found", 0);
    else {
      char msg[32];
      int o = disk_name_append(msg, 0, sizeof(msg), "DF0: open failed, error ");
      disk_name_append_uint(msg, o, sizeof(msg), result, 1);
      osd_popup_show(msg, 3000, OSD_POPUP_MESSAGE);
    }
    return;
  }

  // Eject so Paula raises DSKCHG, give the Amiga time to see the empty drive,
  // then insert the new file. The host may not update the size in the data slot
  // table for 0x0192, so a size that is not a whole number of tracks keeps the old one.
  UnsertFloppy(&df[0]);
  WaitMilliseconds(400);
  uint32_t size = dataslot_size(dataslot);
  if (size == 0 || (size % (512 * 11)) != 0 || size > (MAX_TRACKS * 512 * 11 * 2)) size = old_size;
  InsertFloppy(&df[0], size, dataslot);
  UpdateDriveStatus_fdd();

  fdd_set_slot_name(dataslot, fdd_next_path);
  fdd_message("DF0: ", fdd_next_path);
  mainprintf("Next disk in DF0: %s (%d bytes)\r\n", fdd_next_path, size);
}

// Floppy status line ("DF0 T40 Game (Disk 1 of 3)") while the drives are read
// or written, when Drive Indicator is set to Status (menu reg 7 bit 8)
static bool    fdd_status_pending;
static uint8_t fdd_status_drive;
static uint8_t fdd_status_track;

void minimig_fdd_status_note(uint8_t drive, uint8_t track){
  if ((drive & 0x3) == 0) df0_track_reads++;
  fdd_status_drive = drive & 0x3;
  fdd_status_track = track;
  fdd_status_pending = 1;
}

void minimig_fdd_status_update(){
  if (!fdd_status_pending) return;
  fdd_status_pending = 0;
  if (!(AFP_REGISTOR(7) & FDD_STATUS_ENABLE)) return;

  adfTYPE *drive = &df[fdd_status_drive];
  char msg[32];
  int o = disk_name_append(msg, 0, sizeof(msg), "DF");
  o = disk_name_append_uint(msg, o, sizeof(msg), fdd_status_drive, 1);
  o = disk_name_append(msg, o, sizeof(msg), " T");
  // The core counts tracks per side (cylinder * 2 + head); show the cylinder
  o = disk_name_append_uint(msg, o, sizeof(msg), fdd_status_track >> 1, 2);
  uint32_t i = drive->dataslot - DATASLOT_FDD_BASE;
  if ((drive->status & DSK_INSERTED) && i <= 3) {
    if (fdd_slot_name_state[i] == FDD_NAME_UNKNOWN) {
      if (dataslot_get_filename(drive->dataslot, fdd_path) == 0 && fdd_path[0]) fdd_set_slot_name(drive->dataslot, fdd_path);
      else fdd_slot_name_state[i] = FDD_NAME_NONE;
    }
    if (fdd_slot_name_state[i] == FDD_NAME_KNOWN) {
      o = disk_name_append(msg, o, sizeof(msg), " ");
      disk_name_append(msg, o, sizeof(msg), fdd_slot_name[i]);
    }
  }
  osd_popup_show(msg, 1000, OSD_POPUP_STATUS);
}

void minimig_hdd_update(){
  mainprintf("Harddrive are being setup in the core\r\n");
    int temp = 0;
    if (dataslot_size(310) != 0){
      ide_open(0,310);
      temp = temp | 0x23;
    }
		if (dataslot_size(311) != 0){
      ide_open(1,311);
      temp = temp | 0x29;

    }
		if (dataslot_size(312) != 0){
      ide_open(2,312);
      temp = temp | 0x25;

    }
		if (dataslot_size(313) != 0){
      ide_open(3,313);
      temp = temp | 0x31;

    }
    
  // Get the HDD setup sorted
  HPS_spi_uio_cmd8(UIO_MM2_HDD , temp);
  mainprintf("Harddrive setups %0.4x \r\n", temp);
}
