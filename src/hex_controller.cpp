#include "hex_controller.h"

bool HexController::processEvent(ftxui::Event const &event) {
  bool updated = false;

  if (event == Event::Custom) {
    return true;
  }

  // 🛠 Handle number prefix (1-9)
  if (event.is_character() && event.character()[0] >= '0' &&
      event.character()[0] <= '9') {
    model.move_count = model.move_count * 10 + (event.character()[0] - '0');
    model.last_command = "";
    return true;
  }

  // 🛠 Default model.move amount (1 if no prefix was set)
  size_t amount = (model.move_count > 0) ? model.move_count : 1;
  model.move_count = 0; // Reset after execution

  size_t cursor = model.buffer.getAbsoluteCursor();

  // 🛠 model.Movement commands
  if (event == Event::Character('h') || event == Event::ArrowLeft) {
    model.buffer.moveLeft(amount);
    model.last_command = (amount > 1 ? std::to_string(amount) : "") + "h";
    updated = true;
  }
  if (event == Event::Character('l') || event == Event::ArrowRight) {
    model.buffer.moveRight(amount);
    model.last_command = (amount > 1 ? std::to_string(amount) : "") + "l";
    updated = true;
  }
  if (event == Event::Character('k') || event == Event::ArrowUp) {
    model.buffer.moveLeft(amount * model.columns * model.word_size);
    model.last_command = (amount > 1 ? std::to_string(amount) : "") + "k";
    updated = true;
  }
  if (event == Event::Character('j') || event == Event::ArrowDown) {
    model.buffer.moveRight(amount * model.columns * model.word_size);
    model.last_command = (amount > 1 ? std::to_string(amount) : "") + "j";
    updated = true;
  }

  // 🛠 Implement 'w' (model.Move forward to the next model.word start)
  if (event == Event::Character('w')) {
    for (size_t i = 0; i < amount; ++i) {
      size_t next_word_start =
          cursor + model.word_size - (cursor % model.word_size);
      size_t move_distance = next_word_start - cursor;
      model.buffer.moveRight(move_distance);
    }
    model.last_command = (amount > 1 ? std::to_string(amount) : "") + "w";
    updated = true;
  }

  // 🛠 Implement 'b' (model.Move backward to the previous model.word start)
  if (event == Event::Character('b')) {
    for (size_t i = 0; i < amount; ++i) {
      size_t prev_word_start = (cursor % model.word_size == 0)
                                   ? cursor - model.word_size
                                   : cursor - (cursor % model.word_size);
      size_t move_distance = cursor - prev_word_start;
      model.buffer.moveLeft(move_distance);
    }
    model.last_command = (amount > 1 ? std::to_string(amount) : "") + "b";
    updated = true;
  }

  // 🛠 Implement 'e' (model.Move to the end of the current/next model.word)
  if (event == Event::Character('e')) {
    for (size_t i = 0; i < amount; ++i) {
      size_t current_word_end =
          cursor - (cursor % model.word_size) + model.word_size - 1;
      if (cursor % model.word_size ==
          model.word_size -
              1) { // If already at end, model.move to next model.word end
        current_word_end += model.word_size;
      }
      size_t move_distance = current_word_end - cursor;
      model.buffer.moveRight(move_distance);
    }
    model.last_command = (amount > 1 ? std::to_string(amount) : "") + "e";
    updated = true;
  }

  auto home = Event::Special({27, 91, 72});
  if (event == home) {
    model.buffer.goHome();
    model.last_command = "Home";
    updated = true;
  }

  auto end = Event::Special({27, 91, 70});
  if (event == end) {
    model.buffer.goEnd();
    model.last_command = "End";
    updated = true;
  }

  auto altMinus = Event::Special({27, 45});
  if (event == altMinus) {
    // handle Alt+"-"
    if (model.word_size > amount) {
      model.word_size -= amount;
    } else {
      model.word_size = 1;
    }
    model.last_command = (amount > 1 ? std::to_string(amount) : "") + "Alt -";
    updated = true;
  }

  if (event == Event::Character('-')) {
    // handle plain "-"
    if (model.columns > amount) {

      model.columns -= amount;
    } else {
      model.columns = 1;
    }
    model.last_command = (amount > 1 ? std::to_string(amount) : "") + "-";
    updated = true;
  }

  auto altPlus = Event::Special({27, 43});
  if (event == altPlus) {

    // handle Alt+"+"
    model.word_size += amount;
    model.last_command = (amount > 1 ? std::to_string(amount) : "") + "Alt +";
    updated = true;
  }

  if (event == Event::Character('+')) {
    // handle plain "+"
    model.columns += amount;
    model.last_command = (amount > 1 ? std::to_string(amount) : "") + "+";
    updated = true;
  }

  if (event == Event::Character('r')) {
    model.buffer.reload();
    model.last_command = "r";
    updated = true;
  }

  if (event == Event::Character('q')) {
    // how to quit ??
    model.screen.ExitLoopClosure()();
    return true;
  }

  return updated;
}

HexController::HexController(HexModel &model) : model(model) {}
