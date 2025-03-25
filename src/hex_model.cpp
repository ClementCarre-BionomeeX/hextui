#include "hex_model.h"

void HexModel::adjustViewport() {
  if (content_box_.y_max <= content_box_.y_min) {
    return; // Box not initialized yet; skip adjustment.
  }

  viewport_size = content_box_.y_max - content_box_.y_min;
  viewport_size =
      viewport_size > 1 ? viewport_size - 1 : 1; // Adjust for borders

  size_t number_of_char = columns * word_size * viewport_size;
  size_t n = (size_t)std::pow(2, std::log(number_of_char) / std::log(2));

  buffer.chunk_size = n;
  buffer.reload();

  size_t cursor_abs = buffer.getAbsoluteCursor();
  size_t cursor_row = cursor_abs / (columns * word_size);

  size_t viewport_row_start = viewport_offset / (columns * word_size);
  size_t viewport_row_end = viewport_row_start + viewport_size;

  if (cursor_row < viewport_row_start) {
    viewport_offset = cursor_row * (columns * word_size);
  } else if (cursor_row >= viewport_row_end) {
    viewport_offset = cursor_row * (columns * word_size) -
                      (viewport_size - 1) * (columns * word_size);
  }
}
HexModel::HexModel(const std::string &filename, ScreenInteractive &screen)
    : buffer(filename, [this]() { this->screen.PostEvent(Event::Custom); }),
      screen(screen) {}
