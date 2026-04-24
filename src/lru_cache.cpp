#include "../include/lru_cache.h"

// Constructor
LRUCache::LRUCache(int cap) : capacity(cap)
{
    if (cap <= 0)
    {
        throw std::invalid_argument("Capacity must be positive");
    }
}

// GET operation: O(1)
std::string LRUCache::get(const std::string &key)
{
    std::lock_guard<std::mutex> lock(mtx); // Thread-safe lock

    // Check if key exists
    if (cache_map.find(key) == cache_map.end())
    {
        throw std::out_of_range("Key not found: " + key);
    }

    // Get iterator to node in list
    auto node_iter = cache_map[key];

    // Move this node to FRONT (most recently used)
    // splice: move node_iter to front of list
    cache_list.splice(cache_list.begin(), cache_list, node_iter);

    // Return the value
    return node_iter->value;
}

// PUT operation: O(1)
void LRUCache::put(const std::string &key, const std::string &value)
{
    std::lock_guard<std::mutex> lock(mtx); // Thread-safe lock

    // Case 1: Key already exists
    if (cache_map.find(key) != cache_map.end())
    {
        // Update value
        auto node_iter = cache_map[key];
        node_iter->value = value;

        // Move to FRONT (most recently used)
        cache_list.splice(cache_list.begin(), cache_list, node_iter);
        return;
    }

    // Case 2: New key
    // Create new node and add to FRONT
    cache_list.push_front({key, value});

    // Add to hash map (iterator points to front node)
    cache_map[key] = cache_list.begin();

    // Case 3: Cache is full, evict LRU (from BACK)
    if (cache_map.size() > static_cast<size_t>(capacity))
    {
        // Get the key at BACK (least recently used)
        std::string lru_key = cache_list.back().key;

        // Remove from hash map
        cache_map.erase(lru_key);

        // Remove from list
        cache_list.pop_back();
    }
}

// Delete key from cache: O(1)
bool LRUCache::erase(const std::string &key)
{
    std::lock_guard<std::mutex> lock(mtx);

    auto it = cache_map.find(key);
    if (it == cache_map.end())
    {
        return false;
    }

    // erase from list using stored iterator
    cache_list.erase(it->second);

    // erase from map
    cache_map.erase(it);

    return true;
}

// Check if key exists
bool LRUCache::contains(const std::string &key)
{
    std::lock_guard<std::mutex> lock(mtx);
    return cache_map.find(key) != cache_map.end();
}

// Get current size
int LRUCache::size() const
{
    return cache_map.size();
}

// Get capacity
int LRUCache::getCapacity() const
{
    return capacity;
}

// Clear cache
void LRUCache::clear()
{
    std::lock_guard<std::mutex> lock(mtx);
    cache_map.clear();
    cache_list.clear();
}