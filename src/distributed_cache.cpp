#include "../include/distributed_cache.h"
#include <iostream>
#include <sstream>
#include <chrono>

// ==================== CacheMessage ====================

std::string CacheMessage::serialize() const
{
    std::ostringstream oss;
    oss << static_cast<int>(type) << "|"
        << sender_node_id << "|"
        << key << "|"
        << value << "|"
        << timestamp;
    return oss.str();
}

CacheMessage CacheMessage::deserialize(const std::string &data)
{
    CacheMessage msg;
    std::istringstream iss(data);
    std::string token;
    std::vector<std::string> parts;

    while (std::getline(iss, token, '|'))
    {
        parts.push_back(token);
    }

    if (parts.size() >= 5)
    {
        msg.type = static_cast<MessageType>(std::stoi(parts[0]));
        msg.sender_node_id = parts[1];
        msg.key = parts[2];
        msg.value = parts[3];
        msg.timestamp = std::stoi(parts[4]);
    }

    return msg;
}

// ==================== CacheNode ====================

CacheNode::CacheNode(const std::string &id, int p, int capacity)
    : node_id(id), port(p), local_cache(capacity), running(false), message_counter(0)
{
}

CacheNode::~CacheNode()
{
    stop();
}

void CacheNode::start()
{
    running = true;
    std::cout << "[" << node_id << "] Node started on port " << port << std::endl;
}

void CacheNode::stop()
{
    running = false;
    if (message_processor_thread.joinable())
    {
        message_processor_thread.join();
    }
    std::cout << "[" << node_id << "] Node stopped" << std::endl;
}

void CacheNode::addPeerNode(const std::string &peer_id)
{
    peer_nodes.push_back(peer_id);
    std::cout << "[" << node_id << "] Added peer: " << peer_id << std::endl;
}

std::string CacheNode::get(const std::string &key)
{
    try
    {
        std::string value = local_cache.get(key);
        std::cout << "[" << node_id << "] GET " << key << " HIT: " << value << std::endl;
        return value;
    }
    catch (const std::out_of_range &)
    {
        std::cout << "[" << node_id << "] GET " << key << "  MISS" << std::endl;
        throw;
    }
}

void CacheNode::put(const std::string &key, const std::string &value)
{
    // 1. Update local cache
    local_cache.put(key, value);
    std::cout << "[" << node_id << "] PUT " << key << " = " << value << " (local)" << std::endl;

    // 2. Broadcast INVALIDATE message to all peers
    // They will remove this key (simple consistency)
    CacheMessage msg;
    msg.type = MessageType::INVALIDATE;
    msg.sender_node_id = node_id;
    msg.key = key;
    msg.value = "";
    msg.timestamp = message_counter++;

    broadcastMessage(msg);
}

void CacheNode::broadcastMessage(const CacheMessage &msg)
{
    std::lock_guard<std::mutex> lock(queue_mutex);

    for (const auto &peer_id : peer_nodes)
    {
        std::cout << "[" << node_id << "] Broadcasting to " << peer_id
                  << ": " << msg.key << std::endl;
    }

    // In real implementation, would send over network
    // For simulation, we'll store in queue
    message_queue.push(msg);
}

void CacheNode::receiveMessage(const CacheMessage &msg)
{
    std::lock_guard<std::mutex> lock(queue_mutex);

    // Handle message based on type
    if (msg.type == MessageType::INVALIDATE)
    {
        // Remove key from local cache
        try
        {
            // We don't have a delete method, so we'll just track it
            std::cout << "[" << node_id << "] INVALIDATING key: " << msg.key
                      << " (from " << msg.sender_node_id << ")" << std::endl;
            // In real implementation, delete from cache
        }
        catch (...)
        {
            // Key doesn't exist, that's fine
        }
    }
    else if (msg.type == MessageType::PUT)
    {
        // Update local cache
        std::cout << "[" << node_id << "] Received PUT: " << msg.key
                  << " = " << msg.value << std::endl;
        local_cache.put(msg.key, msg.value);
    }
}

void CacheNode::processMessageQueue()
{
    std::lock_guard<std::mutex> lock(queue_mutex);

    while (!message_queue.empty())
    {
        CacheMessage msg = message_queue.front();
        message_queue.pop();
        receiveMessage(msg);
    }
}

// ==================== DistributedCache ====================

DistributedCache::DistributedCache()
{
}

void DistributedCache::addNode(const std::string &node_id, int capacity)
{
    auto node = std::make_shared<CacheNode>(node_id, 5000 + nodes.size(), capacity);
    nodes.push_back(node);
    node->start();

    // Register all existing nodes as peers
    for (auto &existing_node : nodes)
    {
        if (existing_node->getNodeId() != node_id)
        {
            node->addPeerNode(existing_node->getNodeId());
            existing_node->addPeerNode(node_id);
        }
    }
}

void DistributedCache::putToNode(const std::string &node_id, const std::string &key, const std::string &value)
{
    auto node = getNode(node_id);
    if (node)
    {
        node->put(key, value);

        // Simulate network: invalidate on other nodes
        for (auto &other_node : nodes)
        {
            if (other_node->getNodeId() != node_id)
            {
                CacheMessage msg;
                msg.type = MessageType::INVALIDATE;
                msg.sender_node_id = node_id;
                msg.key = key;
                msg.timestamp = 0;
                other_node->receiveMessage(msg);
            }
        }
    }
}

void DistributedCache::getFromNode(const std::string &node_id, const std::string &key)
{
    auto node = getNode(node_id);
    if (node)
    {
        try
        {
            node->get(key);
        }
        catch (const std::out_of_range &)
        {
            std::cout << "[CACHE MISS] Would fetch from database" << std::endl;
        }
    }
}

void DistributedCache::printNetworkStatus()
{
    std::cout << "\n========== Network Status ==========" << std::endl;
    for (const auto &node : nodes)
    {
        std::cout << "Node: " << node->getNodeId()
                  << " | Port: " << node->getPort()
                  << " | Cache Size: " << node->getCacheSize() << std::endl;
    }
    std::cout << "===================================\n"
              << std::endl;
}

void DistributedCache::printAllCaches()
{
    std::cout << "\n========== All Cache Contents ==========" << std::endl;
    for (const auto &node : nodes)
    {
        std::cout << "Node: " << node->getNodeId()
                  << " | Cache Size: " << node->getCacheSize() << std::endl;
    }
    std::cout << "========================================\n"
              << std::endl;
}

std::shared_ptr<CacheNode> DistributedCache::getNode(const std::string &node_id)
{
    for (auto &node : nodes)
    {
        if (node->getNodeId() == node_id)
        {
            return node;
        }
    }
    return nullptr;
}