#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "buffer.h"

using namespace ftxui;

class HexViewer : public ComponentBase {
private:
  Buffer buffer;
  size_t viewport_size = 20;
  size_t viewport_offset = 0;

  size_t word_size = 4;
  size_t columns = 4;

  size_t move_count = 0;         // Stores the number prefix (e.g., "123")
  std::string last_command = ""; // Stores the last executed command

  void adjustViewport() {
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

  std::vector<Element> formatHexRow(size_t start, size_t length) {
    std::vector<Element> row;
    size_t abs_cursor = buffer.getAbsoluteCursor();

    for (size_t i = 0; i < length; ++i) {
      size_t abs_pos = start + i;

      // 🛠 Ensure chunk is preloaded before rendering
      if (abs_pos >= buffer.chunk_offset + buffer.data.size()) {
        buffer.checkChunks(abs_pos - (abs_pos % buffer.chunk_size));
      }

      if (abs_pos < buffer.chunk_offset ||
          abs_pos >= buffer.chunk_offset + buffer.data.size()) {
        row.push_back(text("  ")); // 🛠 Empty space instead of ".."
      } else {
        size_t local_pos = abs_pos - buffer.chunk_offset;
        std::ostringstream oss;
        oss << std::setw(2) << std::setfill('0') << std::hex
            << (int)buffer.data[local_pos];

        auto byte_element = text(oss.str());

        // Highlight selected byte
        if (abs_pos == abs_cursor) {
          byte_element = byte_element | inverted;
        }

        row.push_back(byte_element);
      }

      // Add extra space every 4 bytes for readability
      if ((i + 1) % word_size == 0) {
        row.push_back(text(" "));
      }
    }
    return row;
  }

  Element formatInspector(size_t index) {
    // 🛠 Fix: No need to check against buffer.data.size()
    if (index >= buffer.file_size) {
      return text(
          "Out of bounds"); // Only return this if the cursor is beyond EOF
    }

    uint8_t u8 =
        buffer.data[index - buffer.chunk_offset]; // Adjust for chunk offset
    int8_t i8 = static_cast<int8_t>(u8);
    uint16_t u16 = 0;
    int16_t i16 = 0;
    uint32_t u32 = 0;
    int32_t i32 = 0;
    uint64_t u64 = 0;
    int64_t i64 = 0;
    float f32 = 0.0f;
    double f64 = 0.0;

    if (index + 2 <= buffer.file_size) {
      std::memcpy(&u16, &buffer.data[index - buffer.chunk_offset], 2);
      std::memcpy(&i16, &buffer.data[index - buffer.chunk_offset], 2);
    }
    if (index + 4 <= buffer.file_size) {
      std::memcpy(&u32, &buffer.data[index - buffer.chunk_offset], 4);
      std::memcpy(&i32, &buffer.data[index - buffer.chunk_offset], 4);
      std::memcpy(&f32, &buffer.data[index - buffer.chunk_offset], 4);
    }
    if (index + 8 <= buffer.file_size) {
      std::memcpy(&u64, &buffer.data[index - buffer.chunk_offset], 8);
      std::memcpy(&i64, &buffer.data[index - buffer.chunk_offset], 8);
      std::memcpy(&f64, &buffer.data[index - buffer.chunk_offset], 8);
    }

    return vbox({
        text("Inspector:") | bold | underlined,
        text("  uint8: " + std::to_string(u8)),
        text("   int8: " + std::to_string(i8)) | underlined,
        text(" uint16: " + std::to_string(u16)),
        text("  int16: " + std::to_string(i16)) | underlined,
        text(" uint32: " + std::to_string(u32)),
        text("  int32: " + std::to_string(i32)) | underlined,
        text(" uint64: " + std::to_string(u64)),
        text("  int64: " + std::to_string(i64)) | underlined,
        text("float32: " + std::to_string(f32)),
        text("float64: " + std::to_string(f64)) | underlined,
    });
  }

  std::vector<Element> formatUtf8Row(size_t start, size_t length) {
    std::vector<Element> row;
    size_t abs_cursor = buffer.getAbsoluteCursor();

    for (size_t i = 0; i < length; ++i) {
      size_t abs_pos = start + i;

      if (abs_pos < buffer.chunk_offset ||
          abs_pos >= buffer.chunk_offset + buffer.data.size()) {
        row.push_back(text(".") | color(Color::GrayDark)); // Out-of-bounds
      } else {
        uint8_t byte = buffer.data[abs_pos - buffer.chunk_offset];
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

public:
  HexViewer(const std::string &filename) : buffer(filename) {}

  Element Render() override {
    std::vector<Element> rows;
    size_t abs_cursor = buffer.getAbsoluteCursor();
    size_t file_size_display = buffer.file_size - 1;

    for (size_t i = viewport_offset;
         i < viewport_offset + viewport_size * (columns * word_size);
         i += (columns * word_size)) {
      std::vector<Element> hex_row = formatHexRow(i, (columns * word_size));
      std::vector<Element> utf8_row = formatUtf8Row(i, (columns * word_size));
      hex_row.insert(hex_row.end(), utf8_row.begin(),
                     utf8_row.end()); // Merge both rows
      rows.push_back(hbox(hex_row));
    }

    // 🛠 Status bar elements
    std::ostringstream status;
    status << abs_cursor << " / " << file_size_display;

    std::ostringstream command_info;
    if (move_count > 0) {
      command_info << move_count; // Display number prefix if active
    }
    if (!last_command.empty()) {
      command_info << last_command; // Show last executed command
    }

    size_t viewerwidth =
        columns * word_size * 2 + (columns - 1) + 2 + columns * word_size + 2;
    return vbox(Elements{
        // 🛠 NEW: Top Info Bar with File Name
        hbox({text(" File: " + buffer.filename + " ") | bold |
              color(Color::Green)}) |
            border,

        // Main UI
        hbox(Elements{
            vbox(rows) | border | size(WIDTH, EQUAL, viewerwidth),
            /*vbox(rows) | border | size(WIDTH, EQUAL, columns * 3 + 3 + 2),*/
            separator(),
            formatInspector(abs_cursor) | border |
                size(WIDTH, GREATER_THAN, 40) | flex,
        }),

        // Bottom Status Bar
        hbox(Elements{
            text(" " + command_info.str() + " ") | color(Color::Yellow),
            filler(),
            text(" " + status.str() + " ") | color(Color::Cyan),
        }) | size(HEIGHT, EQUAL, 1) |
            border,
    });
  }

  bool OnEvent(Event event) override {
    bool updated = false;

    // 🛠 Handle number prefix (1-9)
    if (event.is_character() && event.character()[0] >= '0' &&
        event.character()[0] <= '9') {
      move_count = move_count * 10 + (event.character()[0] - '0');
      last_command = "";
      return true;
    }

    // 🛠 Default move amount (1 if no prefix was set)
    size_t amount = (move_count > 0) ? move_count : 1;
    move_count = 0; // Reset after execution

    size_t cursor = buffer.getAbsoluteCursor();

    // 🛠 Movement commands
    if (event == Event::Character('h') || event == Event::ArrowLeft) {
      buffer.moveLeft(amount);
      last_command = (amount > 1 ? std::to_string(amount) : "") + "h";
      updated = true;
    }
    if (event == Event::Character('l') || event == Event::ArrowRight) {
      buffer.moveRight(amount);
      last_command = (amount > 1 ? std::to_string(amount) : "") + "l";
      updated = true;
    }
    if (event == Event::Character('k') || event == Event::ArrowUp) {
      buffer.moveLeft(amount * columns * word_size);
      last_command = (amount > 1 ? std::to_string(amount) : "") + "k";
      updated = true;
    }
    if (event == Event::Character('j') || event == Event::ArrowDown) {
      buffer.moveRight(amount * columns * word_size);
      last_command = (amount > 1 ? std::to_string(amount) : "") + "j";
      updated = true;
    }

    // 🛠 Implement 'w' (Move forward to the next word start)
    if (event == Event::Character('w')) {
      for (size_t i = 0; i < amount; ++i) {
        size_t next_word_start = cursor + word_size - (cursor % word_size);
        size_t move_distance = next_word_start - cursor;
        buffer.moveRight(move_distance);
      }
      last_command = (amount > 1 ? std::to_string(amount) : "") + "w";
      updated = true;
    }

    // 🛠 Implement 'b' (Move backward to the previous word start)
    if (event == Event::Character('b')) {
      for (size_t i = 0; i < amount; ++i) {
        size_t prev_word_start = (cursor % word_size == 0)
                                     ? cursor - word_size
                                     : cursor - (cursor % word_size);
        size_t move_distance = cursor - prev_word_start;
        buffer.moveLeft(move_distance);
      }
      last_command = (amount > 1 ? std::to_string(amount) : "") + "b";
      updated = true;
    }

    // 🛠 Implement 'e' (Move to the end of the current/next word)
    if (event == Event::Character('e')) {
      for (size_t i = 0; i < amount; ++i) {
        size_t current_word_end = cursor - (cursor % word_size) + word_size - 1;
        if (cursor % word_size ==
            word_size - 1) { // If already at end, move to next word end
          current_word_end += word_size;
        }
        size_t move_distance = current_word_end - cursor;
        buffer.moveRight(move_distance);
      }
      last_command = (amount > 1 ? std::to_string(amount) : "") + "e";
      updated = true;
    }

    auto home = Event::Special({27, 91, 72});
    if (event == home) {
      buffer.goHome();
      last_command = "Home";
      updated = true;
    }

    auto end = Event::Special({27, 91, 70});
    if (event == end) {
      buffer.goEnd();
      last_command = "End";
      updated = true;
    }

    auto altMinus = Event::Special({27, 45});
    if (event == altMinus) {
      // handle Alt+"-"
      if (word_size > amount) {
        word_size -= amount;
      } else {
        word_size = 1;
      }
      last_command = (amount > 1 ? std::to_string(amount) : "") + "Alt -";
      updated = true;
    }

    if (event == Event::Character('-')) {
      // handle plain "-"
      if (columns > amount) {

        columns -= amount;
      } else {
        columns = 1;
      }
      last_command = (amount > 1 ? std::to_string(amount) : "") + "-";
      updated = true;
    }

    auto altPlus = Event::Special({27, 43});
    if (event == altPlus) {

      // handle Alt+"+"
      word_size += amount;
      last_command = (amount > 1 ? std::to_string(amount) : "") + "Alt +";
      updated = true;
    }

    if (event == Event::Character('+')) {
      // handle plain "+"
      columns += amount;
      last_command = (amount > 1 ? std::to_string(amount) : "") + "+";
      updated = true;
    }

    adjustViewport();
    return updated;
  }
};

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <binary file>\n";
    return 1;
  }

  auto screen = ScreenInteractive::TerminalOutput();
  auto viewer = std::make_shared<HexViewer>(argv[1]);
  screen.Loop(viewer);

  return 0;
}
