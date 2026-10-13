#include <cassert>
#include <cstdio>
#include <vector>
#include <cstdint>
#include <cstring>

// Forward declaration of the shortcut logic to be implemented
#include "../src/MPUBIOS/drivers/KMIO/pocket_shortcuts.h"
#include "../src/MPUBIOS/drivers/floppy_drive/disk_name.h"

static void expect_step(const char *in, int to_first, const char *want) {
    char out[256];
    int ok = disk_name_step(in, out, sizeof(out), to_first);
    if (!want) {
        if (ok) printf("  unexpected step: %s -> %s\n", in, out);
        assert(!ok);
        return;
    }
    if (!ok || strcmp(out, want) != 0) printf("  step failed: %s -> %s (want %s)\n", in, ok ? out : "<none>", want);
    assert(ok && strcmp(out, want) == 0);
}

static void expect_fit(const char *in, int width, const char *want) {
    char out[64];
    disk_name_fit(in, out, width);
    if (strcmp(out, want) != 0) printf("  fit failed: %s -> '%s' (want '%s')\n", in, out, want);
    assert(strcmp(out, want) == 0);
    assert((int)strlen(out) <= width);
}

static void test_disk_names() {
    printf("Running test: next disk file names...\n");
    expect_step("/Assets/amiga/common/Game (Disk 1 of 3).adf", 0, "/Assets/amiga/common/Game (Disk 2 of 3).adf");
    expect_step("/Assets/amiga/common/Game (Disk 2 of 3).adf", 0, "/Assets/amiga/common/Game (Disk 3 of 3).adf");
    expect_step("/Assets/amiga/common/Game (Disk 3 of 3).adf", 0, "/Assets/amiga/common/Game (Disk 1 of 3).adf");
    expect_step("Game (disk 9 of 12).adf", 0, "Game (disk 10 of 12).adf");
    expect_step("Game (Disk 12 of 12).adf", 0, "Game (Disk 1 of 12).adf");
    expect_step("Game Disk1.adf", 0, "Game Disk2.adf");
    expect_step("Game_Disk_1.adf", 0, "Game_Disk_2.adf");
    expect_step("Game-Disk-01.ADF", 0, "Game-Disk-02.ADF");
    expect_step("Game DISK#9.adf", 0, "Game DISK#10.adf");
    expect_step("Game-1.adf", 0, "Game-2.adf");
    expect_step("Game_1.adf", 0, "Game_2.adf");
    expect_step("Game 9.adf", 0, "Game 10.adf");
    expect_step("Game (Disk A).adf", 0, "Game (Disk B).adf");
    expect_step("Game (disk b).adf", 0, "Game (disk c).adf");
    expect_step("Game (Disk Z).adf", 0, "Game (Disk A).adf");
    // "disk" wins over a trailing number, and a word starting with "disk" is skipped
    expect_step("Diskworld (Disk 1 of 2) v2.adf", 0, "Diskworld (Disk 2 of 2) v2.adf");
    expect_step("Discworld 2 Disk 3 1995.adf", 0, "Discworld 2 Disk 4 1995.adf");
    expect_step("Diskworld 2.adf", 0, "Diskworld 3.adf");
    // Directories are never touched
    expect_step("/Assets/amiga/Disk 1/Game-1.adf", 0, "/Assets/amiga/Disk 1/Game-2.adf");
    // Back to the first disk
    expect_step("Game (Disk 3 of 3).adf", 1, "Game (Disk 1 of 3).adf");
    expect_step("Game Disk05.adf", 1, "Game Disk01.adf");
    expect_step("Game (Disk C).adf", 1, "Game (Disk A).adf");
    expect_step("Game-4.adf", 1, "Game-1.adf");
    // No disk index
    expect_step("Workbench.adf", 0, nullptr);
    expect_step("Game (1990).adf", 0, nullptr);
    expect_step("/Assets/amiga/Disk 2/Game.adf", 0, nullptr);
    expect_step("Diskette.adf", 0, nullptr);
    // Output buffer too small
    {
        char small[8];
        assert(!disk_name_step("Game Disk1.adf", small, sizeof(small), 0));
    }

    printf("Running test: OSD file name fitting...\n");
    expect_fit("/Assets/amiga/common/Game (Disk 2 of 3).adf", 26, "Game (Disk 2 of 3)");
    expect_fit("Workbench", 26, "Workbench");
    expect_fit("/a/Monkey Island 2 - LeChuck's Revenge (Disk 5 of 11).adf", 26, "Monkey Islan~Disk 5 of 11)");
    expect_fit("/a/0123456789.adf", 5, "01~89");
    {
        char out[32];
        int o = disk_name_append(out, 0, sizeof(out), "DF");
        o = disk_name_append_uint(out, o, sizeof(out), 0, 1);
        o = disk_name_append(out, o, sizeof(out), " T");
        o = disk_name_append_uint(out, o, sizeof(out), 7, 2);
        assert(strcmp(out, "DF0 T07") == 0 && o == 7);
        o = disk_name_append(out, 0, 4, "ABCDEFG");
        assert(strcmp(out, "ABC") == 0 && o == 3);
    }
}

int main() {
    PocketShortcuts shortcuts;
    std::vector<std::pair<uint8_t, bool>> events;

    auto callback = [](void *userdata, uint8_t code, bool down) {
        auto *ev = static_cast<std::vector<std::pair<uint8_t, bool>>*>(userdata);
        ev->push_back({code, down});
    };
    shortcuts.set_callback(callback, &events);

    printf("Running test: Unmodified face buttons -> joystick only, no keys...\n");
    for (int bit = 4; bit <= 7; bit++) {
        shortcuts.process(1u << bit, false, false);
        assert(events.empty());
        assert(!shortcuts.suppress_joystick());
        shortcuts.process(0, false, false);
        assert(events.empty());
    }
    shortcuts.process((1 << 4) | (1 << 5) | (1 << 6) | (1 << 7), false, false);
    assert(events.empty());
    shortcuts.process(0, false, false);
    assert(events.empty());

    printf("Running test: Modifier L (bit 8) combos...\n");
    // Hold L (bit 8)
    shortcuts.process(1 << 8, false, false);
    assert(events.empty());
    assert(shortcuts.suppress_joystick()); // joystick must be suppressed while L is held

    // Hold L + A -> Space
    shortcuts.process((1 << 8) | (1 << 4), false, false);
    assert(events.size() == 1);
    assert(events[0].first == AMIGA_SPACE && events[0].second == true);
    events.clear();

    // Release A while keeping L held
    shortcuts.process(1 << 8, false, false);
    assert(events.size() == 1);
    assert(events[0].first == AMIGA_SPACE && events[0].second == false);
    events.clear();

    // Hold L + B -> Return
    shortcuts.process((1 << 8) | (1 << 5), false, false);
    assert(events.size() == 1);
    assert(events[0].first == AMIGA_RETURN && events[0].second == true);
    events.clear();

    // Hold L + X -> Esc
    shortcuts.process((1 << 8) | (1 << 6), false, false);
    // B was released, X pressed
    assert(events.size() == 2);
    assert(events[0].first == AMIGA_RETURN && events[0].second == false);
    assert(events[1].first == AMIGA_ESC && events[1].second == true);
    events.clear();

    // Hold L + Y -> 1
    shortcuts.process((1 << 8) | (1 << 7), false, false);
    assert(events.size() == 2);
    assert(events[0].first == AMIGA_ESC && events[0].second == false);
    assert(events[1].first == AMIGA_ONE && events[1].second == true);
    events.clear();

    // Hold L + Dpad Up -> F1
    shortcuts.process((1 << 8) | (1 << 0), false, false);
    assert(events.size() == 2);
    assert(events[0].first == AMIGA_ONE && events[0].second == false);
    assert(events[1].first == AMIGA_F1 && events[1].second == true);
    events.clear();

    // Hold L + Dpad Right -> F2
    shortcuts.process((1 << 8) | (1 << 3), false, false);
    assert(events[0].first == AMIGA_F1 && events[0].second == false);
    assert(events[1].first == AMIGA_F2 && events[1].second == true);
    events.clear();

    // Hold L + Dpad Down -> F3
    shortcuts.process((1 << 8) | (1 << 1), false, false);
    assert(events[0].first == AMIGA_F2 && events[0].second == false);
    assert(events[1].first == AMIGA_F3 && events[1].second == true);
    events.clear();

    // Hold L + Dpad Left -> F4
    shortcuts.process((1 << 8) | (1 << 2), false, false);
    assert(events[0].first == AMIGA_F3 && events[0].second == false);
    assert(events[1].first == AMIGA_F4 && events[1].second == true);
    events.clear();

    printf("Running test: Modifier L + R combos (F5-F8, 2, Y/N)...\n");
    // Hold L (bit 8) + R (bit 9) + Dpad Up (bit 0) -> F5
    shortcuts.process((1 << 8) | (1 << 9) | (1 << 0), false, false);
    assert(events[0].first == AMIGA_F4 && events[0].second == false);
    assert(events[1].first == AMIGA_F5 && events[1].second == true);
    events.clear();

    // L + R + Dpad Right (bit 3) -> F6
    shortcuts.process((1 << 8) | (1 << 9) | (1 << 3), false, false);
    assert(events[0].first == AMIGA_F5 && events[0].second == false);
    assert(events[1].first == AMIGA_F6 && events[1].second == true);
    events.clear();

    // L + R + Dpad Down (bit 1) -> F7
    shortcuts.process((1 << 8) | (1 << 9) | (1 << 1), false, false);
    assert(events[0].first == AMIGA_F6 && events[0].second == false);
    assert(events[1].first == AMIGA_F7 && events[1].second == true);
    events.clear();

    // L + R + Dpad Left (bit 2) -> F8
    shortcuts.process((1 << 8) | (1 << 9) | (1 << 2), false, false);
    assert(events[0].first == AMIGA_F7 && events[0].second == false);
    assert(events[1].first == AMIGA_F8 && events[1].second == true);
    events.clear();

    // L + R + Y -> 2
    shortcuts.process((1 << 8) | (1 << 9) | (1 << 7), false, false);
    assert(events[0].first == AMIGA_F8 && events[0].second == false);
    assert(events[1].first == AMIGA_TWO && events[1].second == true);
    events.clear();

    // L + R + X -> 'Y'
    shortcuts.process((1 << 8) | (1 << 9) | (1 << 6), false, false);
    assert(events[0].first == AMIGA_TWO && events[0].second == false);
    assert(events[1].first == AMIGA_Y && events[1].second == true);
    events.clear();

    // L + R + B -> 'N'
    shortcuts.process((1 << 8) | (1 << 9) | (1 << 5), false, false);
    assert(events[0].first == AMIGA_Y && events[0].second == false);
    assert(events[1].first == AMIGA_N && events[1].second == true);
    events.clear();

    // Release all: verify everything is released
    shortcuts.process(0, false, false);
    assert(events.size() == 1);
    assert(events[0].first == AMIGA_N && events[0].second == false);
    assert(!shortcuts.suppress_joystick());
    events.clear();

    printf("Running test: Driving mode (is_driving = true) - holding L does not suppress joystick or trigger shortcuts...\n");
    // Hold L (bit 8) in driving mode (is_driving = true)
    shortcuts.process(1 << 8, false, false, true);
    assert(events.empty());
    assert(!shortcuts.suppress_joystick()); // joystick must NOT be suppressed while L (brake) is held!

    // Hold L + A in driving mode: L is the brake, not the shortcut layer, and
    // unmodified face buttons are joystick only, so no key
    shortcuts.process((1 << 8) | (1 << 4), false, false, true);
    assert(events.empty());
    assert(!shortcuts.suppress_joystick());

    // Release all in driving mode
    shortcuts.process(0, false, false, true);
    assert(events.empty());
    assert(!shortcuts.suppress_joystick());

    test_disk_names();

    printf("All shortcut tests passed successfully!\n");
    return 0;
}
