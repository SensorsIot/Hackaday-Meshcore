#pragma once

#include "Screen.h"
#include "UILayout.h"
#include <helpers/BaseChatMesh.h>
#include "NodePrefs.h"

// Single default-channel chat (FSD §6): scrollable history + persistent
// compose line. Incoming channel messages already arrive as
// "<sender>: <text>" (BaseChatMesh::sendGroupMessage encodes the sender),
// so they're stored verbatim; our own outgoing is stored with a "me:"
// prefix and transmitted on channel index 0 ("Public").
class ChatScreen : public Screen {
public:
  static const int   MAX_MSGS   = 40;
  static const int   MSG_LEN    = 100;   // stored chars per message
  static const int   MAX_LINES  = 80;    // wrapped display-line cache
  static const int   LINE_CHARS = UI_CHARS_PER_LINE;   // 35
  static const int   INPUT_LEN  = MAX_TEXT_LEN;        // 160

  ChatScreen() : _mesh(NULL), _prefs(NULL), _count(0), _head(0),
                 _scroll(0), _input_len(0), _cursor(0), _nlines(0) {
    _input[0] = 0;
  }

  void begin(BaseChatMesh* mesh, NodePrefs* prefs) { _mesh = mesh; _prefs = prefs; }

  // Called by the router when a channel message arrives.
  void onIncoming(const char* text);

  const char* title() const override { return "Chat"; }
  void draw(DisplayDriver& d) override;
  bool handleKey(const KeyEvent& k) override;

private:
  BaseChatMesh* _mesh;
  NodePrefs*    _prefs;

  // Message ring buffer.
  char _msgs[MAX_MSGS][MSG_LEN];
  int  _count;   // number of stored messages (<= MAX_MSGS)
  int  _head;    // index of oldest message
  int  _scroll;  // history lines scrolled up from bottom (0 = newest)

  // Compose line.
  char _input[INPUT_LEN + 1];
  int  _input_len;
  int  _cursor;

  // Wrapped-line cache, rebuilt each draw.
  char _lines[MAX_LINES][LINE_CHARS + 1];
  int  _nlines;

  void appendMsg(const char* text);
  void rebuildLines();
  void sendCurrent();
};
