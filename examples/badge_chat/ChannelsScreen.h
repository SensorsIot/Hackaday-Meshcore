#pragma once

#include "Screen.h"
#include "UILayout.h"
#include <helpers/BaseChatMesh.h>

// Read-only channel list (F3): the configured group channels by name
// (index 0 is the default "Public"). ↑/↓ scroll. No editing.
class ChannelsScreen : public Screen {
public:
  ChannelsScreen() : _mesh(NULL), _scroll(0) {}
  void begin(BaseChatMesh* mesh) { _mesh = mesh; }

  const char* title() const override { return "Channels"; }
  void onEnter() override { _scroll = 0; }
  void draw(DisplayDriver& d) override;
  bool handleKey(const KeyEvent& k) override;

private:
  BaseChatMesh* _mesh;
  int           _scroll;
};
