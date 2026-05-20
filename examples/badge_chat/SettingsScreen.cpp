#include "SettingsScreen.h"
#include <string.h>
#include <math.h>

// Live radio re-apply (declared in the variant's target.cpp).
extern void radio_set_params(float freq, float bw, uint8_t sf, uint8_t cr);
extern void radio_set_tx_power(int8_t dbm);

static const char* TELEM_OPTS[] = { "deny", "flags", "all" };
static const char* LOC_OPTS[]   = { "none", "share" };
static const char* BW_OPTS[]    = { "62", "125", "250", "500" };
static const float BW_VALS[]    = { 62.5f, 125.0f, 250.0f, 500.0f };

void SettingsScreen::addItem(const Item& it) {
  if (_n < MAX_ITEMS) _items[_n++] = it;
}

void SettingsScreen::begin(MyMesh* mesh, NodePrefs* prefs) {
  _mesh = mesh; _prefs = prefs;
  _n = 0;
  if (!_prefs) return;

  addItem({"Name",        K_STR,   _prefs->node_name,        0,0,0,        NULL,0, NULL, 31});
  addItem({"Freq MHz",    K_FLOAT, &_prefs->freq,            400,960,0.125, NULL,0, NULL, 0});
  addItem({"Bandwidth",   K_FENUM, &_prefs->bw,              0,0,0,        BW_OPTS,4, BW_VALS, 0});
  addItem({"SpreadFac",   K_U8,    &_prefs->sf,              7,12,1,       NULL,0, NULL, 0});
  addItem({"CodingRate",  K_U8,    &_prefs->cr,              5,8,1,        NULL,0, NULL, 0});
  addItem({"TX power dB", K_I8,    &_prefs->tx_power_dbm,    -9,22,1,      NULL,0, NULL, 0});
  addItem({"RX boost",    K_BOOL,  &_prefs->rx_boosted_gain, 0,0,0,        NULL,0, NULL, 0});
  addItem({"Airtime f",   K_FLOAT, &_prefs->airtime_factor,  0,9,0.1,      NULL,0, NULL, 0});
  addItem({"Multi-ack",   K_U8,    &_prefs->multi_acks,      0,3,1,        NULL,0, NULL, 0});
  addItem({"Manual add",  K_BOOL,  &_prefs->manual_add_contacts, 0,0,0,    NULL,0, NULL, 0});
  addItem({"RX delay",    K_FLOAT, &_prefs->rx_delay_base,   0,50,0.5,     NULL,0, NULL, 0});
  addItem({"Max hops",    K_U8,    &_prefs->autoadd_max_hops, 0,64,1,      NULL,0, NULL, 0});
  addItem({"Path hash",   K_U8,    &_prefs->path_hash_mode,  0,3,1,        NULL,0, NULL, 0});
  addItem({"Repeater",    K_BOOL,  &_prefs->client_repeat,   0,0,0,        NULL,0, NULL, 0});
  addItem({"Telem base",  K_ENUM,  &_prefs->telemetry_mode_base, 0,0,0,    TELEM_OPTS,3, NULL, 0});
  addItem({"Telem loc",   K_ENUM,  &_prefs->telemetry_mode_loc,  0,0,0,    TELEM_OPTS,3, NULL, 0});
  addItem({"Telem env",   K_ENUM,  &_prefs->telemetry_mode_env,  0,0,0,    TELEM_OPTS,3, NULL, 0});
  addItem({"Loc policy",  K_ENUM,  &_prefs->advert_loc_policy, 0,0,0,      LOC_OPTS,2, NULL, 0});
  addItem({"BLE pin",     K_U32,   &_prefs->ble_pin,         0,999999,1,   NULL,0, NULL, 0});
}

double SettingsScreen::getNum(const Item& it) const {
  switch (it.kind) {
    case K_FLOAT: return *(float*)it.ptr;
    case K_U8:    return *(uint8_t*)it.ptr;
    case K_I8:    return *(int8_t*)it.ptr;
    case K_U32:   return *(uint32_t*)it.ptr;
    case K_BOOL:
    case K_ENUM:  return *(uint8_t*)it.ptr;
    case K_FENUM: {
      float v = *(float*)it.ptr; int best = 0; float bd = 1e9f;
      for (int i = 0; i < it.nopts; i++) {
        float d = fabsf(it.fvals[i] - v);
        if (d < bd) { bd = d; best = i; }
      }
      return best;
    }
    default: return 0;
  }
}

void SettingsScreen::setNum(const Item& it, double v) {
  switch (it.kind) {
    case K_FLOAT: *(float*)it.ptr = (float)v; break;
    case K_U8:    *(uint8_t*)it.ptr = (uint8_t)lround(v); break;
    case K_I8:    *(int8_t*)it.ptr = (int8_t)lround(v); break;
    case K_U32:   *(uint32_t*)it.ptr = (uint32_t)llround(v); break;
    case K_BOOL:
    case K_ENUM:  *(uint8_t*)it.ptr = (uint8_t)lround(v); break;
    case K_FENUM: { int i = (int)lround(v); if (i < 0) i = 0; if (i >= it.nopts) i = it.nopts-1;
                    *(float*)it.ptr = it.fvals[i]; break; }
    default: break;
  }
}

void SettingsScreen::valueStr(const Item& it, char* out, size_t n) const {
  switch (it.kind) {
    case K_STR:   snprintf(out, n, "%s", (const char*)it.ptr); break;
    case K_FLOAT: snprintf(out, n, "%g", *(float*)it.ptr); break;
    case K_U8:    snprintf(out, n, "%u", *(uint8_t*)it.ptr); break;
    case K_I8:    snprintf(out, n, "%d", *(int8_t*)it.ptr); break;
    case K_U32:
      // BLE pin: 0 means "unset" — show the effective compiled default.
      if (_prefs && it.ptr == &_prefs->ble_pin && *(uint32_t*)it.ptr == 0) {
        snprintf(out, n, "0 (def %u)", _mesh ? (unsigned)_mesh->getBLEPin() : 0);
      } else {
        snprintf(out, n, "%lu", (unsigned long)*(uint32_t*)it.ptr);
      }
      break;
    case K_BOOL:  snprintf(out, n, "%s", *(uint8_t*)it.ptr ? "on" : "off"); break;
    case K_ENUM:
    case K_FENUM: { int i = (int)getNum(it); snprintf(out, n, "%s", (i>=0 && i<it.nopts) ? it.opts[i] : "?"); break; }
    default: out[0] = 0;
  }
}

void SettingsScreen::applyAndSave() {
  radio_set_params(_prefs->freq, _prefs->bw, _prefs->sf, _prefs->cr);
  radio_set_tx_power(_prefs->tx_power_dbm);
  if (_mesh) _mesh->savePrefs();
}

void SettingsScreen::beginEdit() {
  const Item& it = _items[_sel];
  if (it.kind == K_STR) {
    strncpy(_edit_buf, (const char*)it.ptr, sizeof(_edit_buf)-1);
    _edit_buf[sizeof(_edit_buf)-1] = 0;
    _edit_len = strlen(_edit_buf);
  } else {
    _edit_val = getNum(it);   // for enums this is the index
  }
  _editing = true;
}

void SettingsScreen::commitEdit() {
  const Item& it = _items[_sel];
  if (it.kind == K_STR) {
    uint8_t cap = it.strmax ? it.strmax : 31;
    strncpy((char*)it.ptr, _edit_buf, cap);
    ((char*)it.ptr)[cap] = 0;
  } else {
    setNum(it, _edit_val);
  }
  _editing = false;
  applyAndSave();
}

bool SettingsScreen::handleKey(const KeyEvent& k) {
  if (_n == 0) return false;

  if (_editing) {
    const Item& it = _items[_sel];
    if (it.kind == K_STR) {
      if (k.type == KeyEvent::CHAR) {
        if (_edit_len < (int)sizeof(_edit_buf)-1) { _edit_buf[_edit_len++] = k.chr; _edit_buf[_edit_len] = 0; }
        return true;
      }
      if (k.type == KeyEvent::NAV) {
        if (k.nav == KeyEvent::BACKSPACE) { if (_edit_len > 0) _edit_buf[--_edit_len] = 0; return true; }
        if (k.nav == KeyEvent::ENTER)     { commitEdit(); return true; }
        if (k.nav == KeyEvent::ESC)       { _editing = false; return true; }
      }
      return true;  // swallow other keys while editing text
    }
    // numeric / enum / bool-as-value
    if (k.type == KeyEvent::NAV) {
      bool isEnum = (it.kind == K_ENUM || it.kind == K_FENUM);
      double step = isEnum ? 1 : it.vstep;
      double lo   = isEnum ? 0 : it.vmin;
      double hi   = isEnum ? it.nopts-1 : it.vmax;
      if (k.nav == KeyEvent::UP)   { _edit_val += step; if (_edit_val > hi) _edit_val = hi; return true; }
      if (k.nav == KeyEvent::DOWN) { _edit_val -= step; if (_edit_val < lo) _edit_val = lo; return true; }
      if (k.nav == KeyEvent::ENTER){ commitEdit(); return true; }
      if (k.nav == KeyEvent::ESC)  { _editing = false; return true; }
    }
    return true;
  }

  // list navigation
  if (k.type != KeyEvent::NAV) return false;
  if (k.nav == KeyEvent::UP)   { if (_sel > 0) _sel--; return true; }
  if (k.nav == KeyEvent::DOWN) { if (_sel < _n-1) _sel++; return true; }
  if (k.nav == KeyEvent::ENTER) {
    Item& it = _items[_sel];
    if (it.kind == K_BOOL) {                 // toggle inline, no modal
      *(uint8_t*)it.ptr = *(uint8_t*)it.ptr ? 0 : 1;
      applyAndSave();
    } else {
      beginEdit();
    }
    return true;
  }
  return false;
}

void SettingsScreen::drawList(DisplayDriver& d) {
  d.setTextSize(UI_INFO_FONT);
  const int lh      = UI_INFO_LINE_H;
  const int visible = (UI_CONTENT_BOTTOM - UI_CONTENT_TOP) / lh;   // ~5

  if (_sel < _scroll) _scroll = _sel;
  if (_sel >= _scroll + visible) _scroll = _sel - visible + 1;

  char val[40], line[64];
  int y = UI_CONTENT_TOP;
  for (int i = _scroll; i < _n && i < _scroll + visible; i++) {
    valueStr(_items[i], val, sizeof(val));
    snprintf(line, sizeof(line), "%s: %s", _items[i].label, val);
    if (i == _sel) {
      d.setColor(DisplayDriver::LIGHT);
      d.fillRect(0, y, UI_SCREEN_W, lh);
      d.setColor(DisplayDriver::DARK);
      d.setCursor(2, y);
      d.print(line);
      d.setColor(DisplayDriver::LIGHT);
    } else {
      d.setColor(DisplayDriver::LIGHT);
      d.setCursor(2, y);
      d.print(line);
    }
    y += lh;
  }
}

void SettingsScreen::drawEditor(DisplayDriver& d) {
  const Item& it = _items[_sel];
  d.setColor(DisplayDriver::LIGHT);
  d.setTextSize(UI_INFO_FONT);

  int y = UI_CONTENT_TOP;
  d.setCursor(2, y);
  d.print(it.label);
  y += UI_INFO_LINE_H;

  char buf[64];
  if (it.kind == K_STR) {
    snprintf(buf, sizeof(buf), "> %s_", _edit_buf);
    d.setCursor(2, y); d.print(buf); y += UI_INFO_LINE_H;
    d.setCursor(2, y); d.print("type, ENTER ok, ESC x");
  } else {
    char val[32];
    bool isEnum = (it.kind == K_ENUM || it.kind == K_FENUM);
    if (isEnum) {
      int i = (int)_edit_val;
      snprintf(val, sizeof(val), "%s", (i>=0 && i<it.nopts) ? it.opts[i] : "?");
    } else if (it.kind == K_FLOAT) {
      snprintf(val, sizeof(val), "%g", _edit_val);
    } else {
      snprintf(val, sizeof(val), "%ld", lround(_edit_val));
    }
    snprintf(buf, sizeof(buf), "< %s >", val);
    d.setCursor(2, y); d.print(buf); y += UI_INFO_LINE_H;
    d.setCursor(2, y); d.print("UP/DN, ENTER ok, ESC x");
  }
}

void SettingsScreen::draw(DisplayDriver& d) {
  if (_n == 0) {
    d.setColor(DisplayDriver::LIGHT);
    d.setTextSize(UI_INFO_FONT);
    d.setCursor(2, UI_CONTENT_TOP);
    d.print("No settings");
    return;
  }
  if (_editing) drawEditor(d);
  else          drawList(d);
}
