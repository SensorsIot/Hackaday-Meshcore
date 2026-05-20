#pragma once

#include <helpers/ui/DisplayDriver.h>
#include "TCA8418Keyboard.h"   // KeyEvent

// Flat-screen base (FSD §6). The router (BadgeUITask) draws the status
// bar and F-key strip; each Screen draws only the content band
// (UI_CONTENT_TOP..UI_CONTENT_BOTTOM) and handles keys when current.
class Screen {
public:
  virtual ~Screen() {}
  virtual void onEnter() {}
  virtual void onExit()  {}
  virtual const char* title() const = 0;
  virtual void draw(DisplayDriver& d) = 0;
  virtual bool handleKey(const KeyEvent& k) = 0;  // true = consumed
  virtual void tick() {}
};
