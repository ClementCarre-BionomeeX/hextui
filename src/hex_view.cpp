#include "hex_view.h"
#include "utils.h"

ftxui::Element HexView::formatInspector(size_t index) {
  // 🛠 Fix: No need to check against model.buffer.data.size()
  if (index >= model.buffer.file_size) {
    return text(
        "Out of bounds"); // Only return this if the cursor is beyond EOF
  }

  uint8_t u8 =
      model.buffer
          .data[index - model.buffer.chunk_offset]; // Adjust for chunk offset
  int8_t i8 = static_cast<int8_t>(u8);
  uint16_t u16 = 0;
  int16_t i16 = 0;
  uint32_t u32 = 0;
  int32_t i32 = 0;
  uint64_t u64 = 0;
  int64_t i64 = 0;
  float f32 = 0.0f;
  double f64 = 0.0;
  char ch = static_cast<char>(u8);
  uint16_t cu1 = 0, cu2 = 0;
  std::memcpy(&cu1, &model.buffer.data[index - model.buffer.chunk_offset], 2);

  bool is_surrogate = (cu1 >= 0xD800 && cu1 <= 0xDBFF);
  if (is_surrogate && index + 4 <= model.buffer.file_size) {
    std::memcpy(&cu2, &model.buffer.data[index - model.buffer.chunk_offset + 2],
                2);
  }

  if (index + 2 <= model.buffer.file_size) {
    std::memcpy(&u16, &model.buffer.data[index - model.buffer.chunk_offset], 2);
    std::memcpy(&i16, &model.buffer.data[index - model.buffer.chunk_offset], 2);
  }
  if (index + 4 <= model.buffer.file_size) {
    std::memcpy(&u32, &model.buffer.data[index - model.buffer.chunk_offset], 4);
    std::memcpy(&i32, &model.buffer.data[index - model.buffer.chunk_offset], 4);
    std::memcpy(&f32, &model.buffer.data[index - model.buffer.chunk_offset], 4);
  }
  if (index + 8 <= model.buffer.file_size) {
    std::memcpy(&u64, &model.buffer.data[index - model.buffer.chunk_offset], 8);
    std::memcpy(&i64, &model.buffer.data[index - model.buffer.chunk_offset], 8);
    std::memcpy(&f64, &model.buffer.data[index - model.buffer.chunk_offset], 8);
  }

  std::string utf8_char = utf16_to_utf8(cu1, cu2);
  return window(text("Inspector:") | bold,
                vbox({
                    text("  uint8: " + std::to_string(u8)),
                    text("   int8: " + std::to_string(i8)),
                    text(" uint16: " + std::to_string(u16)),
                    text("  int16: " + std::to_string(i16)),
                    text(" uint32: " + std::to_string(u32)),
                    text("  int32: " + std::to_string(i32)),
                    text(" uint64: " + std::to_string(u64)),
                    text("  int64: " + std::to_string(i64)),
                    text("float32: " + std::to_string(f32)),
                    text("float64: " + std::to_string(f64)),
                    text("  utf-8: " + std::string(1, ch)),
                    text(" utf-16: " + utf8_char),
                }));
}

std::vector<Element> HexView::formatUtf8Row(size_t start, size_t length) {
  std::vector<Element> row;
  size_t abs_cursor = model.buffer.getAbsoluteCursor();

  for (size_t i = 0; i < length; ++i) {
    size_t abs_pos = start + i;

    if (abs_pos < model.buffer.chunk_offset ||
        abs_pos >= model.buffer.chunk_offset + model.buffer.data.size()) {
      row.push_back(text(".") | color(Color::GrayDark)); // Out-of-bounds
    } else {
      uint8_t byte = model.buffer.data[abs_pos - model.buffer.chunk_offset];
      char display_char =
          (byte >= 32 && byte <= 126) ? byte : '.'; // Printable ASCII or dot
      auto char_element =
          text(std::string(1, display_char)) | color(Color::White);

      // 🛠 Highlight the corresponding UTF-8 character when the cursor is over
      // it
      if (abs_pos == abs_cursor) {
        char_element = char_element | inverted;
      }

      row.push_back(char_element);
    }
  }
  return row;
}

std::vector<Element> HexView::formatHexRow(size_t start, size_t length) {
  std::vector<Element> row;
  size_t abs_cursor = model.buffer.getAbsoluteCursor();

  for (size_t i = 0; i < length; ++i) {
    size_t abs_pos = start + i;

    if (abs_pos < model.buffer.chunk_offset ||
        abs_pos >= model.buffer.chunk_offset + model.buffer.data.size()) {
      row.push_back(text("  ")); // 🛠 Empty space instead of ".."
    } else {
      size_t local_pos = abs_pos - model.buffer.chunk_offset;
      std::ostringstream oss;
      oss << std::setw(2) << std::setfill('0') << std::hex
          << (int)model.buffer.data[local_pos];

      auto byte_element = text(oss.str());

      // Highlight selected byte
      if (abs_pos == abs_cursor) {
        byte_element = byte_element | inverted;
      }

      row.push_back(byte_element);
    }

    // Add extra space every word_size for readability
    if ((i + 1) % model.word_size == 0) {
      row.push_back(text(" "));
    }
  }
  return row;
}

std::vector<Element> HexView::generate_content() {

  std::vector<Element> rows;
  rows.reserve(model.viewport_size);
  for (size_t i = model.viewport_offset;
       i < model.viewport_offset +
               model.viewport_size * (model.columns * model.word_size);
       i += (model.columns * model.word_size)) {
    std::vector<Element> hex_row =
        formatHexRow(i, (model.columns * model.word_size));
    std::vector<Element> utf8_row =
        formatUtf8Row(i, (model.columns * model.word_size));
    hex_row.insert(hex_row.end(), utf8_row.begin(),
                   utf8_row.end()); // Merge both rows
    rows.push_back(hbox(hex_row));
  }
  return rows;
}

std::string HexView::generate_infobar() {

  size_t abs_cursor = model.buffer.getAbsoluteCursor();
  size_t file_size_display = model.buffer.file_size - 1;

  // 🛠 Status bar elements
  std::ostringstream status;
  status << abs_cursor << " / " << file_size_display;
  return status.str();
}

ftxui::Element HexView::render() {

  if (model.content_box_.y_max - model.content_box_.y_min <= 0) {
    model.screen.PostEvent(Event::Special("Loading"));
    return vbox({filler(),
                 text("Loading... If you see this message, there is probably "
                      "an error somewhere :)") |
                     bold | center,
                 filler()}) |
           reflect(model.content_box_);
  }

  model.adjustViewport();

  std::ostringstream command_info;
  if (model.move_count > 0) {
    command_info << model.move_count; // Display number prefix if active
  }
  if (!model.last_command.empty()) {
    command_info << model.last_command; // Show last executed command
  }

  size_t viewerwidth = model.columns * model.word_size * 2 +
                       (model.columns - 1) + 2 +
                       model.columns * model.word_size + 2;
  return vbox(Elements{
      // 🛠 NEW: Top Info Bar with File Name
      window(text("File:") | bold, {text(model.buffer.filename + " ") | bold |
                                    color(Color::Green) | flex}),

      // Main UI
      hbox(Elements{
          window(text("Data:") | bold, vbox(generate_content())) |
              size(WIDTH, EQUAL, viewerwidth) | reflect(model.content_box_),
          separator(),
          formatInspector(model.buffer.getAbsoluteCursor()) |
              size(WIDTH, GREATER_THAN, 40) | flex,
      }) | flex,

      // Bottom Status Bar
      hbox(Elements{
          text(" " + command_info.str() + " ") | color(Color::Yellow),
          filler(),
          text(" " + generate_infobar() + " ") | color(Color::Cyan),
      }) | size(HEIGHT, EQUAL, 1) |
          border,
  });
}
