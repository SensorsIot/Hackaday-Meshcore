#pragma once

#include "Screen.h"
#include "UILayout.h"
#include "MyMesh.h"      // savePrefs(), NodePrefs
#include "NodePrefs.h"

// Editable settings (F4): a scrollable list of NodePrefs fields. ↑/↓ move
// the highlight, Enter edits the selected row with a type-appropriate
// modal, ESC cancels. On commit the value is written to NodePrefs, the
// radio params are re-applied, and prefs are persisted.
class SettingsScreen : public Screen {
public:
  enum Kind { K_STR, K_FLOAT, K_U8, K_I8, K_U32, K_BOOL, K_ENUM, K_FENUM };

  struct Item {
    const char* label;
    Kind        kind;
    void*       ptr;          // points at the NodePrefs field
    double      vmin, vmax, vstep;            // numeric range/step
    const char* const* opts;  uint8_t nopts;  // enum labels
    const float* fvals;                       // K_FENUM: value per option
    uint8_t     strmax;                       // K_STR capacity
  };

  static const int MAX_ITEMS = 24;

  SettingsScreen() : _mesh(NULL), _prefs(NULL), _n(0), _sel(0), _scroll(0),
                     _editing(false), _edit_val(0), _edit_idx(0), _edit_len(0) {
    _edit_buf[0] = 0;
  }
  void begin(MyMesh* mesh, NodePrefs* prefs);

  const char* title() const override { return "Settings"; }
  void onExit() override { _editing = false; }
  void draw(DisplayDriver& d) override;
  bool handleKey(const KeyEvent& k) override;

private:
  MyMesh*    _mesh;
  NodePrefs* _prefs;
  Item       _items[MAX_ITEMS];
  int        _n;
  int        _sel;       // highlighted row
  int        _scroll;    // first visible row

  bool       _editing;   // edit modal active for _sel
  double     _edit_val;   // working numeric value
  int        _edit_idx;   // working enum index
  char       _edit_buf[40];  // working text
  int        _edit_len;

  void addItem(const Item& it);
  void valueStr(const Item& it, char* out, size_t n) const;
  double getNum(const Item& it) const;
  void   setNum(const Item& it, double v);
  void beginEdit();
  void commitEdit();
  void applyAndSave();
  void drawList(DisplayDriver& d);
  void drawEditor(DisplayDriver& d);
};
