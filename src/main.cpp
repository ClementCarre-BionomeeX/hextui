#include <cmath>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utf8.h>
#include <vector>

#include "buffer.h"

using namespace ftxui;

std::string utf16_to_utf8(uint16_t cu1, uint16_t cu2 = 0) {
  std::u16string utf16;
  if (cu1 >= 0xD800 && cu1 <= 0xDBFF && cu2 >= 0xDC00 && cu2 <= 0xDFFF) {
    utf16 += static_cast<char16_t>(cu1);
    utf16 += static_cast<char16_t>(cu2);
  } else {
    utf16 += static_cast<char16_t>(cu1);
  }

  std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> conv;
  return conv.to_bytes(utf16); // ✅ Safe for FTXUI
}

// TODO: encapsulate commands
// - undo redo
// - last command
// - string representation
// - ...
class Command {};

class hexModel {
public:
  Buffer buffer;
  size_t viewport_size = 0;
  size_t viewport_offset = 0;
  size_t word_size = 4;
  size_t columns = 4;
  size_t move_count = 0;

  // TODO: change to a Command class
  std::string last_command = "";

  // TODO: model responsibility ?
  ScreenInteractive &screen;
  Box content_box_;

  hexModel(const std::string &filename, ScreenInteractive &screen)
      : buffer(filename, [this]() { this->screen.PostEvent(Event::Custom); }),
        screen(screen) {}
};

class HexViewer : public ComponentBase,
                  public std::enable_shared_from_this<HexViewer> {
private:
  hexModel model;
  /*Buffer buffer;*/
  /*size_t viewport_size = 0;*/
  /*ScreenInteractive &screen;*/
  /*size_t viewport_offset = 0;*/
  /**/
  /*size_t word_size = 4;*/
  /*size_t columns = 4;*/
  /**/
  /*size_t move_count = 0;         // Stores the number prefix (e.g., "123")*/
  /*std::string last_command = ""; // Stores the last executed command*/
  /**/
  /*Box model.content_box_;*/

  void adjustViewport() {
    if (model.content_box_.y_max <= model.content_box_.y_min) {
      return; // Box not initialized yet; skip adjustment.
    }

    model.viewport_size = model.content_box_.y_max - model.content_box_.y_min;
    model.viewport_size = model.viewport_size > 1 ? model.viewport_size - 1
                                                  : 1; // Adjust for borders

    size_t number_of_char =
        model.columns * model.word_size * model.viewport_size;
    size_t n = (size_t)std::pow(2, std::log(number_of_char) / std::log(2));

    model.buffer.chunk_size = n;
    model.buffer.reload();

    size_t cursor_abs = model.buffer.getAbsoluteCursor();
    size_t cursor_row = cursor_abs / (model.columns * model.word_size);

    size_t viewport_row_start =
        model.viewport_offset / (model.columns * model.word_size);
    size_t viewport_row_end = viewport_row_start + model.viewport_size;

    if (cursor_row < viewport_row_start) {
      model.viewport_offset = cursor_row * (model.columns * model.word_size);
    } else if (cursor_row >= viewport_row_end) {
      model.viewport_offset =
          cursor_row * (model.columns * model.word_size) -
          (model.viewport_size - 1) * (model.columns * model.word_size);
    }
  }

  std::vector<Element> formatHexRow(size_t start, size_t length) {
    std::vector<Element> row;
    size_t abs_cursor = model.buffer.getAbsoluteCursor();

    for (size_t i = 0; i < length; ++i) {
      size_t abs_pos = start + i;

      // 🛠 Ensure chunk is preloaded before rendering
      if (abs_pos >= model.buffer.chunk_offset + model.buffer.data.size()) {
        model.buffer.checkChunks(abs_pos - (abs_pos % model.buffer.chunk_size));
      }

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

  std::string utf16_to_utf8(uint16_t cu1, uint16_t cu2 = 0) {
    std::u16string utf16;
    if (cu1 >= 0xD800 && cu1 <= 0xDBFF && cu2 >= 0xDC00 && cu2 <= 0xDFFF) {
      utf16 += static_cast<char16_t>(cu1);
      utf16 += static_cast<char16_t>(cu2);
    } else {
      utf16 += static_cast<char16_t>(cu1);
    }

    std::string utf8;
    try {
      utf8::utf16to8(utf16.begin(), utf16.end(), std::back_inserter(utf8));
    } catch (const std::exception &e) {
      // Invalid UTF-16 sequence
      return "Invalid UTF-16";
    }
    return utf8;
  }

  Element formatInspector(size_t index) {
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
      std::memcpy(&cu2,
                  &model.buffer.data[index - model.buffer.chunk_offset + 2], 2);
    }

    if (index + 2 <= model.buffer.file_size) {
      std::memcpy(&u16, &model.buffer.data[index - model.buffer.chunk_offset],
                  2);
      std::memcpy(&i16, &model.buffer.data[index - model.buffer.chunk_offset],
                  2);
    }
    if (index + 4 <= model.buffer.file_size) {
      std::memcpy(&u32, &model.buffer.data[index - model.buffer.chunk_offset],
                  4);
      std::memcpy(&i32, &model.buffer.data[index - model.buffer.chunk_offset],
                  4);
      std::memcpy(&f32, &model.buffer.data[index - model.buffer.chunk_offset],
                  4);
    }
    if (index + 8 <= model.buffer.file_size) {
      std::memcpy(&u64, &model.buffer.data[index - model.buffer.chunk_offset],
                  8);
      std::memcpy(&i64, &model.buffer.data[index - model.buffer.chunk_offset],
                  8);
      std::memcpy(&f64, &model.buffer.data[index - model.buffer.chunk_offset],
                  8);
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

  std::vector<Element> formatUtf8Row(size_t start, size_t length) {
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

public:
  /*HexViewer(const std::string &filename, ScreenInteractive &screen)*/
  /*    : model.buffer(filename, [this]() {
   * this->screen.PostEvent(Event::Custom);
   * }),*/
  /*      screen(screen) {}*/

  HexViewer(const std::string &filename, ScreenInteractive &screen)
      : model(filename, screen) {}

  void run() { model.screen.Loop(shared_from_this()); }

  std::vector<Element> generate_content() {
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
  std::string generate_infobar() {

    size_t abs_cursor = model.buffer.getAbsoluteCursor();
    size_t file_size_display = model.buffer.file_size - 1;

    // 🛠 Status bar elements
    std::ostringstream status;
    status << abs_cursor << " / " << file_size_display;
    return status.str();
  }
  /*std::string generate_filebar() {}*/

  Element Render() override {

    if (model.content_box_.y_max - model.content_box_.y_min <= 0) {
      model.screen.PostEvent(Event::Special("Loading"));
      return vbox({filler(),
                   text("Loading... If you see this message, there is probably "
                        "an error somewhere :)") |
                       bold | center,
                   filler()}) |
             reflect(model.content_box_);
    }

    auto rows = generate_content();
    auto status = generate_infobar();
    adjustViewport();

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
        hbox({text(" File: " + model.buffer.filename + " ") | bold |
              color(Color::Green) | flex}) |
            border,

        // Main UI
        hbox(Elements{
            window(text("Data:") | bold, vbox(rows)) |
                size(WIDTH, EQUAL, viewerwidth) | reflect(model.content_box_),
            /*vbox(rows) | border | size(WIDTH, EQUAL, model.columns * 3 + 3 +
               2),*/
            separator(),
            formatInspector(model.buffer.getAbsoluteCursor()) |
                size(WIDTH, GREATER_THAN, 40) | flex,
        }) | flex,

        // Bottom Status Bar
        hbox(Elements{
            text(" " + command_info.str() + " ") | color(Color::Yellow),
            filler(),
            text(" " + status + " ") | color(Color::Cyan),
        }) | size(HEIGHT, EQUAL, 1) |
            border,
    });
  }

  bool OnEvent(Event event) override {
    bool updated = false;

    if (event == Event::Custom) {
      return true;
    }

    // 🛠 Handle number prefix (1-9)
    if (event.is_character() && event.character()[0] >= '0' &&
        event.character()[0] <= '9') {
      model.move_count = model.move_count * 10 + (event.character()[0] - '0');
      model.last_command = "";
      return true;
    }

    // 🛠 Default model.move amount (1 if no prefix was set)
    size_t amount = (model.move_count > 0) ? model.move_count : 1;
    model.move_count = 0; // Reset after execution

    size_t cursor = model.buffer.getAbsoluteCursor();

    // 🛠 model.Movement commands
    if (event == Event::Character('h') || event == Event::ArrowLeft) {
      model.buffer.moveLeft(amount);
      model.last_command = (amount > 1 ? std::to_string(amount) : "") + "h";
      updated = true;
    }
    if (event == Event::Character('l') || event == Event::ArrowRight) {
      model.buffer.moveRight(amount);
      model.last_command = (amount > 1 ? std::to_string(amount) : "") + "l";
      updated = true;
    }
    if (event == Event::Character('k') || event == Event::ArrowUp) {
      model.buffer.moveLeft(amount * model.columns * model.word_size);
      model.last_command = (amount > 1 ? std::to_string(amount) : "") + "k";
      updated = true;
    }
    if (event == Event::Character('j') || event == Event::ArrowDown) {
      model.buffer.moveRight(amount * model.columns * model.word_size);
      model.last_command = (amount > 1 ? std::to_string(amount) : "") + "j";
      updated = true;
    }

    // 🛠 Implement 'w' (model.Move forward to the next model.word start)
    if (event == Event::Character('w')) {
      for (size_t i = 0; i < amount; ++i) {
        size_t next_word_start =
            cursor + model.word_size - (cursor % model.word_size);
        size_t move_distance = next_word_start - cursor;
        model.buffer.moveRight(move_distance);
      }
      model.last_command = (amount > 1 ? std::to_string(amount) : "") + "w";
      updated = true;
    }

    // 🛠 Implement 'b' (model.Move backward to the previous model.word start)
    if (event == Event::Character('b')) {
      for (size_t i = 0; i < amount; ++i) {
        size_t prev_word_start = (cursor % model.word_size == 0)
                                     ? cursor - model.word_size
                                     : cursor - (cursor % model.word_size);
        size_t move_distance = cursor - prev_word_start;
        model.buffer.moveLeft(move_distance);
      }
      model.last_command = (amount > 1 ? std::to_string(amount) : "") + "b";
      updated = true;
    }

    // 🛠 Implement 'e' (model.Move to the end of the current/next model.word)
    if (event == Event::Character('e')) {
      for (size_t i = 0; i < amount; ++i) {
        size_t current_word_end =
            cursor - (cursor % model.word_size) + model.word_size - 1;
        if (cursor % model.word_size ==
            model.word_size -
                1) { // If already at end, model.move to next model.word end
          current_word_end += model.word_size;
        }
        size_t move_distance = current_word_end - cursor;
        model.buffer.moveRight(move_distance);
      }
      model.last_command = (amount > 1 ? std::to_string(amount) : "") + "e";
      updated = true;
    }

    auto home = Event::Special({27, 91, 72});
    if (event == home) {
      model.buffer.goHome();
      model.last_command = "Home";
      updated = true;
    }

    auto end = Event::Special({27, 91, 70});
    if (event == end) {
      model.buffer.goEnd();
      model.last_command = "End";
      updated = true;
    }

    auto altMinus = Event::Special({27, 45});
    if (event == altMinus) {
      // handle Alt+"-"
      if (model.word_size > amount) {
        model.word_size -= amount;
      } else {
        model.word_size = 1;
      }
      model.last_command = (amount > 1 ? std::to_string(amount) : "") + "Alt -";
      updated = true;
    }

    if (event == Event::Character('-')) {
      // handle plain "-"
      if (model.columns > amount) {

        model.columns -= amount;
      } else {
        model.columns = 1;
      }
      model.last_command = (amount > 1 ? std::to_string(amount) : "") + "-";
      updated = true;
    }

    auto altPlus = Event::Special({27, 43});
    if (event == altPlus) {

      // handle Alt+"+"
      model.word_size += amount;
      model.last_command = (amount > 1 ? std::to_string(amount) : "") + "Alt +";
      updated = true;
    }

    if (event == Event::Character('+')) {
      // handle plain "+"
      model.columns += amount;
      model.last_command = (amount > 1 ? std::to_string(amount) : "") + "+";
      updated = true;
    }

    if (event == Event::Character('r')) {
      model.buffer.reload();
      model.last_command = "r";
      updated = true;
    }

    if (event == Event::Character('q')) {
      // how to quit ??
      model.screen.ExitLoopClosure()();
      return true;
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

  auto screen = ScreenInteractive::Fullscreen();
  auto viewer = std::make_shared<HexViewer>(argv[1], screen);

  viewer->run();

  return 0;
}
