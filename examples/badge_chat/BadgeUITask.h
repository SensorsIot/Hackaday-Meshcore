#pragma once

#include <MeshCore.h>
#include <helpers/ui/DisplayDriver.h>
#include <helpers/SensorManager.h>
#include <helpers/BaseSerialInterface.h>
#include <helpers/BaseChatMesh.h>
#include <Arduino.h>

#include "AbstractUITask.h"   // from companion_radio (reused via -I)
#include "NodePrefs.h"        // from companion_radio (reused via -I)
#include "Screen.h"
#include "ChatScreen.h"
#include "NodesScreen.h"
#include "ChannelsScreen.h"
#include "StatusScreen.h"
#include "SettingsScreen.h"   // pulls MyMesh.h

// BadgeUITask — Hackaday badge UI (FSD §6). Flat-screens + router:
// F1..F5 switch the current screen; the task draws the status bar and
// F-key label strip, and each screen draws its own content band. Keys
// from the TCA8418 are drained in loop() and dispatched to the current
// screen (function keys are handled by the router first).
class BadgeUITask : public AbstractUITask {
public:
  enum ScreenId { SCR_CHAT = 0, SCR_NODES, SCR_CHANNELS, SCR_SETTINGS, SCR_STATUS, SCR_COUNT };

  BadgeUITask(mesh::MainBoard* board, BaseSerialInterface* serial);

  void begin(DisplayDriver* display, SensorManager* sensors, NodePrefs* node_prefs);
  void setMesh(MyMesh* mesh);   // wired from main.cpp

  // --- AbstractUITask ---
  void msgRead(int msgcount) override;
  void newMsg(uint8_t path_len, const char* from_name, const char* text, int msgcount) override;
  void notify(UIEventType t = UIEventType::none) override;
  void loop() override;

private:
  DisplayDriver* _display;
  SensorManager* _sensors;
  NodePrefs*     _node_prefs;
  MyMesh*        _mesh;
  bool           _dirty;
  unsigned long  _next_refresh;

  int            _cur;
  ChatScreen     _chat;
  NodesScreen    _nodes;
  ChannelsScreen _channels;
  SettingsScreen _settings;
  StatusScreen   _status;
  Screen*        _screens[SCR_COUNT];

  void setScreen(int id);
  void tryInitChat();
  void render();
  void drawStatusBar();
  void drawFKeyStrip();
};
