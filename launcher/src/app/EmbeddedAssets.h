#pragma once

#include <span>

namespace black::assets {

std::span<const unsigned char> logoImage();
/// Stand-ins for the game's Small.bin and Big.bin menu fonts until the disc is in place (SIL Open Font License).
std::span<const unsigned char> rowFont();
std::span<const unsigned char> titleFont();
/// The launcher's text in each menu language but English (launcher/assets/translations).
std::span<const unsigned char> translationsPtBr();
std::span<const unsigned char> translationsEs();
std::span<const unsigned char> translationsFr();
std::span<const unsigned char> translationsDe();
std::span<const unsigned char> translationsIt();

}  // namespace black::assets
