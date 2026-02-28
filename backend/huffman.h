#ifndef HUFFMAN_H
#define HUFFMAN_H

#include <iostream>
#include <string>
#include <queue>
#include <unordered_map>
#include <vector>

using namespace std;

// A Tree Node
struct Node {
    char ch;
    int freq;
    Node *left, *right;
};

// Comparison object to be used to order the heap
struct compare {
    bool operator()(Node* l, Node* r) {
        return l->freq > r->freq;
    }
};

class Huffman {
public:
    // Store the codes (e.g., 'a' -> "110")
    unordered_map<char, string> huffmanCode;
    Node* root = nullptr;

    // Traverse the Huffman Tree and store codes in a map
    void encode(Node* root, string str) {
        if (root == nullptr)
            return;

        if (!root->left && !root->right) {
            huffmanCode[root->ch] = str;
        }

        encode(root->left, str + "0");
        encode(root->right, str + "1");
    }

    // MAIN FUNCTION: Build Huffman Tree and Compress text
    // (Unit 5 - Greedy Approach using Priority Queue)
    string compress(string text) {
        // Count frequency of appearance of each character
        unordered_map<char, int> freq;
        for (char ch : text) {
            freq[ch]++;
        }

        // Create a priority queue to store live nodes of Huffman tree
        priority_queue<Node*, vector<Node*>, compare> pq;

        // Create a leaf node for each character and add it to the priority queue.
        for (auto pair : freq) {
            Node* z = new Node();
            z->ch = pair.first;
            z->freq = pair.second;
            z->left = z->right = nullptr;
            pq.push(z);
        }

        // Standard Huffman Logic:
        // iterate while size of heap doesn't become 1
        while (pq.size() != 1) {
            // Extract the two minimum freq items from the heap
            Node *left = pq.top(); pq.pop();
            Node *right = pq.top(); pq.pop();

            // Create a new internal node with these two nodes as children
            // and with frequency equal to the sum of the two nodes' frequencies.
            // Add the new node to the priority queue.
            Node* top = new Node();
            top->ch = '\0'; // Internal nodes don't have characters
            top->freq = left->freq + right->freq;
            top->left = left;
            top->right = right;

            pq.push(top);
        }

        // The remaining node is the root node and the tree is complete.
        root = pq.top();

        // Generate the binary codes
        encode(root, "");

        // Construct the encoded string
        string str = "";
        for (char ch : text) {
            str += huffmanCode[ch];
        }

        return str;
    }

    // Helper to print the map (for debugging)
    void printCodes() {
        cout << "\n--- Huffman Codes (Greedy Mapping) ---\n";
        for (auto pair : huffmanCode) {
            cout << pair.first << " : " << pair.second << endl;
        }
        cout << "--------------------------------------\n";
    }
};

#endif