#ifndef MINIREDIS_SHARDED_CACHE_HPP
#define MINIREDIS_SHARDED_CACHE_HPP
#include "lru_cache.hpp"
#include <vector>
#include <memory>
#include <functional>
#include <stdexcept>

class ShardedCache {
private:
    int num_shards;
    std::vector<std::unique_ptr<LRUCache>> shards;
    std::hash<std::string> hasher;

    int get_shard_idx(const std::string& key) const {
        return hasher(key) % num_shards;
    }

public:
    ShardedCache(int total_cap, int shards_cnt) {
        if (total_cap < 1) {
            throw std::invalid_argument("cache capacity must be at least 1");
        }
        if (shards_cnt < 1) {
            throw std::invalid_argument("shard count must be at least 1");
        }

        num_shards = shards_cnt;
        const int base_shard_capacity = total_cap / num_shards;
        const int extra_entries = total_cap % num_shards;
        shards.reserve(num_shards);

        for (int i = 0; i < num_shards; i++) {
            // Spread the remainder over the first shards so the sum of the
            // shard capacities is exactly the requested total capacity.
            const int shard_capacity = base_shard_capacity + (i < extra_entries ? 1 : 0);
            shards.push_back(std::make_unique<LRUCache>(shard_capacity));
        }
    }

    bool get(const std::string& key, std::string& out_val) {
        int idx = get_shard_idx(key);
        return shards[idx]->get(key, out_val);
    }

    void put(const std::string& key, const std::string& val) {
        int idx = get_shard_idx(key);
        shards[idx]->put(key, val);
    }
};

#endif  // MINIREDIS_SHARDED_CACHE_HPP
