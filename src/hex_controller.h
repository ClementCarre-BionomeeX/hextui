#pragma once
#include "hex_model.h"
#include <ftxui/component/event.hpp>

class HexController {
private:
  HexModel &model;

public:
  explicit HexController(HexModel &model);

  bool processEvent(ftxui::Event const &event);
};
