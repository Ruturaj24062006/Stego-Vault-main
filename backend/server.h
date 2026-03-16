#ifndef SERVER_H
#define SERVER_H

#include "httplib.h"
#include "huffman.h"
#include "stego.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

class AppServer {
private:
    static constexpr long long PAYLOAD_HEADER_BITS = 32;
    static constexpr size_t MAX_TEXT_PAYLOAD_BYTES = 200000;
    static constexpr size_t MAX_FILE_PAYLOAD_BYTES = 2000000;
    static constexpr const char* CAPACITY_ERROR_MESSAGE = "The selected image is too small to store this data. Please upload a larger image.";

    httplib::Server svr;
    std::string host;
    int port;

    // FIX: The Compressor is now a persistent member of the server!
    // It will remember the Huffman Tree from the /encode route for the /decode route.
    HuffmanCompressor globalCompressor;

    std::string toLower(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return value;
    }

    bool hasAllowedImageExtension(const std::string& filename) {
        const size_t extensionIndex = filename.find_last_of('.');
        const std::string extension = extensionIndex == std::string::npos ? "" : toLower(filename.substr(extensionIndex));
        return extension == ".png" || extension == ".jpg" || extension == ".jpeg";
    }

    bool hasAllowedImageMimeType(const std::string& contentType) {
        const std::string normalized = toLower(contentType);
        return normalized == "image/png" || normalized == "image/jpeg" || normalized == "image/jpg";
    }

    bool startsWith(const std::string& value, const std::string& prefix) {
        return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
    }

    bool parseFilePayload(const std::string& payload, std::string& filename, std::string& dataUrl) {
        const std::string separator = "|||";
        if (!startsWith(payload, "STEGO_FILE|||")) {
            return false;
        }

        const size_t first = payload.find(separator);
        if (first == std::string::npos) {
            return false;
        }

        const size_t second = payload.find(separator, first + separator.size());
        if (second == std::string::npos) {
            return false;
        }

        filename = payload.substr(first + separator.size(), second - (first + separator.size()));
        dataUrl = payload.substr(second + separator.size());
        return !filename.empty() && !dataUrl.empty();
    }

    size_t estimateBase64DecodedSize(const std::string& encoded) {
        if (encoded.empty()) {
            return 0;
        }

        size_t padding = 0;
        if (encoded.size() >= 1 && encoded[encoded.size() - 1] == '=') {
            padding++;
        }
        if (encoded.size() >= 2 && encoded[encoded.size() - 2] == '=') {
            padding++;
        }
        return (encoded.size() * 3 / 4) - padding;
    }

    bool validateSecretPayload(const std::string& payload, std::string& errorMessage) {
        std::string filename;
        std::string dataUrl;

        if (parseFilePayload(payload, filename, dataUrl)) {
            const size_t comma = dataUrl.find(',');
            const std::string prefix = comma == std::string::npos ? "" : dataUrl.substr(0, comma);
            const std::string base64Data = comma == std::string::npos ? "" : dataUrl.substr(comma + 1);

            if (!startsWith(prefix, "data:") || prefix.find(";base64") == std::string::npos || base64Data.empty()) {
                errorMessage = "Only text or small files are supported.";
                return false;
            }

            if (estimateBase64DecodedSize(base64Data) > MAX_FILE_PAYLOAD_BYTES) {
                errorMessage = "Only text or small files are supported.";
                return false;
            }

            return true;
        }

        if (payload.size() > MAX_TEXT_PAYLOAD_BYTES) {
            errorMessage = "Only text or small files are supported.";
            return false;
        }

        return true;
    }

    void cleanupValidationFiles(const std::string& imagePath, const std::string& resultPath) {
        std::remove(imagePath.c_str());
        std::remove(resultPath.c_str());
    }

    std::string quotePath(const std::string& filePath) {
        return std::string("\"") + filePath + "\"";
    }

    bool fileExists(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        return file.good();
    }

    std::string findValidationScript() {
        const std::string currentScript = "validate_image.py";
        if (fileExists(currentScript)) {
            return currentScript;
        }

        const std::string backendScript = "backend/validate_image.py";
        if (fileExists(backendScript)) {
            return backendScript;
        }

        return "";
    }

    bool parseValidationResult(const std::string& resultPath, std::string& errorMessage) {
        std::ifstream resultFile(resultPath.c_str());
        if (!resultFile) {
            errorMessage = "Image validation service is unavailable on the server.";
            return false;
        }

        std::string result;
        std::getline(resultFile, result);

        const size_t separatorIndex = result.find('|');
        const std::string status = separatorIndex == std::string::npos ? result : result.substr(0, separatorIndex);
        const std::string message = separatorIndex == std::string::npos ? "" : result.substr(separatorIndex + 1);

        if (status == "VALID") {
            return true;
        }

        if (!message.empty()) {
            errorMessage = message;
        } else {
            errorMessage = "Uploaded file is not a valid image.";
        }

        return false;
    }

    bool shouldBypassValidationFailure(const std::string& errorMessage) {
        const std::string normalized = toLower(errorMessage);
        return normalized.find("validation service") != std::string::npos ||
               normalized.find("install pillow") != std::string::npos ||
               normalized.find("image validation failed on the server") != std::string::npos;
    }

    bool runValidationScript(const std::string& scriptPath, const std::string& imagePath, const std::string& resultPath) {
        const std::string arguments = quotePath(scriptPath) + " " + quotePath(imagePath) + " " + quotePath(resultPath);

#ifdef _WIN32
        const std::string quietRedirect = " >nul 2>&1";
        const std::vector<std::string> pythonRunners = {"py -3", "py", "python3", "python"};
#else
        const std::string quietRedirect = " >/dev/null 2>&1";
        const std::vector<std::string> pythonRunners = {"python3", "python"};
#endif

        for (const auto& runner : pythonRunners) {
            std::remove(resultPath.c_str());
            std::system((runner + " " + arguments + quietRedirect).c_str());
            if (fileExists(resultPath)) {
                return true;
            }
        }

        return false;
    }

    bool validateImageUpload(const httplib::MultipartFormData& imageFile, std::string& errorMessage) {
        if (!hasAllowedImageExtension(imageFile.filename) && !hasAllowedImageMimeType(imageFile.content_type)) {
            errorMessage = "Only image files are supported for steganography";
            return false;
        }

        const std::string scriptPath = findValidationScript();
        if (scriptPath.empty()) {
            // Fall back to native C++ decode validation in loadImageFromMemory.
            return true;
        }

        const auto uniqueSuffix = std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count());
        const size_t extensionIndex = imageFile.filename.find_last_of('.');
        std::string extension = extensionIndex == std::string::npos ? "" : imageFile.filename.substr(extensionIndex);
        if (extension.empty()) {
            extension = ".img";
        }

        const std::string imagePath = "stego_upload_" + uniqueSuffix + extension;
        const std::string resultPath = "stego_validation_" + uniqueSuffix + ".txt";

        {
            std::ofstream imageStream(imagePath.c_str(), std::ios::binary);
            if (!imageStream) {
                errorMessage = "Failed to validate uploaded image.";
                cleanupValidationFiles(imagePath, resultPath);
                return false;
            }

            imageStream.write(imageFile.content.data(), static_cast<std::streamsize>(imageFile.content.size()));
            if (!imageStream.good()) {
                errorMessage = "Failed to validate uploaded image.";
                cleanupValidationFiles(imagePath, resultPath);
                return false;
            }
        }

        const bool commandStarted = runValidationScript(scriptPath, imagePath, resultPath);
        bool validationPassed = false;
        if (commandStarted) {
            validationPassed = parseValidationResult(resultPath, errorMessage);
            if (!validationPassed && shouldBypassValidationFailure(errorMessage)) {
                // Python/Pillow validator is optional; final validation happens in C++ image load.
                validationPassed = true;
            }
        } else {
            // If validator could not run, rely on native C++ validation path.
            validationPassed = true;
        }
        cleanupValidationFiles(imagePath, resultPath);
        return validationPassed;
    }

    void setCorsHeaders(httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
    }

    void setupRoutes() {
        svr.Options(".*", [this](const httplib::Request& req, httplib::Response& res) {
            setCorsHeaders(res);
            res.status = 200;
        });

        svr.Post("/encode", [this](const httplib::Request& req, httplib::Response& res) {
            setCorsHeaders(res);

            if (!req.has_file("image") || !req.has_file("message")) {
                res.status = 400;
                res.set_content("Missing image or message", "text/plain");
                return;
            }

            auto image_file = req.get_file_value("image");
            auto message_file = req.get_file_value("message");
            std::string secretMessage = message_file.content;
            std::string validationError;

            if (!validateImageUpload(image_file, validationError)) {
                res.status = 400;
                res.set_content(validationError, "text/plain");
                return;
            }

            if (!validateSecretPayload(secretMessage, validationError)) {
                res.status = 400;
                res.set_content(validationError, "text/plain");
                return;
            }

            ImageStego stego;

            const unsigned char* buffer = reinterpret_cast<const unsigned char*>(image_file.content.c_str());
            if (!stego.loadImageFromMemory(buffer, image_file.content.length())) {
                res.status = 400;
                res.set_content("Uploaded file is not a valid PNG, JPG, or JPEG image.", "text/plain");
                return;
            }

            // Use the persistent global compressor
            std::string compressedBits = globalCompressor.encode(secretMessage);
            const long long requiredBits = static_cast<long long>(compressedBits.length()) + PAYLOAD_HEADER_BITS;
            const long long capacityBits = stego.getCapacityBits();

            if (requiredBits > capacityBits) {
                res.status = 400;
                res.set_content(CAPACITY_ERROR_MESSAGE, "text/plain");
                return;
            }

            if (!stego.encodeData(compressedBits)) {
                res.status = 400;
                res.set_content(CAPACITY_ERROR_MESSAGE, "text/plain");
                return;
            }

            std::string encodedImageBuffer = stego.saveImageToMemory();
            
            res.status = 200;
            res.set_content(encodedImageBuffer, "image/png");
        });

        svr.Post("/decode", [this](const httplib::Request& req, httplib::Response& res) {
            setCorsHeaders(res);

            if (!req.has_file("image")) {
                res.status = 400;
                res.set_content("Missing image", "text/plain");
                return;
            }

            auto image_file = req.get_file_value("image");
            std::string validationError;

            if (!validateImageUpload(image_file, validationError)) {
                res.status = 400;
                res.set_content(validationError, "text/plain");
                return;
            }

            ImageStego stego;

            const unsigned char* buffer = reinterpret_cast<const unsigned char*>(image_file.content.c_str());
            if (!stego.loadImageFromMemory(buffer, image_file.content.length())) {
                res.status = 400;
                res.set_content("Uploaded file is not a valid PNG, JPG, or JPEG image.", "text/plain");
                return;
            }

            std::string extractedBits = stego.decodeData();
            
            // Use the persistent global compressor that remembers the tree!
            std::string decodedMessage = globalCompressor.decode(extractedBits);

            if (decodedMessage.empty()) {
                res.status = 400;
                res.set_content("No hidden data found or image corrupted.", "text/plain");
            } else {
                res.status = 200;
                res.set_content(decodedMessage, "text/plain");
            }
        });
    }

public:
    AppServer(const std::string& hostAddress, int portNumber) {
        host = hostAddress;
        port = portNumber;
        setupRoutes();
    }

    void start() {
        std::cout << "========================================\n";
        std::cout << "🚀 Stego-Vault API Server Running!\n";
        std::cout << "🌐 Listening on: http://" << host << ":" << port << "\n";
        std::cout << "========================================\n";
        svr.listen(host, port);
    }
};

#endif // SERVER_H