#include "BadgeUITask.h"
#include "UILayout.h"
#include "TCA8418Keyboard.h"   // variant driver (reused via -I)
#include "target.h"            // radio_driver (for last-packet RSSI)
#include <helpers/ChannelDetails.h>
#include <helpers/AutoDiscoverRTCClock.h>

extern TCA8418Keyboard keyboard;     // owned by the variant's target.cpp
extern AutoDiscoverRTCClock rtc_clock;

#define DEFAULT_CHANNEL_IDX 0

BadgeUITask::BadgeUITask(mesh::MainBoard* board, BaseSerialInterface* serial)
  : AbstractUITask(board, serial),
    _display(NULL), _sensors(NULL), _node_prefs(NULL), _mesh(NULL),
    _dirty(true), _next_refresh(0), _cur(SCR_CHAT) {
  _screens[SCR_CHAT]     = &_chat;
  _screens[SCR_NODES]    = &_nodes;
  _screens[SCR_CHANNELS] = &_channels;
  _screens[SCR_SETTINGS] = &_settings;
  _screens[SCR_STATUS]   = &_status;
}

void BadgeUITask::begin(DisplayDriver* display, SensorManager* sensors, NodePrefs* node_prefs) {
  _display    = display;
  _sensors    = sensors;
  _node_prefs = node_prefs;
  tryInitChat();
  _dirty = true;
}

void BadgeUITask::setMesh(MyMesh* mesh) {
  _mesh = mesh;
  tryInitChat();
}

void BadgeUITask::tryInitChat() {
  if (_mesh && _node_prefs) {
    _chat.begin(_mesh, _node_prefs);
    _nodes.begin(_mesh);
    _channels.begin(_mesh);
    _status.begin(_mesh, _node_prefs, _board);
    _settings.begin(_mesh, _node_prefs);
  }
}

void BadgeUITask::setScreen(int id) {
  if (id == _cur || id < 0 || id >= SCR_COUNT) return;
  _screens[_cur]->onExit();
  _cur = id;
  _screens[_cur]->onEnter();
  _dirty = true;
}

void BadgeUITask::msgRead(int /*msgcount*/) { _dirty = true; }

void BadgeUITask::newMsg(uint8_t /*path_len*/, const char* /*from_name*/,
                         const char* text, int /*msgcount*/) {
  // text already arrives as "<sender>: <message>".
  _chat.onIncoming(text);
  _dirty = true;
}

void BadgeUITask::notify(UIEventType /*t*/) { _dirty = true; }

void BadgeUITask::loop() {
  KeyEvent ev;
  while (keyboard.getNextKey(ev)) {
    if (ev.type == KeyEvent::FUNCTION && ev.fn_num >= 1 && ev.fn_num <= SCR_COUNT) {
      setScreen(ev.fn_num - 1);
    } else {
      _screens[_cur]->handleKey(ev);
    }
    _dirty = true;
  }

  unsigned long now = millis();
  if (_dirty || now >= _next_refresh) {
    render();
    _dirty = false;
    _next_refresh = now + 1000;   // periodic repaint (clock, node count)
  }
}

void BadgeUITask::drawStatusBar() {
  uint32_t t = rtc_clock.getCurrentTime();
  int hh = (int)((t / 3600) % 24);
  int mm = (int)((t / 60) % 60);

  const char* chan = "Public";
  ChannelDetails ch;
  if (_mesh && _mesh->getChannel(DEFAULT_CHANNEL_IDX, ch) && ch.name[0]) chan = ch.name;

  int nodes = _mesh ? _mesh->getNumContacts() : 0;

  // Last received packet's RSSI (negative dBm). 0/positive => nothing heard
  // yet, so show dashes.
  int rssi = (int)radio_driver.getLastRSSI();
  char rbuf[12];
  if (rssi < 0) snprintf(rbuf, sizeof(rbuf), "%ddBm", rssi);
  else          snprintf(rbuf, sizeof(rbuf), "--dBm");

  char buf[80];
  snprintf(buf, sizeof(buf), "%02d:%02d  #%s  nodes:%d  %s", hh, mm, chan, nodes, rbuf);

  _display->setColor(DisplayDriver::LIGHT);
  _display->setTextSize(1);
  _display->setCursor(2, UI_STATUS_Y);
  _display->print(buf);
  _display->fillRect(0, UI_STATUS_SEP_Y, UI_SCREEN_W, 1);
}

void BadgeUITask::drawFKeyStrip() {
  static const char* labels[SCR_COUNT] = {
    "F1 Chat", "F2 Nodes", "F3 Chan", "F4 Set", "F5 Stat"
  };
  _display->fillRect(0, UI_FKEY_SEP_Y, UI_SCREEN_W, 1);
  _display->setTextSize(1);

  const int cell = UI_SCREEN_W / SCR_COUNT;   // ~85px
  for (int i = 0; i < SCR_COUNT; i++) {
    int x = i * cell;
    int label_w = (int)strlen(labels[i]) * UI_CHAR_W1;
    int tx = x + (cell - label_w) / 2;
    if (i == _cur) {
      // Highlight current: filled cell, dark text.
      _display->setColor(DisplayDriver::LIGHT);
      _display->fillRect(x, UI_FKEY_Y - 1, cell, UI_CHAR_H1 + 2);
      _display->setColor(DisplayDriver::DARK);
      _display->setCursor(tx, UI_FKEY_Y);
      _display->print(labels[i]);
      _display->setColor(DisplayDriver::LIGHT);
    } else {
      _display->setColor(DisplayDriver::LIGHT);
      _display->setCursor(tx, UI_FKEY_Y);
      _display->print(labels[i]);
    }
  }
}

void BadgeUITask::render() {
  if (!_display) return;
  _display->startFrame(DisplayDriver::DARK);
  drawStatusBar();
  _screens[_cur]->draw(*_display);
  drawFKeyStrip();
  _display->endFrame();
}
