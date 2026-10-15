#include "TempFolder.h"
#include "core/Sha256.h"

#include <doctest/doctest.h>

#include <string>
#include <string_view>

using black::core::Sha256;

namespace {

std::string hashOf(std::string_view text) {
    Sha256 hash;
    hash.update(std::as_bytes(std::span(text.data(), text.size())));
    return Sha256::toHex(hash.finish());
}

}  // namespace

TEST_CASE("SHA-256 matches the FIPS 180-4 test vectors") {
    CHECK(hashOf("") == "E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855");
    CHECK(hashOf("abc") == "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD");
    CHECK(hashOf("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
          "248D6A61D20638B8E5C026930C3E6039A33CE45964FF2167F6ECEDD419DB06C1");
    CHECK(hashOf(std::string(1'000'000, 'a')) == "CDC76E5C9914FB9281A1C7E284D73E67F1809A48A497200E046D39CCC7112CD0");
}

TEST_CASE("SHA-256 gives the same digest however the input is split") {
    const std::string text(1000, 'x');
    for (const std::size_t split : {std::size_t{0}, std::size_t{1}, std::size_t{55}, std::size_t{56}, std::size_t{63},
                                    std::size_t{64}, std::size_t{65}, std::size_t{999}}) {
        Sha256 hash;
        hash.update(std::as_bytes(std::span(text.data(), split)));
        hash.update(std::as_bytes(std::span(text.data() + split, text.size() - split)));
        CHECK(Sha256::toHex(hash.finish()) == hashOf(text));
    }
}

TEST_CASE("sha256File hashes a file and reports one it cannot open") {
    const black::tests::TempFolder folder;
    folder.write("abc.bin", "abc");
    const auto digest = black::core::sha256File(folder.path() / "abc.bin");
    REQUIRE(digest.ok());
    CHECK(Sha256::toHex(digest.value()) == hashOf("abc"));

    const auto missing = black::core::sha256File(folder.path() / "missing.bin");
    REQUIRE_FALSE(missing.ok());
    CHECK(missing.error().message.find("missing.bin") != std::string::npos);
}
