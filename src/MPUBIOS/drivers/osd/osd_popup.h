#ifndef OSD_POPUP_H
#define OSD_POPUP_H
#include <inttypes.h>

// One line info popup on top of the game (disk change messages, floppy status).
// Show and poll from the main loop only. The mouse popup (input interrupt) and the
// OSD keyboard leave the OSD alone while a popup is active.

#define OSD_POPUP_STATUS  0   // floppy status line, replaced by any message
#define OSD_POPUP_MESSAGE 1   // disk change and error messages

void osd_popup_show(const char *text, unsigned int duration_ms, int priority);
bool osd_popup_active();
void osd_popup_cancel();      // safe from the interrupt: the caller owns the OSD now
void osd_popup_poll();

#endif
