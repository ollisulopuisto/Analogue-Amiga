#ifndef POCKET_SHORTCUTS_H
#define POCKET_SHORTCUTS_H

#include <stdint.h>
#include <stdbool.h>

#ifndef AMIGA_SPACE
#define AMIGA_ONE       0x01
#define AMIGA_TWO       0x02
#define AMIGA_Y         0x15
#define AMIGA_N         0x36
#define AMIGA_SPACE     0x40
#define AMIGA_RETURN    0x44
#define AMIGA_ESC       0x45
#define AMIGA_F1        0x50
#define AMIGA_F2        0x51
#define AMIGA_F3        0x52
#define AMIGA_F4        0x53
#define AMIGA_F5        0x54
#define AMIGA_F6        0x55
#define AMIGA_F7        0x56
#define AMIGA_F8        0x57
#endif

#ifdef __cplusplus
class PocketShortcuts {
public:
    typedef void (*KeyCallback)(void *userdata, uint8_t scancode, bool down);

    PocketShortcuts() : callback_(nullptr), userdata_(nullptr), suppress_joystick_(false) {
        for (int i = 0; i < MAX_KEYS; i++) {
            active_keys_[i] = 0;
        }
    }

    void set_callback(KeyCallback cb, void *userdata = nullptr) {
        callback_ = cb;
        userdata_ = userdata;
    }

    bool suppress_joystick() const {
        return suppress_joystick_;
    }

    void process(uint32_t c1, bool is_cd32_mode, bool osd_active, bool is_driving = false) {
        if (osd_active) {
            release_all();
            suppress_joystick_ = false;
            return;
        }

        bool up    = (c1 & (1 << 0)) != 0;
        bool down  = (c1 & (1 << 1)) != 0;
        bool left  = (c1 & (1 << 2)) != 0;
        bool right = (c1 & (1 << 3)) != 0;
        bool btn_a = (c1 & (1 << 4)) != 0;
        bool btn_b = (c1 & (1 << 5)) != 0;
        bool btn_x = (c1 & (1 << 6)) != 0;
        bool btn_y = (c1 & (1 << 7)) != 0;
        bool mod_l = (c1 & (1 << 8)) != 0;
        bool mod_r = (c1 & (1 << 9)) != 0;
        (void)is_cd32_mode;  // face buttons type nothing unshifted in either pad mode

        uint8_t desired[MAX_KEYS];
        int count = 0;

        // In Driving mode, L is the brake and R is gas, so L should not activate the shortcut layer or suppress joystick
        if (mod_l && !is_driving) {
            suppress_joystick_ = true;
            if (mod_r) {
                // Layer L + R
                if (up)    desired[count++] = AMIGA_F5;
                if (right) desired[count++] = AMIGA_F6;
                if (down)  desired[count++] = AMIGA_F7;
                if (left)  desired[count++] = AMIGA_F8;
                if (btn_a) desired[count++] = AMIGA_SPACE;
                if (btn_b) desired[count++] = AMIGA_N;
                if (btn_x) desired[count++] = AMIGA_Y;
                if (btn_y) desired[count++] = AMIGA_TWO;
            } else {
                // Layer L alone
                if (up)    desired[count++] = AMIGA_F1;
                if (right) desired[count++] = AMIGA_F2;
                if (down)  desired[count++] = AMIGA_F3;
                if (left)  desired[count++] = AMIGA_F4;
                if (btn_a) desired[count++] = AMIGA_SPACE;
                if (btn_b) desired[count++] = AMIGA_RETURN;
                if (btn_x) desired[count++] = AMIGA_ESC;
                if (btn_y) desired[count++] = AMIGA_ONE;
            }
        } else {
            suppress_joystick_ = false;
            // No modifier held: face buttons are joystick only (A, X, Y fire,
            // B jumps), never keys, so they can be mashed in a game.
        }

        // Release any keys that are active but not in desired
        for (int i = 0; i < MAX_KEYS; i++) {
            if (active_keys_[i] != 0) {
                bool found = false;
                for (int j = 0; j < count; j++) {
                    if (desired[j] == active_keys_[i]) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    if (callback_) callback_(userdata_, active_keys_[i], false);
                    active_keys_[i] = 0;
                }
            }
        }

        // Press any keys in desired that are not yet active
        for (int j = 0; j < count; j++) {
            bool found = false;
            for (int i = 0; i < MAX_KEYS; i++) {
                if (active_keys_[i] == desired[j]) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                // Add to active_keys_
                for (int i = 0; i < MAX_KEYS; i++) {
                    if (active_keys_[i] == 0) {
                        active_keys_[i] = desired[j];
                        if (callback_) callback_(userdata_, desired[j], true);
                        break;
                    }
                }
            }
        }
    }

    void release_all() {
        for (int i = 0; i < MAX_KEYS; i++) {
            if (active_keys_[i] != 0) {
                if (callback_) callback_(userdata_, active_keys_[i], false);
                active_keys_[i] = 0;
            }
        }
    }

private:
    static const int MAX_KEYS = 8;
    uint8_t active_keys_[MAX_KEYS];
    KeyCallback callback_;
    void *userdata_;
    bool suppress_joystick_;
};

extern "C" {
#endif

void pocket_amiga_shortcuts_init(void);
void pocket_amiga_shortcuts_update(uint32_t c1, bool is_cd32_mode, bool osd_active, bool is_driving);
bool pocket_amiga_shortcuts_suppress_joystick(void);

#ifdef __cplusplus
}
#endif

#endif // POCKET_SHORTCUTS_H
