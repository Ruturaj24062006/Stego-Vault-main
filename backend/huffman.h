#ifndef HUFFMAN_COMPRESSOR_H
#define HUFFMAN_COMPRESSOR_H

#include <iostream>
#include <string>
#include <queue>
#include <unordered_map>

class HuffmanCompressor {
private:
    struct Node {
        char ch;
        int freq;
        Node *left, *right;
        Node(char character, int frequency) {
            ch = character;
            freq = frequency;
            left = right = nullptr;
        }
    };

    struct Compare {
        bool operator()(Node* l, Node* r) {
            return l->freq > r->freq;
        }
    };

    Node* root;
    std::unordered_map<char, std::string> huffmanCodes;

    void generateCodes(Node* node, std::string code) {
        if (!node) return;
        if (!node->left && !node->right) huffmanCodes[node->ch] = code;
        generateCodes(node->left, code + "0");
        generateCodes(node->right, code + "1");
    }

    void destroyTree(Node* node) {
        if (node) {
            destroyTree(node->left);
            destroyTree(node->right);
            delete node;
        }
    }

public:
    HuffmanCompressor() { root = nullptr; }
    ~HuffmanCompressor() { destroyTree(root); }

    std::string encode(const std::string& text) {
        if (text.empty()) return "";

        // FIX: Clean up the old tree if someone encrypts a second time!
        if (root != nullptr) {
            destroyTree(root);
            root = nullptr;
        }
        huffmanCodes.clear();

        std::unordered_map<char, int> freqMap;
        for (char ch : text) freqMap[ch]++;

        std::priority_queue<Node*, std::vector<Node*>, Compare> pq;
        for (auto pair : freqMap) pq.push(new Node(pair.first, pair.second));

        while (pq.size() > 1) {
            Node *left = pq.top(); pq.pop();
            Node *right = pq.top(); pq.pop();
            Node *sumNode = new Node('\0', left->freq + right->freq);
            sumNode->left = left;
            sumNode->right = right;
            pq.push(sumNode);
        }

        root = pq.top();
        generateCodes(root, "");

        std::string encodedString = "";
        for (char ch : text) encodedString += huffmanCodes[ch];

        return encodedString;
    }

    std::string decode(const std::string& encodedText) {
        // If the tree is null, we can't decode anything!
        if (encodedText.empty() || root == nullptr) return "";

        std::string decodedString = "";
        Node* curr = root;

        for (char bit : encodedText) {
            if (bit == '0') curr = curr->left;
            else curr = curr->right;

            if (!curr->left && !curr->right) {
                decodedString += curr->ch;
                curr = root; 
            }
        }
        return decodedString;
    }
};

#endif // HUFFMAN_COMPRESSOR_H