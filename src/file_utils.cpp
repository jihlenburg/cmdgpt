/**
 * @file file_utils.cpp
 * @brief Implementation of file utilities for multimodal support
 * @author Joern Ihlenburg
 * @date 2025
 *
 * This file implements utilities for reading, validating, and processing
 * various file types including images and PDFs.
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

#include "file_utils.h"
#include "base64.h"
#include "cmdgpt.h"
#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>

namespace cmdgpt
{

/**
 * @brief Magic bytes for file type detection
 */
struct MagicBytes
{
    std::vector<uint8_t> bytes;
    FileType type;
    size_t offset; // Offset in file where magic bytes should appear
};

static const std::vector<MagicBytes> MAGIC_SIGNATURES = {
    // PNG: 89 50 4E 47 0D 0A 1A 0A
    {{0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A}, FileType::PNG, 0},
    // JPEG: FF D8 FF
    {{0xFF, 0xD8, 0xFF}, FileType::JPEG, 0},
    // GIF: 47 49 46 38 37 61 (GIF87a) or 47 49 46 38 39 61 (GIF89a)
    {{0x47, 0x49, 0x46, 0x38, 0x37, 0x61}, FileType::GIF, 0},
    {{0x47, 0x49, 0x46, 0x38, 0x39, 0x61}, FileType::GIF, 0},
    // WebP: 52 49 46 46 ?? ?? ?? ?? 57 45 42 50
    {{0x52, 0x49, 0x46, 0x46}, FileType::WEBP, 0}, // Check RIFF at start
    {{0x57, 0x45, 0x42, 0x50}, FileType::WEBP, 8}, // Check WEBP at offset 8
    // PDF: 25 50 44 46 (%PDF)
    {{0x25, 0x50, 0x44, 0x46}, FileType::PDF, 0}};

FileType detect_file_type(const std::vector<uint8_t>& data)
{
    if (data.empty())
    {
        return FileType::UNKNOWN;
    }

    for (const auto& magic : MAGIC_SIGNATURES)
    {
        if (data.size() >= magic.offset + magic.bytes.size())
        {
            bool match = true;
            for (size_t i = 0; i < magic.bytes.size(); ++i)
            {
                if (data[magic.offset + i] != magic.bytes[i])
                {
                    match = false;
                    break;
                }
            }
            if (match)
            {
                // Special case for WebP: need to check both RIFF and WEBP
                if (magic.type == FileType::WEBP && magic.offset == 0)
                {
                    // Check if WEBP appears at offset 8
                    if (data.size() >= 12 && data[8] == 0x57 && data[9] == 0x45 &&
                        data[10] == 0x42 && data[11] == 0x50)
                    {
                        return FileType::WEBP;
                    }
                }
                else
                {
                    return magic.type;
                }
            }
        }
    }

    return FileType::UNKNOWN;
}

std::string get_mime_type(FileType type)
{
    switch (type)
    {
    case FileType::PNG:
        return "image/png";
    case FileType::JPEG:
        return "image/jpeg";
    case FileType::GIF:
        return "image/gif";
    case FileType::WEBP:
        return "image/webp";
    case FileType::PDF:
        return "application/pdf";
    default:
        return "application/octet-stream";
    }
}

std::string get_extension_from_mime(const std::string& mime_type)
{
    static const std::map<std::string, std::string> mime_to_ext = {{"image/png", "png"},
                                                                   {"image/jpeg", "jpg"},
                                                                   {"image/gif", "gif"},
                                                                   {"image/webp", "webp"},
                                                                   {"application/pdf", "pdf"}};

    auto it = mime_to_ext.find(mime_type);
    return (it != mime_to_ext.end()) ? it->second : "dat";
}

ImageData read_image_file(const std::filesystem::path& path)
{
    // Security: Validate path
    auto canonical_path = std::filesystem::canonical(path);

    if (!std::filesystem::exists(canonical_path))
    {
        throw FileException("Image file does not exist: " + path.string());
    }

    if (!std::filesystem::is_regular_file(canonical_path))
    {
        throw FileException("Path is not a regular file: " + path.string());
    }

    // Read file
    std::ifstream file(canonical_path, std::ios::binary | std::ios::ate);
    if (!file)
    {
        throw FileException("Failed to open image file: " + path.string());
    }

    size_t file_size = file.tellg();
    if (file_size == 0)
    {
        throw ImageValidationException("Image file is empty: " + path.string());
    }

    if (file_size > MAX_IMAGE_SIZE)
    {
        throw ImageValidationException(
            "Image file too large: " + std::to_string(file_size / (1024 * 1024)) +
            "MB (max: " + std::to_string(MAX_IMAGE_SIZE / (1024 * 1024)) + "MB)");
    }

    file.seekg(0);
    std::vector<uint8_t> data(file_size);
    if (!file.read(reinterpret_cast<char*>(data.data()), file_size))
    {
        throw FileException("Failed to read image file: " + path.string());
    }

    // Detect and validate file type
    FileType type = detect_file_type(data);
    if (type != FileType::PNG && type != FileType::JPEG && type != FileType::GIF &&
        type != FileType::WEBP)
    {
        throw ImageValidationException("Unsupported image format. "
                                       "Supported formats: PNG, JPEG, GIF, WEBP");
    }

    ImageData image;
    image.data = std::move(data);
    image.mime_type = get_mime_type(type);
    image.filename = path.filename().string();
    image.size = file_size;

    // TODO: Parse image headers to get dimensions
    // For now, dimensions are optional

    return image;
}

FileData read_pdf_file(const std::filesystem::path& path)
{
    // Security: Validate path
    auto canonical_path = std::filesystem::canonical(path);

    if (!std::filesystem::exists(canonical_path))
    {
        throw FileException("PDF file does not exist: " + path.string());
    }

    if (!std::filesystem::is_regular_file(canonical_path))
    {
        throw FileException("Path is not a regular file: " + path.string());
    }

    // Read file
    std::ifstream file(canonical_path, std::ios::binary | std::ios::ate);
    if (!file)
    {
        throw FileException("Failed to open PDF file: " + path.string());
    }

    size_t file_size = file.tellg();
    if (file_size == 0)
    {
        throw ValidationException("PDF file is empty: " + path.string());
    }

    if (file_size > MAX_PDF_SIZE)
    {
        throw ValidationException(
            "PDF file too large: " + std::to_string(file_size / (1024 * 1024)) +
            "MB (max: " + std::to_string(MAX_PDF_SIZE / (1024 * 1024)) + "MB)");
    }

    file.seekg(0);
    std::vector<uint8_t> data(file_size);
    if (!file.read(reinterpret_cast<char*>(data.data()), file_size))
    {
        throw FileException("Failed to read PDF file: " + path.string());
    }

    // Validate PDF
    if (!validate_pdf(data, MAX_PDF_SIZE))
    {
        throw ValidationException("Invalid PDF file format");
    }

    FileData pdf;
    pdf.data = std::move(data);
    pdf.mime_type = "application/pdf";
    pdf.filename = path.filename().string();
    pdf.size = file_size;
    pdf.type = FileType::PDF;

    return pdf;
}

/**
 * @brief Validate image data format and size
 *
 * @param data Raw image data to validate
 * @param max_size Maximum allowed size in bytes
 * @return true if image is valid, false otherwise
 */
bool validate_image(const std::vector<uint8_t>& data, size_t max_size)
{
    if (data.empty())
    {
        return false;
    }

    if (data.size() > max_size)
    {
        return false;
    }

    FileType type = detect_file_type(data);
    return (type == FileType::PNG || type == FileType::JPEG || type == FileType::GIF ||
            type == FileType::WEBP);
}

/**
 * @brief Validate PDF data format and size
 *
 * @param data Raw PDF data to validate
 * @param max_size Maximum allowed size in bytes
 * @return true if PDF is valid, false otherwise
 */
bool validate_pdf(const std::vector<uint8_t>& data, size_t max_size)
{
    if (data.empty())
    {
        return false;
    }

    if (data.size() > max_size)
    {
        return false;
    }

    // Check PDF header
    if (data.size() < 5)
    {
        return false;
    }

    // Check for %PDF- header
    if (data[0] != 0x25 || data[1] != 0x50 || data[2] != 0x44 || data[3] != 0x46 || data[4] != 0x2D)
    {
        return false;
    }

    // Check for %%EOF at end (with some tolerance for trailing bytes)
    const std::string eof_marker = "%%EOF";
    size_t check_size = std::min(data.size(), size_t(1024));
    size_t start_pos = data.size() > check_size ? data.size() - check_size : 0;

    std::string tail(reinterpret_cast<const char*>(data.data() + start_pos),
                     data.size() - start_pos);

    return tail.find(eof_marker) != std::string::npos;
}

/**
 * @brief Save binary data to file with secure permissions
 *
 * @param data Binary data to save
 * @param path Filesystem path for output file
 * @throws FileException if file cannot be written
 */
void save_file(const std::vector<uint8_t>& data, const std::filesystem::path& path)
{
    // Create parent directory if it doesn't exist
    auto parent = path.parent_path();
    if (!parent.empty() && !std::filesystem::exists(parent))
    {
        std::filesystem::create_directories(parent);
    }

    std::ofstream file(path, std::ios::binary);
    if (!file)
    {
        throw FileException("Failed to create output file: " + path.string());
    }

    if (!file.write(reinterpret_cast<const char*>(data.data()), data.size()))
    {
        throw FileException("Failed to write file: " + path.string());
    }

    // Set secure permissions (owner read/write only)
    std::filesystem::permissions(
        path, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
        std::filesystem::perm_options::replace);
}

/**
 * @brief Generate a unique filename with timestamp
 *
 * @param extension File extension (without dot)
 * @param prefix Filename prefix
 * @return Generated filename with format: prefix_YYYYMMDD_HHMMSS_mmm.extension
 */
std::string generate_timestamp_filename(const std::string& extension, const std::string& prefix)
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << prefix << "_";
    ss << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S");

    // Add milliseconds for uniqueness
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    ss << "_" << std::setfill('0') << std::setw(3) << ms.count();

    if (!extension.empty())
    {
        ss << "." << extension;
    }

    return ss.str();
}

/**
 * @brief Extract data URI images from text and save to files
 *
 * @param text Text containing data URI images
 * @param prefix Prefix for generated filenames
 * @return Vector of saved filenames
 */
std::vector<std::string> extract_and_save_images(const std::string& text, const std::string& prefix)
{
    std::vector<std::string> saved_files;

    // Regular expression to match data URIs
    // Matches: data:image/[type];base64,[base64data]
    // Also handles markdown image syntax: ![alt](data:image/...)
    std::regex data_uri_regex(R"(data:image/(png|jpeg|jpg|gif|webp);base64,([A-Za-z0-9+/]+=*))");

    auto begin = std::sregex_iterator(text.begin(), text.end(), data_uri_regex);
    auto end = std::sregex_iterator();

    int count = 0;
    for (std::sregex_iterator i = begin; i != end; ++i)
    {
        std::smatch match = *i;
        std::string image_type = match[1].str();
        std::string base64_data = match[2].str();

        try
        {
            // Decode base64 data
            std::vector<uint8_t> image_data = base64_decode(base64_data);

            // Validate image
            if (!validate_image(image_data))
            {
                continue; // Skip invalid images
            }

            // Generate filename
            std::string extension =
                (image_type == "jpeg" || image_type == "jpg") ? "jpg" : image_type;
            std::string filename =
                generate_timestamp_filename(extension, prefix + "_" + std::to_string(++count));

            // Save file
            save_file(image_data, filename);
            saved_files.push_back(filename);
        }
        catch (const std::exception& e)
        {
            // Log error but continue processing other images
            // (Requires access to logger, so we'll skip for now)
            continue;
        }
    }

    return saved_files;
}

} // namespace cmdgpt