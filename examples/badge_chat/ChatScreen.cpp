#include "ChatScreen.h"
#include "UILayout.h"
#include <helpers/ChannelDetails.h>
#include <helpers/AutoDiscoverRTCClock.h>
#include <string.h>

extern AutoDiscoverRTCClock rtc_clock;

#define DEFAULT_CHANNEL_IDX 0

void ChatScreen::appendMsg(const char* text) {
  int idx = (_head + _count) % MAX_MSGS;
  if (_count == MAX_MSGS) {
    _head = (_head + 1) % MAX_MSGS;   // overwrite oldest
  } else {
    _count++;
  }
  strncpy(_msgs[idx], text, MSG_LEN - 1);
  _msgs[idx][MSG_LEN - 1] = 0;
  _scroll = 0;   // jump to newest on any new message
}

void ChatScreen::onIncoming(const char* text) {
  appendMsg(text);   // already "<sender>: <text>"
}

void ChatScreen::sendCurrent() {
  if (_input_len == 0 || !_mesh) return;

  ChannelDetails ch;
  if (_mesh->getChannel(DEFAULT_CHANNEL_IDX, ch)) {
    const char* name = (_prefs && _prefs->node_name[0]) ? _prefs->node_name : "me";
    _mesh->sendGroupMessage(rtc_clock.getCurrentTime(), ch.channel, name, _input, _input_len);
  }

  // Echo locally with a "me:" prefix.
  char line[MSG_LEN];
  snprintf(line, sizeof(line), "me: %s", _input);
  appendMsg(line);

  _input[0] = 0;
  _input_len = 0;
  _cursor = 0;
}

// Greedy word-wrap of the ring buffer (oldest->newest) into _lines.
void ChatScreen::rebuildLines() {
  _nlines = 0;
  for (int m = 0; m < _count && _nlines < MAX_LINES; m++) {
    const char* s = _msgs[(_head + m) % MAX_MSGS];
    int len = strlen(s);
    int pos = 0;
    if (len == 0) {  // keep blank line slots from collapsing
      _lines[_nlines][0] = 0;
      _nlines++;
      continue;
    }
    while (pos < len && _nlines < MAX_LINES) {
      int remaining = len - pos;
      int take = remaining < LINE_CHARS ? remaining : LINE_CHARS;
      // Break at the last space within the window (unless the whole
      // remainder fits, or there's no space — then hard-break).
      if (take == LINE_CHARS && remaining > LINE_CHARS) {
        int brk = -1;
        for (int k = take; k > 0; k--) {
          if (s[pos + k] == ' ') { brk = k; break; }
        }
        if (brk > 0) take = brk;
      }
      memcpy(_lines[_nlines], s + pos, take);
      _lines[_nlines][take] = 0;
      _nlines++;
      pos += take;
      while (pos < len && s[pos] == ' ') pos++;   // skip the break space(s)
    }
  }
}

void ChatScreen::draw(DisplayDriver& d) {
  rebuildLines();

  int visible = (UI_COMPOSE_SEP_Y - UI_CHAT_TOP) / UI_CHAT_LINE_H;   // ~6
  if (_scroll < 0) _scroll = 0;
  int max_scroll = _nlines - visible;
  if (max_scroll < 0) max_scroll = 0;
  if (_scroll > max_scroll) _scroll = max_scroll;

  // Bottom-anchored window: last `visible` lines minus scroll offset.
  int last  = _nlines - _scroll;            // exclusive
  int first = last - visible;
  if (first < 0) first = 0;

  d.setColor(DisplayDriver::LIGHT);
  d.setTextSize(2);
  int y = UI_CHAT_TOP;
  for (int i = first; i < last; i++) {
    d.setCursor(2, y);
    d.print(_lines[i]);
    y += UI_CHAT_LINE_H;
  }

  // Separator above compose line.
  d.fillRect(0, UI_COMPOSE_SEP_Y, UI_SCREEN_W, 1);

  // Compose line: "> " + a horizontally-scrolled window around the cursor.
  const int prompt = 2;                              // "> " chars
  const int win    = UI_CHARS_PER_LINE - prompt;     // visible input chars
  int start = 0;
  if (_cursor > win - 1) start = _cursor - (win - 1);
  if (start > _input_len) start = _input_len;

  char shown[UI_CHARS_PER_LINE + 1];
  shown[0] = '>';
  shown[1] = ' ';
  int n = 0;
  while (n < win && (start + n) < _input_len) {
    shown[prompt + n] = _input[start + n];
    n++;
  }
  shown[prompt + n] = 0;

  d.setTextSize(2);
  d.setCursor(2, UI_COMPOSE_Y);
  d.print(shown);

  // Caret: 2px vertical bar at the cursor column.
  int caret_col = prompt + (_cursor - start);
  int caret_x = 2 + caret_col * UI_CHAR_W2;
  d.fillRect(caret_x, UI_COMPOSE_Y, 2, UI_CHAR_H2);
}

bool ChatScreen::handleKey(const KeyEvent& k) {
  if (k.type == KeyEvent::CHAR) {
    if (_input_len < INPUT_LEN) {
      // Insert at cursor.
      for (int i = _input_len; i > _cursor; i--) _input[i] = _input[i - 1];
      _input[_cursor] = k.chr;
      _input_len++;
      _cursor++;
      _input[_input_len] = 0;
    }
    return true;
  }
  if (k.type == KeyEvent::NAV) {
    switch (k.nav) {
      case KeyEvent::ENTER:
        sendCurrent();
        return true;
      case KeyEvent::BACKSPACE:
        if (_cursor > 0) {
          for (int i = _cursor - 1; i < _input_len - 1; i++) _input[i] = _input[i + 1];
          _input_len--;
          _cursor--;
          _input[_input_len] = 0;
        }
        return true;
      case KeyEvent::LEFT:
        if (_cursor > 0) _cursor--;
        return true;
      case KeyEvent::RIGHT:
        if (_cursor < _input_len) _cursor++;
        return true;
      case KeyEvent::UP:
        _scroll++;       // clamped in draw()
        return true;
      case KeyEvent::DOWN:
        if (_scroll > 0) _scroll--;
        return true;
      default:
        return false;
    }
  }
  return false;
}
