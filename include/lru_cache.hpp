#ifndef MINIREDIS_LRU_CACHE_HPP
#define MINIREDIS_LRU_CACHE_HPP
#include <algorithm>
#include <string>
#include <unordered_map>
#include <mutex>

struct Node {
    std::string key;
    std::string val;
    Node* prev;
    Node* next;

    Node(std::string k, std::string v) : key(k), val(v), prev(nullptr), next(nullptr) {}
};

class LRUCache {
private:
    int cap;
    std::unordered_map<std::string, Node*> mp;
    Node* head;
    Node* tail;
    std::mutex mtx;

    // Detach node from its current position
    void remove(Node* node) {
        Node* prev_node = node->prev;
        Node* next_node = node->next;
        prev_node->next = next_node;
        next_node->prev = prev_node;
    }

    // Insert node right after dummy head (Most Recently Used position)
    void insert(Node* node) {
        Node* next_node = head->next;
        head->next = node;
        node->prev = head;
        node->next = next_node;
        next_node->prev = node;
    }

public:
    explicit LRUCache(int capacity)
        : cap(std::max(0, capacity)), head(new Node("", "")), tail(new Node("", "")) {
        head->next = tail;
        tail->prev = head;
    }

    LRUCache(const LRUCache&) = delete;
    LRUCache& operator=(const LRUCache&) = delete;

    ~LRUCache() {
        Node* curr = head;
        while (curr != nullptr) {
            Node* next_node = curr->next;
            delete curr;
            curr = next_node;
        }
    }

    bool get(const std::string& key, std::string& out_val) {
        std::unique_lock<std::mutex> lock(mtx);
        auto it = mp.find(key);
        if (it == mp.end()) {
            return false;
        }
        Node* node = it->second;
        remove(node);
        insert(node); 
        out_val = node->val;
        return true;
    }

    void put(const std::string& key, const std::string& val) {
        std::unique_lock<std::mutex> lock(mtx);
        // A zero-capacity shard is valid when the cache capacity is smaller
        // than its shard count. It must never retain an entry.
        if (cap == 0) {
            return;
        }

        auto it = mp.find(key);
        if (it != mp.end()) {
            Node* node = it->second;
            node->val = val;
            remove(node);
            insert(node);
            return;
        }

        if ((int)mp.size() == cap) {
            Node* lru = tail->prev;
            mp.erase(lru->key);
            remove(lru);
            delete lru;
        }

        Node* new_node = new Node(key, val);
        insert(new_node);
        mp[key] = new_node;
    }
};

#endif  // MINIREDIS_LRU_CACHE_HPP
