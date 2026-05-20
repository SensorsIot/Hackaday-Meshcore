#include "StatusScreen.h"
#include "MyMesh.h"   // FIRMWARE_VERSION / FIRMWARE_BUILD_DATE

void StatusScreen::draw(DisplayDriver& d) {
  d.setColor(DisplayDriver::LIGHT);
  d.setTextSize(UI_INFO_FONT);

  char buf[64];
  int y = UI_CONTENT_TOP;
  const int lh = UI_INFO_LINE_H;

  // 5 lines fit in the content band at this font; pack the fields.
  d.setCursor(2, y);
  snprintf(buf, sizeof(buf), "Name: %s", _prefs ? _prefs->node_name : "?");
  d.print(buf); y += lh;

  d.setCursor(2, y);
  snprintf(buf, sizeof(buf), "FW %s   %d nodes",
           FIRMWARE_VERSION, _mesh ? _mesh->getNumContacts() : 0);
  d.print(buf); y += lh;

  if (_prefs) {
    d.setCursor(2, y);
    snprintf(buf, sizeof(buf), "%.3f MHz  SF%u  BW%.0f", _prefs->freq, _prefs->sf, _prefs->bw);
    d.print(buf); y += lh;

    d.setCursor(2, y);
    snprintf(buf, sizeof(buf), "CR%u  TX%ddBm  %umV",
             _prefs->cr, _prefs->tx_power_dbm,
             _board ? _board->getBattMilliVolts() : 0);
    d.print(buf); y += lh;
  }

  unsigned long s = millis() / 1000;
  d.setCursor(2, y);
  snprintf(buf, sizeof(buf), "Up %luh%02lum  heap %uk",
           s / 3600, (s / 60) % 60, (unsigned)(ESP.getFreeHeap() / 1024));
  d.print(buf); y += lh;
}
