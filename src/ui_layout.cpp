#include "ui_layout.h"

namespace {
int cellW(int cols) { return (UI_SCREEN_W - 2 * UI_GRID_PAD - (cols - 1) * UI_GRID_GAP) / cols; }
int cellH(int rows, int top) {
  return (UI_SCREEN_H - top - 2 * UI_GRID_PAD - (rows - 1) * UI_GRID_GAP) / rows;
}
}  // namespace

CellRect gridCell(int index, int cols, int rows, int top) {
  const int w = cellW(cols);
  const int h = cellH(rows, top);
  const int col = index % cols;
  const int row = index / cols;
  return {UI_GRID_PAD + col * (w + UI_GRID_GAP), top + UI_GRID_PAD + row * (h + UI_GRID_GAP), w, h};
}

CellRect gridCellIn(int index, int cols, int rows, int bx, int by, int bw, int bh, int gap) {
  const int w = (bw - (cols - 1) * gap) / cols;
  const int h = (bh - (rows - 1) * gap) / rows;
  return {bx + (index % cols) * (w + gap), by + (index / cols) * (h + gap), w, h};
}

int gridIndexIn(int x, int y, int cols, int rows, int bx, int by, int bw, int bh, int gap) {
  for (int i = 0; i < cols * rows; ++i) {
    const CellRect c = gridCellIn(i, cols, rows, bx, by, bw, bh, gap);
    if (x >= c.x && x < c.x + c.w && y >= c.y && y < c.y + c.h) return i;
  }
  return -1;
}

int gridIndexAt(int x, int y, int cols, int rows, int top) {
  for (int i = 0; i < cols * rows; ++i) {
    const CellRect c = gridCell(i, cols, rows, top);
    if (x >= c.x && x < c.x + c.w && y >= c.y && y < c.y + c.h) return i;
  }
  return -1;
}

CellRect weatherDaysBox() { return {8, UI_INFO_TOP + 56, UI_SCREEN_W - 16, 100}; }
