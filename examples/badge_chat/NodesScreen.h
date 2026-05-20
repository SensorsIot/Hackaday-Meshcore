#pragma once

#include "Screen.h"
#include "UILayout.h"
#include <helpers/BaseChatMesh.h>

// Read-only contact list (F2): scrollable list of known nodes by name,
// with hop count. ↑/↓ scroll. No editing.
class NodesScreen : public Screen {
public:
  NodesScreen() : _mesh(NULL), _scroll(0) {}
  void begin(BaseChatMesh* mesh) { _mesh = mesh; }

  const char* title() const override { return "Nodes"; }
  void onEnter() override { _scroll = 0; }
  void draw(DisplayDriver& d) override;
  bool handleKey(const KeyEvent& k) override;

private:
  BaseChatMesh* _mesh;
  int           _scroll;
};
