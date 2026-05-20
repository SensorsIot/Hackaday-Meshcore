#pragma once

// Screen geometry and font metrics for the badge UI (FSD §6).
//
// NV3007Display renders Arduino_GFX's built-in 5x7 font scaled by
// setTextSize(): size 1 = 6x8 px per glyph, size 2 = 12x16 px. All
// layout below is derived from those metrics on the 428x142 panel.

#define UI_CHAR_W1   6
#define UI_CHAR_H1   8
#define UI_CHAR_W2   12
#define UI_CHAR_H2   16

#define UI_SCREEN_W  NV3007_WIDTH    // 428 (build flag)
#define UI_SCREEN_H  NV3007_HEIGHT   // 142 (build flag)

// Vertical bands (y, top-down):
//   status bar      0..9    (size 1)
//   separator       10
//   content        13..128  (screen-owned)
//   separator       130
//   F-key strip    133..140 (size 1)
#define UI_STATUS_Y       1
#define UI_STATUS_SEP_Y   10
#define UI_CONTENT_TOP    13
#define UI_CONTENT_BOTTOM 128
#define UI_FKEY_SEP_Y     130
#define UI_FKEY_Y         133

// Chat history fills most of the content band; the compose line sits in
// a fixed strip just above the F-key separator.
#define UI_CHAT_TOP        UI_CONTENT_TOP   // 13
#define UI_COMPOSE_SEP_Y   109
#define UI_COMPOSE_Y       113
#define UI_CHAT_LINE_H     UI_CHAR_H2       // 16
#define UI_CHARS_PER_LINE  (UI_SCREEN_W / UI_CHAR_W2)   // 35 @428/12

// Read-only info/list screens (Status, Nodes) use the proportional
// FreeSans9pt7b font (NV3007Display setTextSize(3)). ~22 px glyph line
// height; 21 px pitch reads cleanly without rows touching.
#define UI_INFO_FONT       3
#define UI_INFO_LINE_H     21
