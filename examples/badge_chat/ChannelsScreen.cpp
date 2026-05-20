#include "ChannelsScreen.h"

void ChannelsScreen::draw(DisplayDriver& d) {
  d.setColor(DisplayDriver::LIGHT);
  d.setTextSize(UI_INFO_FONT);

  const int lh      = UI_INFO_LINE_H;
  const int visible = (UI_CONTENT_BOTTOM - UI_CONTENT_TOP) / lh;   // ~5

  if (!_mesh) return;

  // Empty channel slots have a blank name; only named slots are real.
  ChannelDetails ch;
  int total = 0;
  for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
    if (_mesh->getChannel(i, ch) && ch.name[0]) total++;
  }
  if (total == 0) {
    d.setCursor(2, UI_CONTENT_TOP);
    d.print("No channels");
    return;
  }
  int max_scroll = total - visible;
  if (max_scroll < 0) max_scroll = 0;
  if (_scroll > max_scroll) _scroll = max_scroll;
  if (_scroll < 0) _scroll = 0;

  int shown = 0, skipped = 0, y = UI_CONTENT_TOP;
  char buf[48];
  for (int i = 0; i < MAX_GROUP_CHANNELS && shown < visible; i++) {
    if (!_mesh->getChannel(i, ch) || ch.name[0] == 0) continue;
    if (skipped++ < _scroll) continue;
    snprintf(buf, sizeof(buf), "%s%s", ch.name, i == 0 ? "  (default)" : "");
    d.setCursor(2, y);
    d.print(buf);
    y += lh;
    shown++;
  }
}

bool ChannelsScreen::handleKey(const KeyEvent& k) {
  if (k.type != KeyEvent::NAV) return false;
  if (k.nav == KeyEvent::UP)   { if (_scroll > 0) _scroll--; return true; }
  if (k.nav == KeyEvent::DOWN) { _scroll++; return true; }   // clamps naturally
  return false;
}
