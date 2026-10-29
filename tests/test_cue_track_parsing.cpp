// Host-side unit test for the CUE TRACK-line parsing used by
// src/MPUBIOS/drivers/cue/cue.cpp.
//
// The security fix (cpp/incorrectly-checked-scanf) replaces the buggy
//
//      else if ((sscanf(lptr, "TRACK %d %*s", &bb)) ||
//               (sscanf(lptr, "TRACK %d %*s", &bb)))
//
// with an explicit return-value comparison:
//
//      else if (sscanf(lptr, "TRACK %d %*s", &bb) == 1)
//
// With format "TRACK %d %*s" exactly one assignment (%d) is performed, so a
// successful parse returns 1. A failing parse returns 0, and an input failure
// (e.g. empty line) returns EOF (-1). The old truthiness check treated -1 as
// true and would enter the branch with an uninitialized `bb`; the explicit
// `== 1` check rejects it correctly.
//
// This file is compiled and run on the host with a plain C++ compiler (see
// src/MPUBIOS/drivers/KMIO/test/Makefile for the same pattern). cue.cpp itself
// cannot be compiled on the host because it pulls in MPU/hardware headers.

#include <cassert>
#include <cstdio>
#include <cstring>

// Mirrors the exact sscanf call site in cue.cpp:111.
static int sscanf_track(const char *line, int *bb)
{
    return sscanf(line, "TRACK %d %*s", bb);
}

// The corrected condition: exactly one conversion must succeed.
static bool track_condition_fixed(const char *line)
{
    int bb = 0;
    return sscanf_track(line, &bb) == 1;
}

// The previous buggy condition: truthiness of the sscanf return value.
static bool track_condition_buggy(const char *line)
{
    int bb = 0;
    return (sscanf_track(line, &bb)) || (sscanf_track(line, &bb));
}

int main()
{
    int bb = 0;

    /* --- Valid CUE TRACK lines: exactly one conversion succeeds --- */
    assert(sscanf_track("TRACK 1 AUDIO", &bb) == 1);
    assert(bb == 1);
    assert(track_condition_fixed("TRACK 1 AUDIO") == true);

    assert(sscanf_track("TRACK 10 MODE1/2048", &bb) == 1);
    assert(bb == 10);
    assert(track_condition_fixed("TRACK 10 MODE1/2048") == true);

    assert(sscanf_track("TRACK 99", &bb) == 1);
    assert(bb == 99);
    assert(track_condition_fixed("TRACK 99") == true);

    /* --- Invalid lines: no conversion (or fewer than one) succeeds --- */
    assert(sscanf_track("TRACK", &bb) != 1);
    assert(track_condition_fixed("TRACK") == false);

    assert(sscanf_track("FILE track01.bin BINARY", &bb) != 1);
    assert(track_condition_fixed("FILE track01.bin BINARY") == false);

    assert(sscanf_track("", &bb) != 1);
    assert(track_condition_fixed("") == false);

    /* --- The bug that the fix addresses: EOF (-1) must be rejected --- */
    /* sscanf on an empty string returns EOF (-1). With the old buggy
       truthiness check (-1) || (-1) evaluates to true and the branch would
       be entered with an uninitialized `bb`. The fixed `== 1` check rejects
       it. */
    assert(sscanf_track("", &bb) == -1);
    assert(track_condition_buggy("") == true);   // the defect
    assert(track_condition_fixed("") == false);  // the fix

    printf("All CUE TRACK parsing tests passed.\n");
    return 0;
}
