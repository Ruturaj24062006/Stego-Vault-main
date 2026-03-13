#ifndef SERVER_H
#define SERVER_H

#include "httplib.h"
#include "huffman.h"
#include "stego.h"
#include <iostream>
#include <string>

class AppServer {
private:
    httplib::Server svr;
    std::string host;
    int port;

    // FIX: The Compressor is now a persistent member of the server!
    // It will remember the Huffman Tree from the /encode route for the /decode route.
    HuffmanCompressor globalCompressor;

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

            ImageStego stego;

            // Use the persistent global compressor
            std::string compressedBits = globalCompressor.encode(secretMessage);

            const unsigned char* buffer = reinterpret_cast<const unsigned char*>(image_file.content.c_str());
            if (!stego.loadImageFromMemory(buffer, image_file.content.length())) {
                res.status = 500;
                res.set_content("Failed to load image", "text/plain");
                return;
            }

            if (!stego.encodeData(compressedBits)) {
                res.status = 500;
                res.set_content("Image too small for this message", "text/plain");
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

            ImageStego stego;

            const unsigned char* buffer = reinterpret_cast<const unsigned char*>(image_file.content.c_str());
            if (!stego.loadImageFromMemory(buffer, image_file.content.length())) {
                res.status = 500;
                res.set_content("Failed to load image", "text/plain");
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