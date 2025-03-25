#pragma once
#include "buffer.h"
#include <cmath>
#include <ftxui/component/screen_interactive.hpp>

using namespace ftxui;

struct HexModel {
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

  explicit HexModel(const std::string &filename, ScreenInteractive &screen);

  void adjustViewport();
};
