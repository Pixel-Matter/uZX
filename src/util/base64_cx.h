#pragma once
#include <cstdint>
#include <string>
#include <cstddef>
#include <string_view>
#include <utility>
#include <iostream>
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


constexpr size_t b64DecodedSize(const char* data, const size_t &len) {
    size_t padding = 0;
    if (len > 0 && data[len - 1] == '=') {
        padding++;
    }
    if (len > 1 && data[len - 2] == '=') {
        padding++;
    }
    return (len / 4 * 3) - padding;
}

constexpr bool is_base64(unsigned char c) {
    return (isalnum(c) || (c == '+') || (c == '/'));
}

constexpr std::uint8_t base64_decode_char(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return 0; // Handle padding '='
}

constexpr std::string b64DecodeImpl(const char* input, const size_t &len) {
    size_t i = 0, j = 0;
    std::uint32_t buffer = 0;
    int padding = 0;
    std::string out(b64DecodedSize(input, len), '\0');

    for (; i < len - 1; i += 4, j += 3) {
        buffer = (base64_decode_char(input[i]) << 18) |
                 (base64_decode_char(input[i + 1]) << 12) |
                 (base64_decode_char(input[i + 2]) << 6) |
                  base64_decode_char(input[i + 3]);

        out[j] = (buffer >> 16) & 0xff;
        if (out[i + 2] != '=') {
            out[j + 1] = (buffer >> 8) & 0xff;
        } else {
            padding++;
        }
        if (out[i + 3] != '=') {
            out[j + 2] = buffer & 0xff;
        } else {
            padding++;
        }
    }
    return out.substr(0, j - padding);
}

template <std::size_t N>
constexpr std::string b64DecodeImpl2(const char (&input)[N]) {
    size_t i = 0, j = 0;
    std::uint32_t buffer = 0;
    int padding = 0;
    std::string out(b64DecodedSize(input, N), '\0');

    for (; i < N - 1; i += 4, j += 3) {
        buffer = (base64_decode_char(input[i]) << 18) |
                 (base64_decode_char(input[i + 1]) << 12) |
                 (base64_decode_char(input[i + 2]) << 6) |
                  base64_decode_char(input[i + 3]);

        out[j] = (buffer >> 16) & 0xff;
        if (out[i + 2] != '=') {
            out[j + 1] = (buffer >> 8) & 0xff;
        } else {
            padding++;
        }
        if (out[i + 3] != '=') {
            out[j + 2] = buffer & 0xff;
        } else {
            padding++;
        }
    }
    return out.substr(0, j - padding);
}

}

template <std::size_t N>
constexpr std::string b64Decode(const char (&str64)[N]) {
    return b64DecodeImpl2(str64);
}

namespace {

[[maybe_unused]] void static_test() {
    constexpr char example_encoded[] = "aGVsbG8=";
    constexpr std::size_t inputSize = sizeof(example_encoded);
    static_assert(b64DecodedSize(example_encoded, inputSize) == 5, "Test failed");
    static_assert(b64DecodeImpl(example_encoded, inputSize) == "hello", "Test failed");
}

}

}  // namespace Util
