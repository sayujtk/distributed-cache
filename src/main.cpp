#include <iostream>
#include <iomanip>
#include <chrono>
#include "../include/lru_cache.h"
#include "../include/benchmarking.h"
#include "../include/distributed_cache.h"

void printResults(const BenchmarkResults &results, const std::string &test_name)
{
    std::cout << "\n========== " << test_name << " ==========" << std::endl;
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Total Operations:        " << results.total_operations << std::endl;
    std::cout << "Total Time:              " << results.total_time_ms << " ms" << std::endl;
    std::cout << "Throughput:              " << results.total_throughput_ops_sec << " ops/sec" << std::endl;
    std::cout << "Cache Hits:              " << results.total_hits << std::endl;
    std::cout << "Cache Misses:            " << results.total_misses << std::endl;
    std::cout << "Hit Rate:                " << results.cache_hit_rate_percent << " %" << std::endl;
    std::cout << "Avg GET Latency:         " << results.avg_get_latency_ms << " ms" << std::endl;
    std::cout << "Avg PUT Latency:         " << results.avg_put_latency_ms << " ms" << std::endl;
    std::cout << "==================================\n"
              << std::endl;
}

int main()
{
    std::cout << "===== LRU Cache Benchmarking Suite =====" << std::endl;
    // Test 1: Basic Functionality
    std::cout << "\n--- Test 1: Basic Functionality ---" << std::endl;
    LRUCache cache1(3);
    cache1.put("user:1", "Alice");
    cache1.put("user:2", "Bob");
    cache1.put("user:3", "Charlie");

    std::cout << " Added 3 users. Cache size: " << cache1.size() << std::endl;
    std::cout << " Get user:1: " << cache1.get("user:1") << std::endl;

    cache1.put("user:4", "David");
    std::cout << " Added user:4 (evicted user:3). Cache size: " << cache1.size() << std::endl;

    // Test 2: Single-threaded Benchmark
    std::cout << "===== single thread Benchmarking Suite =====" << std::endl;

    LRUCache cache2(100);                       // Larger cache
    Benchmarker bench_single(cache2, 1, 10000); // 10k operations
    bench_single.generateTestKeys(50);          // 50 different keys

    auto start = std::chrono::high_resolution_clock::now();
    bench_single.runSingleThreadedWorkload();
    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    BenchmarkResults results_single = bench_single.getResults(elapsed_ms, false);
    printResults(results_single, "Single-Threaded (10k ops)");

    // Test 3: Multi-threaded Benchmark (2 threads)
    std::cout << "===== Multithread Benchmarking (2 threads) =====" << std::endl;


    LRUCache cache3(100);
    Benchmarker bench_multi2(cache3, 2, 10000); // 2 threads, 10k ops each
    bench_multi2.generateTestKeys(50);

    start = std::chrono::high_resolution_clock::now();
    bench_multi2.runMultiThreadedWorkload();
    end = std::chrono::high_resolution_clock::now();
    elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    BenchmarkResults results_multi2 = bench_multi2.getResults(elapsed_ms, true);
    printResults(results_multi2, "Multi-Threaded (2 threads, 10k ops each)");

    // Test 4: Multi-threaded Benchmark (4 threads)
    std::cout << "===== Multithread Benchmark (4 threads )=====" << std::endl;

    LRUCache cache4(100);
    Benchmarker bench_multi4(cache4, 4, 5000); // 4 threads, 5k ops each
    bench_multi4.generateTestKeys(50);

    start = std::chrono::high_resolution_clock::now();
    bench_multi4.runMultiThreadedWorkload();
    end = std::chrono::high_resolution_clock::now();
    elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    BenchmarkResults results_multi4 = bench_multi4.getResults(elapsed_ms, true);
    printResults(results_multi4, "Multi-Threaded (4 threads, 5k ops each)");

    // Test 5: Comparison
    std::cout << "===== Performance comprarison =====" << std::endl;

    std::cout << "\nThroughput Comparison:" << std::endl;
    std::cout << "Single-threaded:    " << std::fixed << std::setprecision(0)
              << results_single.total_throughput_ops_sec << " ops/sec" << std::endl;
    std::cout << "2 threads:          " << results_multi2.total_throughput_ops_sec << " ops/sec" << std::endl;
    std::cout << "4 threads:          " << results_multi4.total_throughput_ops_sec << " ops/sec" << std::endl;

    std::cout << "\nLatency Comparison (Avg GET):" << std::endl;
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Single-threaded:    " << results_single.avg_get_latency_ms << " ms" << std::endl;
    std::cout << "2 threads:          " << results_multi2.avg_get_latency_ms << " ms" << std::endl;
    std::cout << "4 threads:          " << results_multi4.avg_get_latency_ms << " ms" << std::endl;

    std::cout << "\n All benchmarks completed!" << std::endl;

    // ADD THIS AT THE END OF main() function, before return 0;

    // ===== DISTRIBUTED CACHE TEST =====
    std::cout << "===== DISTRIBUTED CACHE TEST  =====" << std::endl;


    DistributedCache dist_cache;

    std::cout << "Creating 3-node distributed cache network...\n"
              << std::endl;
    dist_cache.addNode("node1", 50);
    dist_cache.addNode("node2", 50);
    dist_cache.addNode("node3", 50);

    dist_cache.printNetworkStatus();

    std::cout << "--- Test 1: Write to Node1 ---" << std::endl;
    dist_cache.putToNode("node1", "user:1", "Alice");
    dist_cache.printAllCaches();

    std::cout << "\n--- Test 2: Read from Node1 (should HIT) ---" << std::endl;
    dist_cache.getFromNode("node1", "user:1");

    std::cout << "\n--- Test 3: Try read from Node2 (should MISS - invalidated) ---" << std::endl;
    dist_cache.getFromNode("node2", "user:1");

    std::cout << "\n--- Test 4: Write different value to Node2 ---" << std::endl;
    dist_cache.putToNode("node2", "user:2", "Bob");
    dist_cache.printAllCaches();

    std::cout << "\n--- Test 5: Simulate multiple writes (consistency) ---" << std::endl;
    dist_cache.putToNode("node1", "product:100", "Laptop");
    dist_cache.putToNode("node2", "product:100", "Laptop Updated");
    dist_cache.putToNode("node3", "product:100", "Laptop Final");
    dist_cache.printAllCaches();

    std::cout << "\n Distributed Cache tests completed!" << std::endl;

    return 0;
}