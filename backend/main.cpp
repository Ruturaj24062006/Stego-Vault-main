// #define STB_IMAGE_IMPLEMENTATION
// #define STB_IMAGE_WRITE_IMPLEMENTATION

// #include "httplib.h"
// #include "huffman.h"
// #include "stego.h"
// #include <iostream>
// #include <vector>
// #include <bitset>

// // Helper: Convert Binary String back to Text
// std::string binaryToText(std::string binary) {
//     std::string text = "";
//     for (size_t i = 0; i < binary.length(); i += 8) {
//         if (i + 8 > binary.length()) break;
//         std::string byteStr = binary.substr(i, 8);
//         char ch = (char)std::bitset<8>(byteStr).to_ulong();
//         text += ch;
//     }
//     return text;
// }

// int main() {
//     httplib::Server svr;

//     svr.Options("/(.*)", [](const httplib::Request& req, httplib::Response& res) {
//         res.set_header("Access-Control-Allow-Origin", "*");
//         res.set_header("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
//         res.set_header("Access-Control-Allow-Headers", "Content-Type");
//         res.set_content("", "text/html");
//     });

//     // --- ENCODE (LOCK) ---
//     svr.Post("/encode", [](const httplib::Request& req, httplib::Response& res) {
//         res.set_header("Access-Control-Allow-Origin", "*");
//         if (req.has_file("image") && req.has_file("secret")) {
            
//             // 1. Get Data from RAM (No saving to disk)
//             auto image_file = req.get_file_value("image");
//             auto secret_file = req.get_file_value("secret");

//             // 2. Convert Message to Binary
//             std::string binaryMsg = "";
//             for (char c : secret_file.content) {
//                 binaryMsg += std::bitset<8>(c).to_string();
//             }

//             // 3. Process in RAM
//             Stego stego;
//             // Pass the image content directly!
//             std::string resultImageData = stego.embedData(image_file.content, binaryMsg);

//             if (!resultImageData.empty()) {
//                 // 4. Send back the Result (Directly from RAM)
//                 res.set_content(resultImageData, "image/png");
//                 std::cout << "Success: Encoded without disk storage." << std::endl;
//             } else {
//                 res.status = 500;
//                 res.set_content("Error processing image", "text/plain");
//             }
//         }
//     });

//     // --- DECODE (UNLOCK) ---
//     svr.Post("/decode", [](const httplib::Request& req, httplib::Response& res) {
//         res.set_header("Access-Control-Allow-Origin", "*");
        
//         if (req.has_file("image")) {
//             // 1. Get Data from RAM
//             auto image_file = req.get_file_value("image");

//             // 2. Extract Bits
//             Stego stego;
//             std::string extractedBits = stego.extractData(image_file.content);
            
//             // 3. Convert to Text
//             std::string hiddenText = binaryToText(extractedBits);
            
//             std::cout << "Decoded: " << hiddenText << std::endl;
//             res.set_content(hiddenText, "text/plain");
//         }
//     });

//     std::cout << "Stego-Vault (In-Memory Version) started at http://localhost:8080" << std::endl;
//     svr.listen("0.0.0.0", 8080);
// }

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "httplib.h"
#include "stego.h"
#include <iostream>
#include <bitset>

int main() {
    httplib::Server svr;

    svr.Options("/(.*)", [](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        res.set_header("Access-Control-Expose-Headers", "X-Filename"); // Allow React to see the filename
        res.set_content("", "text/html");
    });

    // --- ENCODE (LOCK FILES OR TEXT) ---
    svr.Post("/encode", [](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        if (req.has_file("image")) {
            auto image_file = req.get_file_value("image");
            std::string secret_filename = "secret.txt";
            std::string secret_content = "";

            // Check if user uploaded a file OR typed text
            if (req.has_file("secret_file")) {
                auto sf = req.get_file_value("secret_file");
                secret_filename = sf.filename;
                secret_content = sf.content;
            } else if (req.has_file("secret_text")) {
                secret_content = req.get_file_value("secret_text").content;
            }

            // Create the Secure Packet: NAME ::: DATA ::: STOP
            std::string payload = secret_filename + ":::SEP:::" + secret_content + ":::STEGO_END:::";
            
            std::string binaryMsg = "";
            for (unsigned char c : payload) binaryMsg += std::bitset<8>(c).to_string();

            Stego stego;
            std::string resultImageData = stego.embedData(image_file.content, binaryMsg);

            res.set_content(resultImageData, "image/png");
        }
    });

    // --- DECODE (UNLOCK MULTIMEDIA) ---
    svr.Post("/decode", [](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Expose-Headers", "X-Filename");

        if (req.has_file("image")) {
            auto image_file = req.get_file_value("image");

            Stego stego;
            std::string extractedPayload = stego.extractData(image_file.content);
            
            // Unpack the data
            size_t sepPos = extractedPayload.find(":::SEP:::");
            if (sepPos != std::string::npos) {
                std::string filename = extractedPayload.substr(0, sepPos);
                std::string content = extractedPayload.substr(sepPos + 9);
                
                // Send filename to React secretly via Headers!
                res.set_header("X-Filename", filename);
                // Send raw binary file content
                res.set_content(content, "application/octet-stream");
            }
        }
    });

    std::cout << "Premium Multimedia Stego-Vault started at http://localhost:8080" << std::endl;
    svr.listen("0.0.0.0", 8080);
}