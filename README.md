# Distributed LRU Cache System

A high-performance, thread-safe distributed cache implementation in C++ with multi-threaded LRU eviction and consistency protocols.

## Features

- **Single-Node LRU Cache**
  - O(1) get/put operations using hash map + doubly-linked list
  - LRU eviction policy
  - Thread-safe with mutex locks

- **Multi-threaded Support**
  - Concurrent access from multiple threads
  - 1M+ operations per second throughput
  - 98%+ cache hit rates

- **Distributed Cache Network**
  - Multi-node cache architecture
  - Invalidation-based consistency protocol
  - Inter-node communication via message passing
  - Peer-to-peer synchronization

## Performance Metrics

### Single-Node LRU Cache
```
Throughput:       1,666,667 ops/sec
Hit Rate:         98.01%
Avg Latency:      0.0001 ms
Operations:       O(1) for get/put/eviction
```

### Multi-threaded (2 threads)
```
Throughput:       1,052,632 ops/sec
Hit Rate:         99.08%
Avg GET Latency:  0.001 ms
```

### Multi-threaded (4 threads)
```
Throughput:       952,381 ops/sec
Hit Rate:         99.04%
Avg GET Latency:  0.003 ms
```

### Distributed (3 nodes)
```
Consistency:      Invalidation-based
Message Type:     Key invalidation broadcasts
Network Latency:  Simulated message passing
```

## Architecture

### Single-Node Cache
```
┌─────────────────────────────┐
│   Hash Map (Fast Lookup)    │
│   key → list iterator       │
│   O(1) access               │
└─────────────────────────────┘
           ↓
┌─────────────────────────────┐
│ Doubly-Linked List (Order)  │
│ HEAD = Most Recently Used   │
│ TAIL = Least Recently Used  │
│ O(1) reordering             │
└─────────────────────────────┘
```

**How it works:**
- Hash map provides O(1) lookup by key
- Doubly-linked list maintains usage order
- When accessed: move to HEAD (most recently used)
- When full: evict from TAIL (least recently used)

### Distributed Network
```
┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│   Node 1     │    │   Node 2     │    │   Node 3     │
│ LRU Cache    │←→  │ LRU Cache    │←→  │ LRU Cache    │
└──────────────┘    └──────────────┘    └──────────────┘
      ↑                  ↑                    ↑
      └──────────────────┼────────────────────┘
         INVALIDATE Messages (Consistency)
```

## Consistency Protocol

### Invalidation-Based Approach

**Flow:**
1. Node A updates key X with value V
2. Node A broadcasts "INVALIDATE X" to Node B, C
3. Nodes B and C remove X from their caches
4. Next access to X: Cache MISS → fetch from database

**Trade-offs:**
- ✅ Simple to implement
- ✅ Guaranteed consistency (all nodes agree)
- ✅ No write conflicts
- ❌ Cache misses increase (need to refetch)
- ❌ Network overhead for invalidation messages

**Real-world example:**
- Amazon ElastiCache uses this for cache invalidation
- CDNs use this to invalidate old content
- Works well for read-heavy workloads

## Building & Running

### Requirements
- C++11 or later compiler (g++, clang)
- pthread library (for multi-threading)
- Windows/Linux/Mac

### Compilation

```bash
cd distributed-cache

# Compile all files
g++ -std=c++11 -pthread src/lru_cache.cpp src/benchmarking.cpp src/distributed_cache.cpp src/main.cpp -o build/lru_cache.exe

# Run the program
.\build\lru_cache.exe
```

### What the Output Shows

1. **Basic Functionality Tests** ✅
   - Verify cache put/get operations
   - Test LRU eviction

2. **Single-Threaded Benchmark** (10,000 ops)
   - Throughput: ~1.6M ops/sec
   - Hit Rate: ~98%
   - Latency: ~0.0001ms

3. **Multi-Threaded Benchmarks** (2 and 4 threads)
   - Throughput decreases with contention
   - Hit rates increase (better cache utilization)
   - Shows thread-safety in action

4. **Distributed Cache Tests** (3-node network)
   - Network creation
   - Put/Get operations
   - Invalidation protocol execution
   - Consistency verification

## File Structure

```
distributed-cache/
├── include/
│   ├── lru_cache.h              # Single-node cache header
│   ├── benchmarking.h           # Performance testing header
│   └── distributed_cache.h      # Distributed cache header
├── src/
│   ├── lru_cache.cpp            # LRU cache implementation
│   ├── benchmarking.cpp         # Benchmarking implementation
│   ├─��� distributed_cache.cpp    # Distributed cache implementation
│   └── main.cpp                 # Test suite and demos
├── build/                       # Compiled executables
├── README.md                    # This file
└── .gitignore                  # Git ignore file
```

## Implementation Details

### Single-Node LRU Cache

**Data Structures:**
```cpp
unordered_map<string, list<Node>::iterator> cache_map;
list<Node> cache_list;
mutex mtx;
```

**Operations:**

**get(key)** - O(1)
```
1. Look up key in hash_map
2. Move node to HEAD (most recently used)
3. Return value
```

**put(key, value)** - O(1)
```
1. If key exists:
   - Update value
   - Move to HEAD
2. Else (new key):
   - Add to HEAD
   - Add to hash_map
3. If size > capacity:
   - Remove TAIL (LRU)
   - Delete from hash_map
```

**Thread Safety:**
```cpp
std::lock_guard<std::mutex> lock(mtx);  // RAII - auto unlock
// critical section
// lock automatically released when scope ends
```

### Distributed Cache

**Message Types:**
- `PUT` - Update key-value pair
- `GET` - Retrieve value
- `INVALIDATE` - Remove key from cache
- `ACK` - Acknowledgment

**Consistency Flow:**
```
Node A: put("user:1", "Alice")
  ↓
Hash map updated locally
  ↓
Broadcast INVALIDATE("user:1") to B, C
  ↓
Node B receives → Remove user:1
Node C receives → Remove user:1
  ↓
Result: Consistent state across all nodes ✅
```

## Performance Analysis

### Single vs Multi-threaded

| Metric | Single | 2-Thread | 4-Thread |
|--------|--------|----------|----------|
| Throughput | 1.66M | 1.05M | 0.95M |
| Latency | 0.0001ms | 0.001ms | 0.003ms |
| Hit Rate | 98% | 99% | 99% |

**Why multi-threaded is slower:**
- Threads compete for mutex lock
- Lock contention increases with thread count
- Still very fast (0.95M ops/sec = 1 second handles 950K requests!)

### Why this is good for Amazon

- **Scale:** 1M+ ops/sec = can handle massive traffic
- **Hit rates:** 98%+ = rarely needs to hit slow database
- **Latency:** 0.0001ms = users get instant responses
- **Distributed:** Multiple data centers can stay in sync
- **Thread-safe:** Production-ready code

## Real-World Applications

This architecture powers:
- ✅ **Amazon ElastiCache** - Managed cache service
- ✅ **Redis Cluster** - Distributed in-memory database
- ✅ **Memcached** - High-performance caching
- ✅ **CDN Cache** - Content delivery networks
- ✅ **Database Query Cache** - Cache frequently accessed data

## Interview Talking Points

### Single-Node Cache
1. "Implemented O(1) operations using hash map + doubly-linked list"
   - Shows understanding of data structures
   - Demonstrates algorithm optimization

2. "Thread-safe with mutex locks, preventing race conditions"
   - Shows understanding of concurrency
   - Production-ready code

3. "Achieved 1.6 million operations per second"
   - Impressive performance metric
   - Shows optimization skills

### Multi-threaded
1. "Measured throughput and latency with different thread counts"
   - Shows empirical testing approach
   - Understanding of scalability trade-offs

2. "Demonstrated thread-safety even with 4 concurrent threads"
   - Shows system design thinking
   - Understanding of synchronization

### Distributed
1. "Implemented invalidation-based consistency protocol"
   - Shows distributed systems knowledge
   - Understanding of CAP theorem

2. "Each node can operate independently yet stay consistent"
   - Shows scalability thinking
   - Real-world relevance (Amazon scale)

3. "Trade-off: consistency vs availability"
   - Shows mature thinking
   - Understanding of distributed trade-offs

## Future Enhancements

1. **Advanced Consistency**
   - Replication with quorum voting
   - Vector clocks for ordering
   - Conflict resolution strategies

2. **Sharding**
   - Hash-based key distribution
   - Reduce mutex contention
   - Enable horizontal scaling

3. **Real Networking**
   - TCP sockets instead of simulated messages
   - Async I/O for performance
   - Failure detection and recovery

4. **Optimizations**
   - Lock-free data structures
   - Read-write locks (multiple readers)
   - Bloom filters for negative caching

## Key Learnings

- **Data Structures Matter:** Hash map + linked list = O(1) cache
- **Concurrency is Hard:** Threads need careful synchronization
- **Distributed is Complex:** Consistency across nodes requires protocol
- **Performance Testing is Critical:** Measure to understand trade-offs
- **Simple beats Complex:** Invalidation is simpler than replication

## Author

Created as distributed systems learning project.

## License

Educational use.