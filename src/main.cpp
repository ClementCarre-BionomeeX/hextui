#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

using namespace ftxui;

class Buffer {
public:
  std::vector<uint8_t> data;
  size_t absolute_cursor = 0; // Absolute cursor in file
  size_t chunk_size = 1024;   // Default chunk size
  size_t file_size = 0;
  size_t chunk_offset = 0; // Offset of the first loaded byte in the file
  std::string filename;

  Buffer(const std::string &file, size_t chunk = 1024)
      : filename(file), chunk_size(chunk) {
    std::ifstream file_stream(filename, std::ios::binary | std::ios::ate);
    if (file_stream) {
      file_size = file_stream.tellg(); // Get total file size
    }
    loadChunks(0); // Load initial chunk
  }

  void loadChunks(size_t offset) {
    if (offset >= file_size)
      return;

    std::ifstream file(filename, std::ios::binary);
    if (file) {
      file.seekg(offset);

      // 🛠 Load THREE chunks: previous, current, next (for smooth transitions)
      size_t total_size = chunk_size * 3;
      data.resize(total_size);
      file.read(reinterpret_cast<char *>(data.data()), total_size);
      data.resize(file.gcount()); // Resize to actual read size
      chunk_offset = offset;
    }
  }

  void moveLeft(size_t amount = 1) {
    if (absolute_cursor >= amount) {
      absolute_cursor -= amount;
    } else {
      absolute_cursor = 0; // Prevent negative cursor
    }

    if (absolute_cursor < chunk_offset) {
      loadChunks(absolute_cursor - (absolute_cursor % chunk_size));
    }
  }

  void moveRight(size_t amount = 1) {
    if (absolute_cursor + amount < file_size) {
      absolute_cursor += amount;
    } else {
      absolute_cursor = file_size - 1; // Prevent going beyond EOF
    }

    if (absolute_cursor >= chunk_offset + data.size()) {
      loadChunks(absolute_cursor - (absolute_cursor % chunk_size));
    }
  }

  size_t getAbsoluteCursor() const { return absolute_cursor; }

  void reload() { loadChunks(chunk_offset); }
};

class HexViewer : public ComponentBase {
private:
  Buffer buffer;
  size_t columns = 16;
  size_t viewport_size = 20;
  size_t viewport_offset = 0;

  void adjustViewport() {
    size_t cursor_abs = buffer.getAbsoluteCursor();
    size_t cursor_row = cursor_abs / columns;

    size_t viewport_row_start = viewport_offset / columns;
    size_t viewport_row_end = viewport_row_start + viewport_size;

    if (cursor_row < viewport_row_start) {
      viewport_offset = cursor_row * columns;
    } else if (cursor_row >= viewport_row_end) {
      viewport_offset = cursor_row * columns - (viewport_size - 1) * columns;
    }

    ensureBufferCoversViewport();
  }

  void ensureBufferCoversViewport() {
    size_t viewport_end = viewport_offset + (viewport_size * columns);
    size_t preload_threshold =
        buffer.chunk_offset +
        (buffer.chunk_size * 2); // 🛠 Preload third chunk earlier

    // 🛠 Preload the third chunk BEFORE viewport reaches the second chunk’s end
    if (viewport_end >= preload_threshold) {
      buffer.loadChunks(viewport_end - (viewport_end % buffer.chunk_size));
    }

    // 🛠 Preload previous chunk earlier when moving up
    size_t viewport_start = viewport_offset;
    size_t preload_previous_threshold = buffer.chunk_offset + buffer.chunk_size;

    if (viewport_start < preload_previous_threshold &&
        buffer.chunk_offset > 0) {
      buffer.loadChunks(buffer.chunk_offset - buffer.chunk_size);
    }
  }

  std::vector<Element> formatHexRow(size_t start, size_t length) {
    std::vector<Element> row;
    size_t abs_cursor = buffer.getAbsoluteCursor();

    for (size_t i = 0; i < length; ++i) {
      size_t abs_pos = start + i;

      // 🛠 Ensure chunk is preloaded before rendering
      if (abs_pos >= buffer.chunk_offset + buffer.data.size()) {
        buffer.loadChunks(abs_pos - (abs_pos % buffer.chunk_size));
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

      row.push_back(text(" "));

      // Add extra space every 4 bytes for readability
      if ((i + 1) % 4 == 0) {
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

public:
  HexViewer(const std::string &filename) : buffer(filename) {}

  Element Render() override {
    std::vector<Element> rows;
    size_t abs_cursor = buffer.getAbsoluteCursor();

    for (size_t i = viewport_offset;
         i < viewport_offset + viewport_size * columns; i += columns) {
      rows.push_back(hbox(formatHexRow(i, columns)));
    }

    return hbox({
        vbox(rows) | border | size(WIDTH, EQUAL, columns * 3 + 3 + 2),
        separator(),
        formatInspector(abs_cursor) | border |
            size(WIDTH, LESS_THAN, 40) // 🛠 Fix: Use absolute_cursor directly
    });
  }

  bool OnEvent(Event event) override {
    static size_t move_count = 0; // 🛠 Store number prefix (e.g., "123")
    bool updated = false;

    // 🛠 Handle number prefix (1-9)
    if (event.is_character() && event.character()[0] >= '0' &&
        event.character()[0] <= '9') {
      move_count = move_count * 10 + (event.character()[0] - '0');
      return true; // Don't process move yet
    }

    // 🛠 Default move amount (1 if no prefix was set)
    size_t amount = (move_count > 0) ? move_count : 1;
    move_count = 0; // Reset after execution

    if (event == Event::Character('h') || event == Event::ArrowLeft) {
      buffer.moveLeft(amount);
      updated = true;
    }
    if (event == Event::Character('l') || event == Event::ArrowRight) {
      buffer.moveRight(amount);
      updated = true;
    }
    if (event == Event::Character('k') || event == Event::ArrowUp) {
      buffer.moveLeft(amount * columns);
      ensureBufferCoversViewport();
      updated = true;
    }
    if (event == Event::Character('j') || event == Event::ArrowDown) {
      buffer.moveRight(amount * columns);
      ensureBufferCoversViewport();
      updated = true;
    }

    std::size_t cursor = buffer.getAbsoluteCursor();
    // 🛠 Implement 'w' (Move forward to the beginning of the next word)
    if (event == Event::Character('w')) {
      for (size_t i = 0; i < amount; ++i) {
        size_t next_word_start = cursor + 4 - (cursor % 4);
        size_t move_distance = next_word_start - cursor;
        buffer.moveRight(move_distance);
      }
      updated = true;
    }

    // 🛠 Implement 'b' (Move backward to the beginning of the current/previous
    // word)
    if (event == Event::Character('b')) {
      for (size_t i = 0; i < amount; ++i) {
        size_t prev_word_start =
            (cursor % 4 == 0) ? cursor - 4 : cursor - (cursor % 4);
        size_t move_distance = cursor - prev_word_start;
        buffer.moveLeft(move_distance);
      }
      updated = true;
    }

    // 🛠 Implement 'e' (Move to the end of the current word, or next if already
    // at the end)
    if (event == Event::Character('e')) {
      for (size_t i = 0; i < amount; ++i) {
        size_t current_word_end = cursor - (cursor % 4) + 3;
        if (cursor % 4 == 3) { // If already at the end, move to next word's end
          current_word_end += 4;
        }
        size_t move_distance = current_word_end - cursor;
        buffer.moveRight(move_distance);
      }
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
