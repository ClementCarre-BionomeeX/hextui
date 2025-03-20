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
  size_t cursor = 0;
  size_t chunk_size = 1024; // Default chunk size
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
      data.resize(chunk_size * 2); // Load two chunks (current + next)
      file.read(reinterpret_cast<char *>(data.data()), chunk_size * 2);
      data.resize(file.gcount()); // Resize to actual read size
      chunk_offset = offset;
    }
  }

  void ensureCursorInBuffer() {
    if (cursor >= data.size()) {
      cursor = data.size() - 1;
    }
  }

  void moveLeft(size_t amount = 1) {
    if (cursor >= amount) {
      cursor -= amount;
    } else if (chunk_offset > 0) {
      loadChunks(chunk_offset - chunk_size);
      cursor = std::min(data.size() - 1, chunk_size - 1);
    }
    ensureCursorInBuffer();
  }

  void moveRight(size_t amount = 1) {
    if (cursor + amount < data.size()) {
      cursor += amount;
    } else if (chunk_offset + data.size() < file_size) {
      loadChunks(chunk_offset + chunk_size);
      cursor = 0;
    }
    ensureCursorInBuffer();
  }

  size_t getAbsoluteCursor() const { return chunk_offset + cursor; }

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
    if (viewport_end > buffer.chunk_offset + buffer.data.size()) {
      buffer.loadChunks(buffer.chunk_offset + buffer.chunk_size);
    }
  }

  std::vector<Element> formatHexRow(size_t start, size_t length) {
    std::vector<Element> row;
    for (size_t i = 0; i < length && (start + i) < buffer.data.size(); ++i) {
      std::ostringstream oss;
      oss << std::setw(2) << std::setfill('0') << std::hex
          << (int)buffer.data[start + i];

      auto byte_element = text(oss.str());
      if (start + i == buffer.cursor) {
        byte_element = byte_element | inverted;
      }

      row.push_back(byte_element);
      row.push_back(text(" "));
      if ((i + 1) % 4 == 0) {
        row.push_back(text(" "));
      }
    }
    return row;
  }

  Element formatInspector(size_t index) {
    if (index >= buffer.data.size())
      return text("Out of bounds");

    uint8_t u8 = buffer.data[index];
    int8_t i8 = static_cast<int8_t>(u8);
    uint16_t u16 = 0;
    int16_t i16 = 0;
    uint32_t u32 = 0;
    int32_t i32 = 0;
    uint64_t u64 = 0;
    int64_t i64 = 0;
    float f32 = 0.0f;
    double f64 = 0.0;

    if (index + 2 <= buffer.data.size()) {
      std::memcpy(&u16, &buffer.data[index], 2);
      std::memcpy(&i16, &buffer.data[index], 2);
    }
    if (index + 4 <= buffer.data.size()) {
      std::memcpy(&u32, &buffer.data[index], 4);
      std::memcpy(&i32, &buffer.data[index], 4);
      std::memcpy(&f32, &buffer.data[index], 4);
    }
    if (index + 8 <= buffer.data.size()) {
      std::memcpy(&u64, &buffer.data[index], 8);
      std::memcpy(&i64, &buffer.data[index], 8);
      std::memcpy(&f64, &buffer.data[index], 8);
    }

    return vbox({
        text("Inspector:") | bold | underlined,
        text("uint8:   " + std::to_string(u8)),
        text("int8:    " + std::to_string(i8)),
        text("uint16:  " + std::to_string(u16)),
        text("int16:   " + std::to_string(i16)),
        text("uint32:  " + std::to_string(u32)),
        text("int32:   " + std::to_string(i32)),
        text("uint64:  " + std::to_string(u64)),
        text("int64:   " + std::to_string(i64)),
        text("float32: " + std::to_string(f32)),
        text("float64: " + std::to_string(f64)),
    });
  }

public:
  HexViewer(const std::string &filename) : buffer(filename) {}

  Element Render() override {
    std::vector<Element> rows;
    for (size_t i = viewport_offset;
         i < viewport_offset + viewport_size * columns; i += columns) {
      rows.push_back(hbox(formatHexRow(i - buffer.chunk_offset, columns)));
    }

    return hbox(
        {vbox(rows) | border | size(WIDTH, EQUAL, columns * 3 + 3 + 2),
         separator(),
         formatInspector(buffer.cursor) | border | size(WIDTH, LESS_THAN, 40)});
  }

  bool OnEvent(Event event) override {
    bool updated = false;

    if (event == Event::Character('h') || event == Event::ArrowLeft) {
      buffer.moveLeft(1);
      updated = true;
    }
    if (event == Event::Character('l') || event == Event::ArrowRight) {
      buffer.moveRight(1);
      updated = true;
    }
    if (event == Event::Character('k') || event == Event::ArrowUp) {
      buffer.moveLeft(columns);
      updated = true;
    }
    if (event == Event::Character('j') || event == Event::ArrowDown) {
      buffer.moveRight(columns);
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
