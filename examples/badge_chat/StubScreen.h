#pragma once

#include "Screen.h"
#include "UILayout.h"

// Placeholder for screens not yet built (Nodes/Channels/Settings/Status
// in M2). Draws its title and a "coming in M3" note so F-key navigation
// is exercisable end-to-end. Consumes no keys.
class StubScreen : public Screen {
public:
  explicit StubScreen(const char* name) : _name(name) {}

  const char* title() const override { return _name; }

  void draw(DisplayDriver& d) override {
    d.setColor(DisplayDriver::LIGHT);
    d.setTextSize(2);
    d.setCursor(6, UI_CONTENT_TOP + 8);
    d.print(_name);
    d.setTextSize(1);
    d.setCursor(6, UI_CONTENT_TOP + 32);
    d.print("coming in M3");
  }

  bool handleKey(const KeyEvent& /*k*/) override { return false; }

private:
  const char* _name;
};
