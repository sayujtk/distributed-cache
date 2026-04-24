#ifndef LRU_CACHE_H
#define LRU_CACHE_H

#include <unordered_map>
#include <list>
#include <string>
#include <mutex>
#include <stdexcept>

// Node structure for doubly-linked list
struct Node
{
    std::string key;
    std::string value;
};

class LRUCache
{
private:
    int capacity;

    // Hash map: key → iterator to list node
    std::unordered_map<std::string, std::list<Node>::iterator> cache_map;

    // Doubly-linked list: maintains order of usage
    // Front (begin) = Most Recently Used (MRU)
    // Back (end) = Least Recently Used (LRU)
    std::list<Node> cache_list;

    // Mutex for thread-safe access (we'll use this later)
    std::mutex mtx;

public:
    // Constructor: specify cache capacity
    LRUCache(int cap);

    // Get value by key (returns value, throws if not found)
    std::string get(const std::string &key);

    // Put key-value pair (updates if exists, adds if new)
    void put(const std::string &key, const std::string &value);

    // Delete key from cache (returns true if removed, false if not found)
    bool erase(const std::string &key);

    // Check if key exists
    bool contains(const std::string &key);

    // Get current cache size
    int size() const;

    // Get cache capacity
    int getCapacity() const;

    // Clear entire cache
    void clear();
};

#endif // LRU_CACHE_H