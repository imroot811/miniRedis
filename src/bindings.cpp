#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "../include/sharded_cache.hpp"
#include "../include/thread_pool.hpp"
#include <chrono>
#include <atomic>
#include <algorithm>
#include <future>
#include <random>
#include <stdexcept>
#include <vector>

namespace py = pybind11;

// Helper to run workload on a given ShardedCache instance
std::pair<double, int> execute_workload(
    int num_shards, int cap, int num_threads, int total_ops, int read_ratio
) {
    if (num_threads < 1) {
        throw std::invalid_argument("benchmark requires at least one thread");
    }
    if (total_ops < 1) {
        throw std::invalid_argument("benchmark requires at least one operation");
    }
    if (read_ratio < 0 || read_ratio > 100) {
        throw std::invalid_argument("read ratio must be between 0 and 100");
    }

    ShardedCache cache(cap, num_shards);
    ThreadPool pool(num_threads);

    // Pre-warm the cache with some initial keys so GETs have realistic hits
    int key_range = cap * 2;
    for (int i = 0; i < cap / 2; i++) {
        cache.put("key_" + std::to_string(i), "val_" + std::to_string(i));
    }

    std::atomic<int> hits{0};
    const int base_ops_per_thread = total_ops / num_threads;
    const int remaining_ops = total_ops % num_threads;
    std::vector<std::future<void>> futures;
    futures.reserve(num_threads);

    auto start = std::chrono::high_resolution_clock::now();

    for (int t = 0; t < num_threads; t++) {
        const int thread_ops = base_ops_per_thread + (t < remaining_ops ? 1 : 0);
        futures.push_back(pool.enqueue([&, t, thread_ops]() {
            // Fast per-thread random generator
            std::mt19937 rng(1337 + t);
            std::string dummy_val;

            for (int i = 0; i < thread_ops; i++) {
                int k = rng() % key_range;
                int op = rng() % 100;
                std::string key = "key_" + std::to_string(k);

                if (op < read_ratio) {
                    if (cache.get(key, dummy_val)) {
                        hits.fetch_add(1, std::memory_order_relaxed);
                    }
                } else {
                    cache.put(key, "val_" + std::to_string(i));
                }
            }
        }));
    }

    for (auto& f : futures) {
        f.get();
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    return {elapsed.count(), hits.load()};
}

// LiveEngine exposes both Mode A (CRUD) and Mode B (Benchmark) to FastAPI
class LiveEngine {
private:
    ShardedCache cache;
    ThreadPool pool;

public:
    LiveEngine(int cap, int num_shards, int num_threads)
        : cache(cap, num_shards), pool(num_threads) {}

    void put(const std::string& key, const std::string& val) {
        auto fut = pool.enqueue([this, key, val]() {
            cache.put(key, val);
        });
        fut.get();
    }

    py::object get(const std::string& key) {
        auto fut = pool.enqueue([this, key]() -> std::pair<bool, std::string> {
            std::string val;
            bool ok = cache.get(key, val);
            return {ok, val};
        });
        auto res = fut.get();
        if (!res.first) return py::none();
        return py::cast(res.second);
    }

    py::dict benchmark(int num_threads, int total_ops, int read_ratio, int num_shards, int cap) {
        // 1. Global Lock Baseline (num_shards = 1)
        auto [global_ms, global_hits] = execute_workload(1, cap, num_threads, total_ops, read_ratio);

        // 2. Lock-Striped Cache (num_shards = user specified, e.g., 32)
        auto [sharded_ms, sharded_hits] = execute_workload(num_shards, cap, num_threads, total_ops, read_ratio);

        const double safe_global_ms = std::max(0.001, global_ms);
        const double safe_sharded_ms = std::max(0.001, sharded_ms);
        double global_ops_sec = total_ops / (safe_global_ms / 1000.0);
        double sharded_ops_sec = total_ops / (safe_sharded_ms / 1000.0);
        double speedup = safe_global_ms / safe_sharded_ms;
        double pct_increase = ((sharded_ops_sec - global_ops_sec) / global_ops_sec) * 100.0;

        py::dict result;
        result["global_time_ms"] = global_ms;
        result["global_throughput_ops_sec"] = (long long)global_ops_sec;
        result["sharded_time_ms"] = sharded_ms;
        result["sharded_throughput_ops_sec"] = (long long)sharded_ops_sec;
        result["speedup_factor"] = speedup;
        result["throughput_increase_pct"] = pct_increase;
        return result;
    }
};

PYBIND11_MODULE(cache_engine, m) {
    py::class_<LiveEngine>(m, "LiveEngine")
        .def(py::init<int, int, int>())
        .def("put", &LiveEngine::put)
        .def("get", &LiveEngine::get)
        .def("benchmark", &LiveEngine::benchmark);
}
