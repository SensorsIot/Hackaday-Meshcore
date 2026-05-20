#include "NodesScreen.h"

void NodesScreen::draw(DisplayDriver& d) {
  d.setColor(DisplayDriver::LIGHT);
  d.setTextSize(UI_INFO_FONT);

  const int lh      = UI_INFO_LINE_H;
  const int visible = (UI_CONTENT_BOTTOM - UI_CONTENT_TOP) / lh;   // ~5

  if (!_mesh || _mesh->getNumContacts() == 0) {
    d.setCursor(2, UI_CONTENT_TOP);
    d.print("No nodes yet");
    return;
  }

  // Clamp scroll to the list length.
  int total = _mesh->getNumContacts();
  int max_scroll = total - visible;
  if (max_scroll < 0) max_scroll = 0;
  if (_scroll > max_scroll) _scroll = max_scroll;
  if (_scroll < 0) _scroll = 0;

  ContactsIterator iter;
  ContactInfo c;
  int idx = 0, drawn = 0, y = UI_CONTENT_TOP;
  char buf[48];
  while (iter.hasNext(_mesh, c) && drawn < visible) {
    if (idx++ < _scroll) continue;
    const char* name = c.name[0] ? c.name : "(unnamed)";
    if (c.out_path_len == 0xFF) {
      snprintf(buf, sizeof(buf), "%s", name);            // path unknown
    } else if (c.out_path_len == 0) {
      snprintf(buf, sizeof(buf), "%s  direct", name);
    } else {
      snprintf(buf, sizeof(buf), "%s  %uh", name, c.out_path_len);
    }
    d.setCursor(2, y);
    d.print(buf);
    y += lh;
    drawn++;
  }
}

bool NodesScreen::handleKey(const KeyEvent& k) {
  if (k.type != KeyEvent::NAV) return false;
  if (k.nav == KeyEvent::UP)   { if (_scroll > 0) _scroll--; return true; }
  if (k.nav == KeyEvent::DOWN) { _scroll++; return true; }   // clamped in draw()
  return false;
}
