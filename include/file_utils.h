/**
 * @file file_utils.h
 * @brief File utilities for image and document support
 * @author Joern Ihlenburg
 * @date 2025
 * @version 0.6.0
 *
 * This file provides utilities for reading, validating, and processing
 * various file types including images and PDFs for multimodal AI support.
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

#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cmdgpt
{

/**
 * @brief Maximum file size for images (20MB as per OpenAI limits)
 */
constexpr size_t MAX_IMAGE_SIZE = 20 * 1024 * 1024;

/**
 * @brief Maximum file size for PDFs (512MB for Assistants API)
 */
constexpr size_t MAX_PDF_SIZE = 512 * 1024 * 1024;

/**
 * @brief Supported file types
 */
enum class FileType
{
    UNKNOWN,
    PNG,
    JPEG,
    GIF,
    WEBP,
    PDF
};

/**
 * @brief Image data structure
 */
struct ImageData
{
    std::vector<uint8_t> data;    ///< Raw image data
    std::string mime_type;        ///< MIME type (e.g., "image/png")
    std::string filename;         ///< Original filename
    size_t size;                  ///< File size in bytes
    std::optional<size_t> width;  ///< Image width (if detected)
    std::optional<size_t> height; ///< Image height (if detected)
};

/**
 * @brief File data structure for generic files
 */
struct FileData
{
    std::vector<uint8_t> data; ///< Raw file data
    std::string mime_type;     ///< MIME type
    std::string filename;      ///< Original filename
    size_t size;               ///< File size in bytes
    FileType type;             ///< Detected file type
};

/**
 * @brief Read an image file from disk
 * @param path Path to the image file
 * @return ImageData structure with file contents
 * @throws FileException if file cannot be read
 * @throws ImageValidationException if file is not a valid image
 */
ImageData read_image_file(const std::filesystem::path& path);

/**
 * @brief Read a PDF file from disk
 * @param path Path to the PDF file
 * @return FileData structure with file contents
 * @throws FileException if file cannot be read
 * @throws ValidationException if file is not a valid PDF
 */
FileData read_pdf_file(const std::filesystem::path& path);

/**
 * @brief Detect file type from magic bytes
 * @param data First few bytes of the file
 * @return Detected file type
 */
FileType detect_file_type(const std::vector<uint8_t>& data);

/**
 * @brief Get MIME type for a file type
 * @param type File type
 * @return MIME type string
 */
std::string get_mime_type(FileType type);

/**
 * @brief Validate image format and size
 * @param data Image data
 * @param max_size Maximum allowed size (default: MAX_IMAGE_SIZE)
 * @return true if valid, false otherwise
 */
bool validate_image(const std::vector<uint8_t>& data, size_t max_size = MAX_IMAGE_SIZE);

/**
 * @brief Validate PDF format and size
 * @param data PDF data
 * @param max_size Maximum allowed size (default: MAX_PDF_SIZE)
 * @return true if valid, false otherwise
 */
bool validate_pdf(const std::vector<uint8_t>& data, size_t max_size = MAX_PDF_SIZE);

/**
 * @brief Save binary data to file
 * @param data Binary data to save
 * @param path Output file path
 * @throws FileException if file cannot be written
 */
void save_file(const std::vector<uint8_t>& data, const std::filesystem::path& path);

/**
 * @brief Generate a timestamped filename
 * @param extension File extension (e.g., "png")
 * @param prefix Optional prefix for the filename
 * @return Generated filename
 */
std::string generate_timestamp_filename(const std::string& extension,
                                        const std::string& prefix = "cmdgpt");

/**
 * @brief Get file extension from MIME type
 * @param mime_type MIME type string
 * @return File extension without dot
 */
std::string get_extension_from_mime(const std::string& mime_type);

/**
 * @brief Extract and save base64 images from text
 * @param text Text that may contain base64 image data URIs
 * @param prefix Filename prefix for saved images
 * @return Vector of saved filenames
 *
 * Looks for patterns like: data:image/png;base64,... or markdown ![alt](data:image/...)
 */
std::vector<std::string> extract_and_save_images(const std::string& text,
                                                 const std::string& prefix = "extracted");

} // namespace cmdgpt

#endif // FILE_UTILS_H