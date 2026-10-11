#include "pocket_shortcuts.h"
#include "inputs.h"

static PocketShortcuts s_shortcuts;
static bool s_initialized = false;

static void amiga_key_callback(uint8_t scancode, bool down) {
    if (down) {
        HPS_spi_uio_cmd8(UIO_KEYBOARD, scancode);
    } else {
        HPS_spi_uio_cmd8(UIO_KEYBOARD, scancode | 0x80);
    }
}

extern "C" void pocket_amiga_shortcuts_init(void) {
    s_shortcuts.set_callback(amiga_key_callback);
    s_initialized = true;
}

extern "C" void pocket_amiga_shortcuts_update(uint32_t c1, bool is_cd32_mode, bool osd_active) {
    if (!s_initialized) {
        pocket_amiga_shortcuts_init();
    }
    s_shortcuts.process(c1, is_cd32_mode, osd_active);
}

extern "C" bool pocket_amiga_shortcuts_suppress_joystick(void) {
    return s_shortcuts.suppress_joystick();
}
