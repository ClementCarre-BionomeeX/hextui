#pragma once

#include <fstream>
#include <functional>
#include <vector>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <thread>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

class Buffer {
public:
  std::vector<uint8_t> data;
  size_t absolute_cursor = 0; // Absolute cursor in file
  size_t chunk_size = 1024;   // Default chunk size
  size_t file_size = 0;
  size_t chunk_offset = 0; // Offset of the first loaded byte in the file
  std::string filename;
  size_t current_chunk = 0;
  std::function<void()> render_callback;

private:
  std::thread watcher_thread;
  std::atomic<bool> running{true};
  std::filesystem::file_time_type last_write_time;
  std::mutex buffer_mutex;

public:
  Buffer(const std::string &file, std::function<void()> rcb,
         size_t chunk = 1024)
      : filename(file), chunk_size(chunk), render_callback(rcb) {
    std::ifstream file_stream(filename, std::ios::binary | std::ios::ate);
    if (file_stream) {
      file_size = file_stream.tellg(); // Get total file size
    }
    loadChunk(0); // Load initial chunk

    last_write_time = std::filesystem::last_write_time(filename);
    loadChunk(0);

    // Launch the file-watcher thread
    watcher_thread = std::thread([this] { watchFileChanges(); });
  }

  ~Buffer() {
    running = false;
    if (watcher_thread.joinable())
      watcher_thread.join();
  }
  size_t whichChunkAreWe(size_t position) {
    // TODO: check this formulae
    return position / chunk_size;
  }

  void loadChunk(size_t chunk) {
    std::lock_guard<std::mutex> lock(buffer_mutex);
    size_t offset = (chunk > 1) ? (chunk - 1) * chunk_size : 0;

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

  void checkChunks(size_t new_position, bool force = false) {

    if (new_position >= file_size) {
      new_position = file_size - 1;
    }
    if (new_position < 0) {
      new_position = 0;
    }

    // TODO: first check in which chunck we are after offseting
    auto chunk = whichChunkAreWe(new_position);
    if (chunk != current_chunk || force) {
      // we changed chunk
      current_chunk = chunk;
      loadChunk(chunk);
    }
  }

  void moveLeft(size_t amount = 1) {
    if (absolute_cursor >= amount) {
      absolute_cursor -= amount;
    } else {
      absolute_cursor = 0; // Prevent negative cursor
    }

    checkChunks(absolute_cursor);
  }

  void moveRight(size_t amount = 1) {
    if (absolute_cursor + amount < file_size) {
      absolute_cursor += amount;
    } else {
      absolute_cursor = file_size - 1; // Prevent going beyond EOF
    }

    checkChunks(absolute_cursor);
  }

  size_t getAbsoluteCursor() const { return absolute_cursor; }

  void goHome() {
    absolute_cursor = 0;
    checkChunks(absolute_cursor);
  }
  void goEnd() {
    absolute_cursor = file_size - 1;
    checkChunks(absolute_cursor);
  }
  void reload() { checkChunks(absolute_cursor, true); }

private:
  void watchFileChanges() {
    using namespace std::chrono_literals;

    while (running) {
      std::this_thread::sleep_for(500ms); // Poll every 500ms

      std::error_code ec;
      auto current_write_time = std::filesystem::last_write_time(filename, ec);
      if (ec)
        continue; // file might be temporarily unavailable

      if (current_write_time != last_write_time) {
        last_write_time = current_write_time;
        reload();
        render_callback();
      }
    }
  }
};
