#pragma once

#include <functional>
#include <vector>

#include <atomic>
#include <filesystem>
#include <thread>

#include <cstdint>
#include <cstring>
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
  explicit Buffer(const std::string &file, std::function<void()> rcb,
                  size_t chunk = 1024);

  ~Buffer();
  size_t whichChunkAreWe(size_t position);

  void loadChunk(size_t chunk);
  void checkChunks(size_t new_position, bool force = false);
  size_t getAbsoluteCursor() const;
  void moveLeft(size_t amount = 1);
  void moveRight(size_t amount = 1);
  void goHome();
  void goEnd();
  void reload();

private:
  void watchFileChanges();
};
