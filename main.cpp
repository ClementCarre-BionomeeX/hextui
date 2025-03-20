#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

using namespace ftxui;

class HexViewer : public ComponentBase {
private:
  std::vector<uint8_t> data;
  size_t cursor = 0;
  size_t columns = 16;
  size_t viewport_offset = 0;
  size_t viewport_size = 20;
  int vim_count = 0;

  void loadFile(const std::string &filename) {
    std::ifstream file(filename, std::ios::binary);
    if (file) {
      data.assign((std::istreambuf_iterator<char>(file)),
                  std::istreambuf_iterator<char>());
    }
  }

  std::vector<Element> formatHexRow(size_t start, size_t length) {
    std::vector<Element> row;
    for (size_t i = 0; i < length && (start + i) < data.size(); ++i) {
      std::ostringstream oss;
      oss << std::setw(2) << std::setfill('0') << std::hex
          << (int)data[start + i];

      auto byte_element = text(oss.str());

      // Highlight the selected byte
      if (start + i == cursor) {
        byte_element = byte_element | inverted;
      }

      row.push_back(byte_element);

      // Add **one space** after every byte
      row.push_back(text(" "));

      // Add **a larger gap after every 4 bytes**
      if ((i + 1) % 4 == 0) {
        row.push_back(text(" "));
      }
    }
    return row;
  }

  Element formatInspector(size_t index) {
    if (index >= data.size())
      return text("Out of bounds");

    uint8_t u8 = data[index];
    int8_t i8 = static_cast<int8_t>(u8);
    uint16_t u16 = 0;
    int16_t i16 = 0;
    uint32_t u32 = 0;
    int32_t i32 = 0;
    uint64_t u64 = 0;
    int64_t i64 = 0;
    float f32 = 0.0f;
    double f64 = 0.0;

    if (index + 2 <= data.size()) {
      std::memcpy(&u16, &data[index], 2);
      std::memcpy(&i16, &data[index], 2);
    }
    if (index + 4 <= data.size()) {
      std::memcpy(&u32, &data[index], 4);
      std::memcpy(&i32, &data[index], 4);
      std::memcpy(&f32, &data[index], 4);
    }
    if (index + 8 <= data.size()) {
      std::memcpy(&u64, &data[index], 8);
      std::memcpy(&i64, &data[index], 8);
      std::memcpy(&f64, &data[index], 8);
    }

    return vbox({text("Inspector:") | bold | underlined,
                 text("uint8:   " + std::to_string(u8)),
                 text("int8:    " + std::to_string(i8)),
                 text("uint16:  " + std::to_string(u16)),
                 text("int16:   " + std::to_string(i16)),
                 text("uint32:  " + std::to_string(u32)),
                 text("int32:   " + std::to_string(i32)),
                 text("uint64:  " + std::to_string(u64)),
                 text("int64:   " + std::to_string(i64)),
                 text("float32: " + std::to_string(f32)),
                 text("float64: " + std::to_string(f64))});
  }

public:
  HexViewer(const std::string &filename) { loadFile(filename); }

  Element Render() override {
    std::vector<Element> rows;

    for (size_t i = viewport_offset;
         i < data.size() && i < viewport_offset + viewport_size * columns;
         i += columns) {
      rows.push_back(hbox(formatHexRow(i, columns)));
    }

    return hbox({
        vbox(rows) | border | size(WIDTH, EQUAL, columns * 3 + 3 + 2),
        separator(),
        formatInspector(cursor) | border |
            size(WIDTH, LESS_THAN, 40) // Limit inspector width
    });
  }

  bool OnEvent(Event event) override {
    bool updated = false;

    // Handle number prefixes (e.g., `125j`)
    if (event.is_character()) {
      char c = event.character()[0];
      if (c >= '1' && c <= '9') {
        vim_count = vim_count * 10 + (c - '0'); // Store multi-digit numbers
        return true;
      }
    }

    // Determine move distance (default is 1, unless prefixed)
    int move_amount = vim_count > 0 ? vim_count : 1;
    vim_count = 0; // Reset after executing a motion

    // Vim-style motions with **clamping** and **viewport correction**
    if (event == Event::Character('j') || event == Event::ArrowDown) {
      size_t max_down = (data.size() - cursor) / columns;
      cursor += columns * std::min((size_t)move_amount, max_down);

      // Adjust viewport only if cursor moves below current view
      size_t cursor_row = cursor / columns;
      if (cursor_row >= (viewport_offset / columns) + viewport_size) {
        viewport_offset += columns * move_amount;
      }
      updated = true;
    }
    if (event == Event::Character('k') || event == Event::ArrowUp) {
      size_t max_up = cursor / columns;
      cursor -= columns * std::min((size_t)move_amount, max_up);

      // Adjust viewport only if cursor moves above current view
      size_t cursor_row = cursor / columns;
      if (cursor_row < viewport_offset / columns) {
        viewport_offset -= columns * move_amount;
      }
      updated = true;
    }
    if (event == Event::Character('h') || event == Event::ArrowLeft) {
      if (cursor % columns == 0) {
        // Move to last byte of previous row if not at the very top
        if (cursor >= columns) {
          cursor -= 1;
        }
      } else {
        cursor -= std::min((size_t)move_amount, cursor % columns);
      }
      updated = true;
    }
    if (event == Event::Character('l') || event == Event::ArrowRight) {
      if ((cursor + 1) % columns == 0) {
        // Move to first byte of next row if not at the last row
        if (cursor + 1 < data.size()) {
          cursor += 1;
        }
      } else {
        cursor += std::min((size_t)move_amount,
                           std::min(columns - (cursor % columns) - 1,
                                    data.size() - cursor - 1));
      }
      updated = true;
    }

    // Ensure viewport stays within bounds
    size_t cursor_row = cursor / columns;
    size_t viewport_row_start = viewport_offset / columns;
    size_t viewport_row_end = viewport_row_start + viewport_size;

    if (cursor_row >= viewport_row_end) {
      viewport_offset = cursor_row * columns - (viewport_size - 1) * columns;
      updated = true;
    } else if (cursor_row < viewport_row_start) {
      viewport_offset = cursor_row * columns;
      updated = true;
    }

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
