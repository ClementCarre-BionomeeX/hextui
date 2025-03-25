#pragma once

#include <cstdint>
#include <string>
#include <utf8.h>

std::string utf16_to_utf8(uint16_t cu1, uint16_t cu2 = 0);
