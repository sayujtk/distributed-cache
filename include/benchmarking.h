#ifndef BENCHMARKING_H
#define BENCHMARKING_H

#include "lru_cache.h"
#include <chrono>
#include <vector>
#include <thread>
#include <random>
#include <atomic>

struct BenchmarkResults
{
    double avg_get_latency_ms;       // Average time for get()
    double avg_put_latency_ms;       // Average time for put()
    double total_throughput_ops_sec; // Operations per second
    double cache_hit_rate_percent;   // % of successful gets
    int total_operations;
    int total_hits;
    int total_misses;
    long long total_time_ms;
};

class Benchmarker
{
private:
    LRUCache &cache;
    int num_threads;
    int ops_per_thread;
    std::vector<std::string> test_keys;

    // Atomic counters (thread-safe)
    std::atomic<int> hit_count{0};
    std::atomic<int> miss_count{0};
    std::atomic<long long> total_get_time_us{0}; // microseconds
    std::atomic<long long> total_put_time_us{0};

public:
    Benchmarker(LRUCache &c, int threads, int ops);

    // Generate test keys (what we'll access)
    void generateTestKeys(int num_keys);

    // Simulate cache workload with multiple threads
    void runMultiThreadedWorkload();

    // Single-threaded workload for comparison
    void runSingleThreadedWorkload();

    // Get benchmark results
    BenchmarkResults getResults(long long elapsed_ms, bool multi_threaded);

    // Helper: random key selection (simulates real workload)
    std::string getRandomKey();
};

#endif // BENCHMARKING_H