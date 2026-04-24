#ifndef DISTRIBUTED_CACHE_H
#define DISTRIBUTED_CACHE_H

#include "lru_cache.h"
#include <vector>
#include <string>
#include <thread>
#include <queue>
#include <mutex>
#include <memory>

// Message types for inter-node communication
enum class MessageType
{
    PUT,        // Put key-value pair
    GET,        // Get value by key
    INVALIDATE, // Invalidate key (remove from cache)
    ACK         // Acknowledgment
};

// Message structure for network communication
struct CacheMessage
{
    MessageType type;
    std::string sender_node_id;
    std::string key;
    std::string value;
    int timestamp; // For ordering

    // Convert to string for sending over network
    std::string serialize() const;

    // Parse from string (received from network)
    static CacheMessage deserialize(const std::string &data);
};

// Represents one node in the distributed cache network
class CacheNode
{
private:
    std::string node_id;  // Unique identifier (e.g., "node1")
    int port;             // Port number for this node
    LRUCache local_cache; // The actual LRU cache

    std::vector<std::string> peer_nodes; // Other nodes in network
    std::queue<CacheMessage> message_queue;
    std::mutex queue_mutex;

    bool running;
    std::thread message_processor_thread;

    int message_counter; // For timestamping messages

public:
    CacheNode(const std::string &id, int p, int capacity);
    ~CacheNode();

    // Start this node (begin listening/processing)
    void start();

    // Stop this node
    void stop();

    // Register other nodes in the network
    void addPeerNode(const std::string &peer_id);

    // Local cache operations
    std::string get(const std::string &key);
    void put(const std::string &key, const std::string &value);

    // Network operations
    void broadcastMessage(const CacheMessage &msg);
    void receiveMessage(const CacheMessage &msg);
    void processMessageQueue();

    // Get node info
    std::string getNodeId() const { return node_id; }
    int getPort() const { return port; }
    int getCacheSize() const { return local_cache.size(); }

    // Delete key locally (no network broadcast)
    bool eraseLocal(const std::string &key);
};

// Manages the entire distributed cache network
class DistributedCache
{
private:
    std::vector<std::shared_ptr<CacheNode>> nodes;
    std::mutex network_mutex;

public:
    DistributedCache();

    // Create a new node and add to network
    void addNode(const std::string &node_id, int capacity);

    // Simulate a request to specific node
    void putToNode(const std::string &node_id, const std::string &key, const std::string &value);
    void getFromNode(const std::string &node_id, const std::string &key);

    // Get statistics
    void printNetworkStatus();
    void printAllCaches();

    // Get node by ID
    std::shared_ptr<CacheNode> getNode(const std::string &node_id);

    // Delete key from specific node + invalidate peers
    bool delFromNode(const std::string &node_id, const std::string &key);
};

#endif // DISTRIBUTED_CACHE_H