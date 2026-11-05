#pragma once

// Equal-size button grids below a header: 8 px padding, 6 px gaps,
// on the 320x240 landscape screen.
struct CellRect {
  int x;
  int y;
  int w;
  int h;
};

constexpr int UI_SCREEN_W = 320;
constexpr int UI_SCREEN_H = 240;
constexpr int UI_HEADER_H = 32;
constexpr int UI_GRID_PAD = 8;
constexpr int UI_GRID_GAP = 6;

CellRect gridCell(int index, int cols, int rows, int top);

// Index of the cell under (x, y), or -1 for header, padding and gaps.
int gridIndexAt(int x, int y, int cols, int rows, int top);

// Equal cells filling the box (bx, by, bw, bh) with `gap` between them.
CellRect gridCellIn(int index, int cols, int rows, int bx, int by, int bw, int bh, int gap);
int gridIndexIn(int x, int y, int cols, int rows, int bx, int by, int bw, int bh, int gap);

// Info hub: content starts below the tab strip; the "Updated" line sits at the bottom.
constexpr int UI_INFO_TOP = 63;
constexpr int UI_INFO_FOOTER_Y = 228;

// Weather forecast day cards.
CellRect weatherDaysBox();
