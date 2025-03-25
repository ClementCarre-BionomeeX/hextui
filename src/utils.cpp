#include "utils.h"

std::string utf16_to_utf8(uint16_t cu1, uint16_t cu2) {
  std::u16string utf16;
  if (cu1 >= 0xD800 && cu1 <= 0xDBFF && cu2 >= 0xDC00 && cu2 <= 0xDFFF) {
    utf16 += static_cast<char16_t>(cu1);
    utf16 += static_cast<char16_t>(cu2);
  } else {
    utf16 += static_cast<char16_t>(cu1);
  }

  std::string utf8;
  utf8::utf16to8(utf16.begin(), utf16.end(), std::back_inserter(utf8));
  try {
  } catch (const std::exception &e) {
    return "Invalid UTF-16";
  }
  return utf8;
}
