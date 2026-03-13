#ifndef IMAGE_STEGO_H
#define IMAGE_STEGO_H

#include <iostream>
#include <string>
#include <vector>

#include "stb_image.h"
#include "stb_image_write.h"

class ImageStego {
private:
    unsigned char* image_data;
    int width;
    int height;
    int channels;

    void writeBit(unsigned char& byte, char bit) {
        if (bit == '1') byte |= 1;
        else byte &= ~1;
    }

    char readBit(unsigned char byte) {
        return (byte & 1) ? '1' : '0';
    }

    // NEW: Callback function used by stb_image_write to push bytes into our C++ string
    static void stbiWriteCallback(void *context, void *data, int size) {
        std::string* str = reinterpret_cast<std::string*>(context);
        str->append(reinterpret_cast<const char*>(data), size);
    }

public:
    ImageStego() {
        image_data = nullptr;
        width = height = channels = 0;
    }

    ~ImageStego() {
        if (image_data != nullptr) {
            stbi_image_free(image_data);
        }
    }

    bool loadImageFromMemory(const unsigned char* buffer, int length) {
        if (image_data != nullptr) stbi_image_free(image_data);
        image_data = stbi_load_from_memory(buffer, length, &width, &height, &channels, 0);
        return image_data != nullptr;
    }

    long long getCapacityBits() const {
        if (!image_data || width <= 0 || height <= 0 || channels <= 0) {
            return 0;
        }
        return 1LL * width * height * channels;
    }

    // NEW: This writes the encoded PNG data to a memory buffer instead of a file
    std::string saveImageToMemory() {
        std::string buffer;
        if (!image_data) return buffer;
        
        // stbi_write_png_to_func safely writes the PNG formatting directly into our string buffer
        stbi_write_png_to_func(stbiWriteCallback, &buffer, width, height, channels, image_data, width * channels);
        return buffer;
    }

    bool encodeData(const std::string& binaryString) {
        if (!image_data) return false;
        long long maxCapacity = width * height * channels;
        
        std::string lengthHeader = "";
        long long dataLength = binaryString.length();
        for (int i = 31; i >= 0; --i) {
            lengthHeader += ((dataLength >> i) & 1) ? '1' : '0';
        }

        std::string fullPayload = lengthHeader + binaryString;

        if (fullPayload.length() > maxCapacity) return false;

        for (size_t i = 0; i < fullPayload.length(); i++) {
            writeBit(image_data[i], fullPayload[i]);
        }
        return true;
    }

    std::string decodeData() {
        if (!image_data) return "";

        long long dataLength = 0;
        for (int i = 0; i < 32; i++) {
            char bit = readBit(image_data[i]);
            if (bit == '1') {
                dataLength |= (1LL << (31 - i));
            }
        }

        long long maxCapacity = width * height * channels;
        if (dataLength < 0 || dataLength + 32 > maxCapacity) return ""; 

        std::string extractedBinary = "";
        for (long long i = 32; i < 32 + dataLength; i++) {
            extractedBinary += readBit(image_data[i]);
        }
        return extractedBinary;
    }
};

#endif // IMAGE_STEGO_H