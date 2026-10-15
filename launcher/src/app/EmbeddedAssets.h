#pragma once

#include <span>

namespace black::assets {

std::span<const unsigned char> logoImage();
/// Stand-ins for the game's Small.bin and Big.bin menu fonts until the disc is in place (SIL Open Font License).
std::span<const unsigned char> rowFont();
std::span<const unsigned char> titleFont();

}  // namespace black::assets
