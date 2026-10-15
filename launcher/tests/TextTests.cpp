#include "core/Text.h"

#include <doctest/doctest.h>

using black::core::utf8ToUtf16;

TEST_CASE("UTF-8 decodes to UTF-16") {
    CHECK(utf8ToUtf16("PLAY") == u"PLAY");
    CHECK(utf8ToUtf16("CONFIGURA\xC3\x87\xC3\x95"
                      "ES") == u"CONFIGURAÇÕES");
    CHECK(utf8ToUtf16("\xE2\x82\xAC") == u"€");
    CHECK(utf8ToUtf16("\xF0\x9F\x8E\xAE") == u"\U0001F3AE");  // a surrogate pair
    CHECK(utf8ToUtf16("").empty());
}

TEST_CASE("Malformed UTF-8 becomes U+FFFD and decoding continues") {
    CHECK(utf8ToUtf16("A\xFF"
                      "B") == u"A�B");
    CHECK(utf8ToUtf16("\xC3") == u"�");            // truncated
    CHECK(utf8ToUtf16("\xC0\xAF") == u"��");       // overlong '/'
    CHECK(utf8ToUtf16("\xED\xA0\x80") == u"���");  // an encoded surrogate
}
