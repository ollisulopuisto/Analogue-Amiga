#include <cassert>
#include <cstdio>
#include <vector>
#include <cstdint>

// Forward declaration of the shortcut logic to be implemented
#include "../src/MPUBIOS/drivers/KMIO/pocket_shortcuts.h"

int main() {
    PocketShortcuts shortcuts;
    std::vector<std::pair<uint8_t, bool>> events;

    auto callback = [](void *userdata, uint8_t code, bool down) {
        auto *ev = static_cast<std::vector<std::pair<uint8_t, bool>>*>(userdata);
        ev->push_back({code, down});
    };
    shortcuts.set_callback(callback, &events);

    printf("Running test: Unmodified X -> Space, Y -> Return...\n");
    // Press X (bit 6)
    shortcuts.process(1 << 6, false, false);
    assert(events.size() == 1);
    assert(events[0].first == AMIGA_SPACE && events[0].second == true);
    assert(!shortcuts.suppress_joystick());
    events.clear();

    // Release X
    shortcuts.process(0, false, false);
    assert(events.size() == 1);
    assert(events[0].first == AMIGA_SPACE && events[0].second == false);
    events.clear();

    // Press Y (bit 7)
    shortcuts.process(1 << 7, false, false);
    assert(events.size() == 1);
    assert(events[0].first == AMIGA_RETURN && events[0].second == true);
    events.clear();

    shortcuts.process(0, false, false);
    assert(events.size() == 1);
    assert(events[0].first == AMIGA_RETURN && events[0].second == false);
    events.clear();

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

    printf("All shortcut tests passed successfully!\n");
    return 0;
}
