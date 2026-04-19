#ifndef IMAGE_STEGO_H
#define IMAGE_STEGO_H

#include <cstdint>
#include <iostream>
#include <numeric>
#include <random>
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

    uint64_t hashKey(const std::string& key) const {
        uint64_t hash = 1469598103934665603ULL;
        for (unsigned char ch : key) {
            hash ^= ch;
            hash *= 1099511628211ULL;
        }
        return hash;
    }

    size_t chooseStep(size_t capacity, uint64_t seed) const {
        if (capacity <= 1) {
            return 1;
        }

        size_t step = static_cast<size_t>((seed % (capacity - 1)) + 1);
        while (std::gcd(step, capacity) != 1) {
            step = (step + 1) % capacity;
            if (step == 0) {
                step = 1;
            }
        }

        return step;
    }

    size_t chooseStart(size_t capacity, uint64_t seed) const {
        return capacity == 0 ? 0 : static_cast<size_t>(seed % capacity);
    }

    bool writeBitsAtPositions(const std::vector<bool>& bits, const std::string& key) {
        if (!image_data) {
            return false;
        }

        const size_t capacity = static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(channels);
        const size_t requiredBits = bits.size() + 32;
        if (requiredBits > capacity) {
            return false;
        }

        const uint64_t seed = hashKey(key);
        const size_t start = chooseStart(capacity, seed);
        const size_t step = chooseStep(capacity, seed >> 1);

        size_t position = start;

        auto writeHeaderBit = [&](char bit) {
            writeBit(image_data[position], bit);
            position = (position + step) % capacity;
        };

        for (int i = 31; i >= 0; --i) {
            const char headerBit = ((bits.size() >> i) & 1ULL) ? '1' : '0';
            writeHeaderBit(headerBit);
        }

        for (bool bit : bits) {
            writeBit(image_data[position], bit ? '1' : '0');
            position = (position + step) % capacity;
        }

        return true;
    }

    std::vector<bool> readBitsAtPositions(const std::string& key, size_t bitCount) {
        std::vector<bool> bits;
        if (!image_data || bitCount == 0) {
            return bits;
        }

        const size_t capacity = static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(channels);
        if (bitCount + 32 > capacity) {
            return {};
        }

        const uint64_t seed = hashKey(key);
        const size_t start = chooseStart(capacity, seed);
        const size_t step = chooseStep(capacity, seed >> 1);

        size_t position = start;
        size_t payloadLength = 0;

        for (int i = 0; i < 32; ++i) {
            const char bit = readBit(image_data[position]);
            if (bit == '1') {
                payloadLength |= (1ULL << (31 - i));
            }
            position = (position + step) % capacity;
        }

        if (payloadLength == 0 || payloadLength + 32 > capacity || payloadLength != bitCount) {
            return {};
        }

        bits.reserve(payloadLength);
        for (size_t i = 0; i < payloadLength; ++i) {
            bits.push_back(readBit(image_data[position]) == '1');
            position = (position + step) % capacity;
        }

        return bits;
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

    bool embedRandom(const std::vector<bool>& bits, const std::string& key) {
        return writeBitsAtPositions(bits, key);
    }

    std::string extractRandom(const std::string& key) {
        if (!image_data) {
            return "";
        }

        const size_t capacity = static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(channels);
        if (capacity < 32) {
            return "";
        }

        const uint64_t seed = hashKey(key);
        const size_t start = chooseStart(capacity, seed);
        const size_t step = chooseStep(capacity, seed >> 1);

        size_t position = start;
        size_t payloadLength = 0;

        for (int i = 0; i < 32; ++i) {
            const char bit = readBit(image_data[position]);
            if (bit == '1') {
                payloadLength |= (1ULL << (31 - i));
            }
            position = (position + step) % capacity;
        }

        if (payloadLength == 0 || payloadLength + 32 > capacity) {
            return "";
        }

        std::string extractedBinary;
        extractedBinary.reserve(payloadLength);
        for (size_t i = 0; i < payloadLength; ++i) {
            extractedBinary.push_back(readBit(image_data[position]));
            position = (position + step) % capacity;
        }

        return extractedBinary;
    }

    bool encodeData(const std::string& binaryString) {
        std::vector<bool> bits;
        bits.reserve(binaryString.size());
        for (char bit : binaryString) {
            bits.push_back(bit == '1');
        }
        return embedRandom(bits, "");
    }

    std::string decodeData() {
        return extractRandom("");
    }
};

#endif // IMAGE_STEGO_H