/**
 * @file base64.h
 * @brief Base64 encoding and decoding utilities
 * @author Joern Ihlenburg
 * @date 2025
 * @version 0.6.0
 *
 * This file provides base64 encoding and decoding functionality
 * for image and file support in cmdgpt.
 *
 * @copyright Copyright (c) 2025 Joern Ihlenburg
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

#ifndef BASE64_H
#define BASE64_H

#include <string>
#include <string_view>
#include <vector>

namespace cmdgpt
{

/**
 * @brief Encode binary data to base64 string
 * @param data Binary data to encode
 * @return Base64 encoded string
 */
std::string base64_encode(const std::vector<uint8_t>& data);

/**
 * @brief Encode string data to base64 string
 * @param data String data to encode
 * @return Base64 encoded string
 */
std::string base64_encode(std::string_view data);

/**
 * @brief Decode base64 string to binary data
 * @param encoded Base64 encoded string
 * @return Decoded binary data
 * @throws std::invalid_argument if input is not valid base64
 */
std::vector<uint8_t> base64_decode(std::string_view encoded);

/**
 * @brief Check if a string is valid base64
 * @param str String to validate
 * @return true if valid base64, false otherwise
 */
bool is_valid_base64(std::string_view str);

} // namespace cmdgpt

#endif // BASE64_H