#pragma once

#include "hex_model.h"
#include <cmath>
#include <ftxui/dom/elements.hpp>
#include <utf8.h>

class HexView {
private:
  HexModel &model;

  Element formatInspector(size_t index);
  std::vector<Element> formatUtf8Row(size_t start, size_t length);
  std::vector<Element> formatHexRow(size_t start, size_t length);

public:
  explicit HexView(HexModel &model) : model(model) {}

  std::vector<Element> generate_content();
  std::string generate_infobar();
  Element render();
};
