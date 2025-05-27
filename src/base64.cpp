/**
 * @file base64.cpp
 * @brief Implementation of base64 encoding and decoding utilities
 * @author Joern Ihlenburg
 * @date 2025
 *
 * This file implements base64 encoding and decoding functionality
 * for image and file support in cmdgpt.
 */

/*
MIT License

Copyright (c) 2025 Joern Ihlenburg

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "base64.h"
#include <stdexcept>

namespace cmdgpt
{

// Base64 character set
static constexpr const char* base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                            "abcdefghijklmnopqrstuvwxyz"
                                            "0123456789+/";

/**
 * @brief Check if a character is a valid base64 character
 */
static inline bool is_base64(unsigned char c)
{
    return (isalnum(c) || (c == '+') || (c == '/'));
}

std::string base64_encode(const std::vector<uint8_t>& data)
{
    std::string encoded;
    encoded.reserve(((data.size() + 2) / 3) * 4);

    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];

    size_t in_len = data.size();
    const unsigned char* bytes_to_encode = data.data();

    while (in_len--)
    {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3)
        {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; i < 4; i++)
            {
                encoded += base64_chars[char_array_4[i]];
            }
            i = 0;
        }
    }

    if (i)
    {
        for (j = i; j < 3; j++)
        {
            char_array_3[j] = '\0';
        }

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);

        for (j = 0; j < (i + 1); j++)
        {
            encoded += base64_chars[char_array_4[j]];
        }

        while ((i++ < 3))
        {
            encoded += '=';
        }
    }

    return encoded;
}

std::string base64_encode(std::string_view data)
{
    std::vector<uint8_t> bytes(data.begin(), data.end());
    return base64_encode(bytes);
}

std::vector<uint8_t> base64_decode(std::string_view encoded_string)
{
    size_t in_len = encoded_string.size();
    int i = 0;
    int j = 0;
    int in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    std::vector<uint8_t> decoded;

    // Handle empty string
    if (encoded_string.empty())
    {
        return decoded;
    }

    // Pre-validate
    if (!is_valid_base64(encoded_string))
    {
        throw std::invalid_argument("Invalid base64 string");
    }

    while (in_len-- && (encoded_string[in_] != '=') && is_base64(encoded_string[in_]))
    {
        char_array_4[i++] = encoded_string[in_];
        in_++;
        if (i == 4)
        {
            for (i = 0; i < 4; i++)
            {
                char_array_4[i] =
                    static_cast<unsigned char>(std::string(base64_chars).find(char_array_4[i]));
            }

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (i = 0; (i < 3); i++)
            {
                decoded.push_back(char_array_3[i]);
            }
            i = 0;
        }
    }

    if (i)
    {
        for (j = 0; j < i; j++)
        {
            char_array_4[j] =
                static_cast<unsigned char>(std::string(base64_chars).find(char_array_4[j]));
        }

        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);

        for (j = 0; (j < i - 1); j++)
        {
            decoded.push_back(char_array_3[j]);
        }
    }

    return decoded;
}

bool is_valid_base64(std::string_view str)
{
    if (str.empty())
    {
        return true; // Empty string is valid base64
    }

    size_t len = str.length();

    // Check length is multiple of 4
    if (len % 4 != 0)
    {
        return false;
    }

    // Check all characters except padding
    size_t padding_start = len;
    if (len >= 2 && str[len - 1] == '=')
    {
        padding_start = len - 1;
        if (str[len - 2] == '=')
        {
            padding_start = len - 2;
        }
    }

    for (size_t i = 0; i < padding_start; ++i)
    {
        if (!is_base64(static_cast<unsigned char>(str[i])))
        {
            return false;
        }
    }

    return true;
}

} // namespace cmdgpt