#include <cmath>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <utf8.h>

#include "hex_controller.h"
#include "hex_model.h"
#include "hex_view.h"

using namespace ftxui;

class HexViewer : public ComponentBase,
                  public std::enable_shared_from_this<HexViewer> {
private:
  HexModel model;
  HexController controller;
  HexView view;

public:
  HexViewer(const std::string &filename, ScreenInteractive &screen)
      : model(filename, screen), controller(model), view(model) {}

  void run() { model.screen.Loop(shared_from_this()); }

  Element Render() override { return view.render(); }

  bool OnEvent(Event event) override {
    auto updated = controller.processEvent(event);
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
