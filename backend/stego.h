// #ifndef STEGO_H
// #define STEGO_H

// #include <iostream>
// #include <vector>
// #include <string>
// #include "stb_image.h"
// #include "stb_image_write.h"

// using namespace std;

// class Stego {
// private:
//     // Helper function for writing PNG to memory (RAM) instead of disk
//     static void write_to_mem(void *context, void *data, int size) {
//         std::string *buffer = (std::string *)context;
//         buffer->append((char *)data, size);
//     }

// public:
//     // Helper: Get coordinates in Spiral Order
//     vector<pair<int, int>> getSpiralCoords(int width, int height) {
//         vector<pair<int, int>> coords;
//         int top = 0, bottom = height - 1;
//         int left = 0, right = width - 1;
//         int dir = 0;

//         while (top <= bottom && left <= right) {
//             if (dir == 0) { // Right
//                 for (int i = left; i <= right; ++i) coords.push_back({i, top});
//                 top++;
//             } else if (dir == 1) { // Down
//                 for (int i = top; i <= bottom; ++i) coords.push_back({right, i});
//                 right--;
//             } else if (dir == 2) { // Left
//                 for (int i = right; i >= left; --i) coords.push_back({i, bottom});
//                 bottom--;
//             } else if (dir == 3) { // Up
//                 for (int i = bottom; i >= top; --i) coords.push_back({left, i});
//                 left++;
//             }
//             dir = (dir + 1) % 4;
//         }
//         return coords;
//     }

//     // Embed Data: Takes RAW IMAGE DATA -> Returns NEW RAW IMAGE DATA (No Files)
//     string embedData(const string& rawImageData, const string& binaryData) {
//         int width, height, channels;
        
//         // LOAD FROM MEMORY (RAM)
//         unsigned char* img = stbi_load_from_memory(
//             (unsigned char*)rawImageData.data(), 
//             rawImageData.size(), 
//             &width, &height, &channels, 3
//         );
        
//         if (!img) return "";

//         string dataWithStop = binaryData + "00000000"; 
//         vector<pair<int, int>> path = getSpiralCoords(width, height);
//         int dataIndex = 0;

//         for (auto p : path) {
//             if (dataIndex >= dataWithStop.length()) break;
//             int pixelIndex = (p.second * width + p.first) * 3;
//             unsigned char& blueChannel = img[pixelIndex + 2];
//             blueChannel &= 0xFE; 
//             if (dataWithStop[dataIndex] == '1') blueChannel |= 1; 
//             dataIndex++;
//         }

//         // WRITE TO MEMORY (RAM)
//         string resultImageBuffer = "";
//         stbi_write_png_to_func(write_to_mem, &resultImageBuffer, width, height, 3, img, width * 3);
        
//         stbi_image_free(img);
//         return resultImageBuffer; // Return the PNG file as a string
//     }

//     // Extract Data: Takes RAW IMAGE DATA -> Returns TEXT (No Files)
//     string extractData(const string& rawImageData) {
//         int width, height, channels;
        
//         // LOAD FROM MEMORY (RAM)
//         unsigned char* img = stbi_load_from_memory(
//             (unsigned char*)rawImageData.data(), 
//             rawImageData.size(), 
//             &width, &height, &channels, 3
//         );
        
//         if (!img) return "Error: Could not load image";

//         vector<pair<int, int>> path = getSpiralCoords(width, height);
//         string extractedBits = "";
//         string currentByte = "";

//         for (auto p : path) {
//             int pixelIndex = (p.second * width + p.first) * 3;
//             unsigned char blueChannel = img[pixelIndex + 2];
//             int bit = (blueChannel & 1);
//             extractedBits += (bit ? "1" : "0");
//             currentByte += (bit ? "1" : "0");

//             if (currentByte.length() == 8) {
//                 if (currentByte == "00000000") {
//                     extractedBits = extractedBits.substr(0, extractedBits.length() - 8);
//                     break;
//                 }
//                 currentByte = "";
//             }
//         }

//         stbi_image_free(img);
//         return extractedBits;
//     }
// };

// #endif

#ifndef STEGO_H
#define STEGO_H

#include <iostream>
#include <vector>
#include <string>
#include <bitset>
#include "stb_image.h"
#include "stb_image_write.h"

using namespace std;

class Stego {
private:
    static void write_to_mem(void *context, void *data, int size) {
        std::string *buffer = (std::string *)context;
        buffer->append((char *)data, size);
    }

public:
    vector<pair<int, int>> getSpiralCoords(int width, int height) {
        vector<pair<int, int>> coords;
        int top = 0, bottom = height - 1, left = 0, right = width - 1;
        int dir = 0;
        while (top <= bottom && left <= right) {
            if (dir == 0) { for (int i = left; i <= right; ++i) coords.push_back({i, top}); top++; }
            else if (dir == 1) { for (int i = top; i <= bottom; ++i) coords.push_back({right, i}); right--; }
            else if (dir == 2) { for (int i = right; i >= left; --i) coords.push_back({i, bottom}); bottom--; }
            else if (dir == 3) { for (int i = bottom; i >= top; --i) coords.push_back({left, i}); left++; }
            dir = (dir + 1) % 4;
        }
        return coords;
    }

    string embedData(const string& rawImageData, const string& binaryData) {
        int width, height, channels;
        unsigned char* img = stbi_load_from_memory((unsigned char*)rawImageData.data(), rawImageData.size(), &width, &height, &channels, 3);
        if (!img) return "";

        vector<pair<int, int>> path = getSpiralCoords(width, height);
        int dataIndex = 0;

        for (auto p : path) {
            if (dataIndex >= binaryData.length()) break;
            int pixelIndex = (p.second * width + p.first) * 3;
            img[pixelIndex + 2] = (img[pixelIndex + 2] & 0xFE) | (binaryData[dataIndex] - '0');
            dataIndex++;
        }

        string resultImageBuffer = "";
        stbi_write_png_to_func(write_to_mem, &resultImageBuffer, width, height, 3, img, width * 3);
        stbi_image_free(img);
        return resultImageBuffer;
    }

    // UPGRADED: Reads raw bytes and safely stops at the exact end marker
    string extractData(const string& rawImageData) {
        int width, height, channels;
        unsigned char* img = stbi_load_from_memory((unsigned char*)rawImageData.data(), rawImageData.size(), &width, &height, &channels, 3);
        if (!img) return "";

        vector<pair<int, int>> path = getSpiralCoords(width, height);
        string extractedText = "";
        string currentByte = "";

        for (auto p : path) {
            int pixelIndex = (p.second * width + p.first) * 3;
            currentByte += ((img[pixelIndex + 2] & 1) ? "1" : "0");

            if (currentByte.length() == 8) {
                extractedText += (char)std::bitset<8>(currentByte).to_ulong();
                currentByte = "";
                
                // Safely look for our custom 15-character stop sequence
                if (extractedText.length() >= 15 && extractedText.substr(extractedText.length() - 15) == ":::STEGO_END:::") {
                    extractedText = extractedText.substr(0, extractedText.length() - 15);
                    break;
                }
            }
        }
        stbi_image_free(img);
        return extractedText;
    }
};

#endif