#pragma once
#include <cstdint>
#include <string>
#include <cstddef>
#include <string>

namespace MoTool::Util {

namespace {

constexpr inline static char B64chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

constexpr inline static int B64index[256] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  62, 63, 62, 62, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 0,  0,  0,  0,  0,  0,
    0,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 0,  0,  0,  0,  63,
    0,  26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51
};

template <std::size_t N>
constexpr size_t b64DecodedSize(const char (&input)[N]) {
    size_t padding = 0;
    if (N > 0 && input[N - 1] == '=') {
        padding++;
    }
    if (N > 1 && input[N - 2] == '=') {
        padding++;
    }
    return (N / 4 * 3) - padding;
}

constexpr bool is_base64(unsigned char c) {
    return (isalnum(c) || (c == '+') || (c == '/'));
}

constexpr std::uint32_t b64DecodeChar(char c) {
    if (c >= 'A' && c <= 'Z') return static_cast<std::uint32_t>(c - 'A');
    if (c >= 'a' && c <= 'z') return static_cast<std::uint32_t>(c - 'a' + 26);
    if (c >= '0' && c <= '9') return static_cast<std::uint32_t>(c - '0' + 52);
    if (c == '+') return 62;
    if (c == '/') return 63;
    return 0; // Handle padding '='
}

}  // namespace

template <std::size_t N>
constexpr std::string b64Decode(const char (&input)[N]) {
    size_t i = 0, j = 0, padding = 0;
    std::uint32_t buffer = 0;
    std::string out(b64DecodedSize(input), '\0');

    for (; i < N - 1; i += 4, j += 3) {
        buffer = (b64DecodeChar(input[i]) << 18) |
                 (b64DecodeChar(input[i + 1]) << 12) |
                 (b64DecodeChar(input[i + 2]) << 6) |
                  b64DecodeChar(input[i + 3]);

        out[j] = static_cast<char>((buffer >> 16) & 0xff);
        if (out[i + 2] != '=') {
            out[j + 1] = static_cast<char>((buffer >> 8) & 0xff);
        } else {
            padding++;
        }
        if (out[i + 3] != '=') {
            out[j + 2] = static_cast<char>(buffer & 0xff);
        } else {
            padding++;
        }
    }
    return out.substr(0, j - padding);
}

namespace {

[[maybe_unused]] static void staticTest() {
    constexpr char example_encoded[] = "aGVsbG8=";
    static_assert(b64DecodedSize(example_encoded) == 5, "Test failed");
    static_assert(b64Decode(example_encoded) == "hello", "Test failed");
}

}

}  // namespace Util
