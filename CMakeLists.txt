cmake_minimum_required(VERSION 3.10)
project(hextui)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include(FetchContent)

# Fetch and include FTXUI
FetchContent_Declare(
  ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/ftxui.git
  GIT_TAG v5.0.0
)

FetchContent_Declare(
  utf8cpp
  GIT_REPOSITORY https://github.com/nemtrif/utfcpp.git
  GIT_TAG        v3.2.1 # or latest stable version
)

FetchContent_MakeAvailable(utf8cpp)

FetchContent_MakeAvailable(ftxui)

add_executable(hextui src/main.cpp src/buffer.cpp src/hex_model.cpp src/hex_controller.cpp src/hex_view.cpp src/utils.cpp)

target_include_directories(hextui PRIVATE ${utf8cpp_SOURCE_DIR}/source)
target_link_libraries(hextui PRIVATE ftxui::screen ftxui::dom ftxui::component pthread )

install(TARGETS hextui DESTINATION bin)
