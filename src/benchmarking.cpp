#include "../include/benchmarking.h"
#include <iostream>
#include <iomanip>

Benchmarker::Benchmarker(LRUCache &c, int threads, int ops)
    : cache(c), num_threads(threads), ops_per_thread(ops)
{
}

// Generate random test keys
void Benchmarker::generateTestKeys(int num_keys)
{
    test_keys.clear();
    for (int i = 0; i < num_keys; ++i)
    {
        test_keys.push_back("key:" + std::to_string(i));
    }
    std::cout << "Generated " << num_keys << " test keys" << std::endl;
}

// Get random key from test keys
std::string Benchmarker::getRandomKey()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, test_keys.size() - 1);
    return test_keys[dis(gen)];
}

// Single-threaded workload
void Benchmarker::runSingleThreadedWorkload()
{
    std::cout << "\n--- Running Single-Threaded Workload ---" << std::endl;
    std::cout << "Operations: " << ops_per_thread << std::endl;

    // Reset counters
    hit_count = 0;
    miss_count = 0;
    total_get_time_us = 0;
    total_put_time_us = 0;

    // Workload: 70% GET, 30% PUT (realistic pattern)
    for (int i = 0; i < ops_per_thread; ++i)
    {
        std::string key = getRandomKey();

        if (i % 10 < 7)
        { // 70% GET
            auto start = std::chrono::high_resolution_clock::now();
            try
            {
                cache.get(key);
                hit_count++;
            }
            catch (const std::out_of_range &)
            {
                miss_count++;
            }
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            total_get_time_us += duration.count();
        }
        else
        { // 30% PUT
            auto start = std::chrono::high_resolution_clock::now();
            cache.put(key, "value");
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            total_put_time_us += duration.count();
        }
    }
}

// Multi-threaded worker function
void multiThreadWorker(Benchmarker &bench, LRUCache &cache, int ops,
                       std::vector<std::string> &keys,
                       std::atomic<int> &hits, std::atomic<int> &misses,
                       std::atomic<long long> &get_time, std::atomic<long long> &put_time)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> key_dis(0, keys.size() - 1);

    for (int i = 0; i < ops; ++i)
    {
        std::string key = keys[key_dis(gen)];

        if (i % 10 < 7)
        { // 70% GET
            auto start = std::chrono::high_resolution_clock::now();
            try
            {
                cache.get(key);
                hits++;
            }
            catch (const std::out_of_range &)
            {
                misses++;
            }
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            get_time += duration.count();
        }
        else
        { // 30% PUT
            auto start = std::chrono::high_resolution_clock::now();
            cache.put(key, "value");
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            put_time += duration.count();
        }
    }
}

// Multi-threaded workload
void Benchmarker::runMultiThreadedWorkload()
{
    std::cout << "\n--- Running Multi-Threaded Workload ---" << std::endl;
    std::cout << "Threads: " << num_threads << std::endl;
    std::cout << "Operations per thread: " << ops_per_thread << std::endl;

    // Reset counters
    hit_count = 0;
    miss_count = 0;
    total_get_time_us = 0;
    total_put_time_us = 0;

    // Create worker threads
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back(multiThreadWorker, std::ref(*this), std::ref(cache),
                             ops_per_thread, std::ref(test_keys),
                             std::ref(hit_count), std::ref(miss_count),
                             std::ref(total_get_time_us), std::ref(total_put_time_us));
    }

    // Wait for all threads to complete
    for (auto &t : threads)
    {
        t.join();
    }
}

// Calculate and return results
BenchmarkResults Benchmarker::getResults(long long elapsed_ms, bool multi_threaded)
{
    BenchmarkResults results;

    results.total_operations = (multi_threaded ? num_threads : 1) * ops_per_thread;
    results.total_hits = hit_count;
    results.total_misses = miss_count;
    results.total_time_ms = elapsed_ms;

    // Calculate averages
    long long total_get_ops = results.total_hits + results.total_misses;
    results.avg_get_latency_ms = total_get_ops > 0 ? (total_get_time_us.load() / 1000.0) / total_get_ops : 0;

    long long total_put_ops = results.total_operations - total_get_ops;
    results.avg_put_latency_ms = total_put_ops > 0 ? (total_put_time_us.load() / 1000.0) / total_put_ops : 0;

    // Throughput: operations per second
    results.total_throughput_ops_sec = elapsed_ms > 0 ? (results.total_operations * 1000.0) / elapsed_ms : 0;

    // Hit rate percentage
    results.cache_hit_rate_percent = total_get_ops > 0 ? (results.total_hits * 100.0) / total_get_ops : 0;

    return results;
}