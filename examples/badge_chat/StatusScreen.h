#pragma once

#include "Screen.h"
#include "UILayout.h"
#include <helpers/BaseChatMesh.h>
#include "NodePrefs.h"

// Read-only status/debug screen (F5): identity, firmware, radio params,
// contact count, battery, uptime, free heap. No editing.
class StatusScreen : public Screen {
public:
  StatusScreen() : _mesh(NULL), _prefs(NULL), _board(NULL) {}
  void begin(BaseChatMesh* mesh, NodePrefs* prefs, mesh::MainBoard* board) {
    _mesh = mesh; _prefs = prefs; _board = board;
  }

  const char* title() const override { return "Status"; }
  void draw(DisplayDriver& d) override;
  bool handleKey(const KeyEvent& /*k*/) override { return false; }

private:
  BaseChatMesh*    _mesh;
  NodePrefs*       _prefs;
  mesh::MainBoard* _board;
};
