#include "osd_popup.h"
#include "osd.h"
#include "timer.h"

extern bool keyboard_osd_showing;

static volatile bool popup_on = false;
static volatile bool popup_drawn = false;   // OSD line holds popup_text
static int           popup_priority = OSD_POPUP_STATUS;
static unsigned int  popup_until;
static char          popup_text[32];

static bool popup_same_text(const char *text)
{
  int i = 0;
  while (i < 31 && text[i] && text[i] == popup_text[i]) i++;
  return (i == 31) || (text[i] == popup_text[i]);
}

void osd_popup_show(const char *text, unsigned int duration_ms, int priority)
{
  unsigned int now = GetMilliseconds();
  // The OSD keyboard owns the OSD while it is open
  if (keyboard_osd_showing) return;
  // A status line never replaces a message that is still showing
  if (popup_on && priority < popup_priority && (int)(popup_until - now) > 0) return;

  // Mark it active before drawing so the interrupt does not switch it off meanwhile
  popup_priority = priority;
  popup_until = now + duration_ms;
  popup_on = true;
  if (popup_drawn && popup_same_text(text)) return;

  int i = 0;
  while (i < 31 && text[i]) {
    popup_text[i] = text[i];
    i++;
  }
  popup_text[i] = 0;
  OsdClear();
  OsdWrite(popup_text);
  OsdUpdate(0);
  InfoEnable(10, 10, 32, 1, 291, 31744);
  popup_drawn = true;
}

bool osd_popup_active()
{
  return popup_on;
}

void osd_popup_cancel()
{
  popup_on = false;
  popup_drawn = false;
}

void osd_popup_poll()
{
  if (!popup_on) return;
  if (keyboard_osd_showing) {
    // The keyboard has drawn over it
    osd_popup_cancel();
    return;
  }
  if ((int)(popup_until - GetMilliseconds()) <= 0) {
    osd_popup_cancel();
    OsdDisable();
  }
}
