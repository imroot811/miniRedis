#pragma once
#include "lru_cache.hpp"
#include <vector>
#include <memory>
#include <functional>

class ShardedCache {
private:
    int num_shards;
    std::vector<std::unique_ptr<LRUCache>> shards;
    std::hash<std::string> hasher;

    int get_shard_idx(const std::string& key) {
        return hasher(key) % num_shards;
    }

public:
    ShardedCache(int total_cap, int shards_cnt) {
        num_shards = std::max(1, shards_cnt);
        int shard_cap = std::max(1, total_cap / num_shards);

        for (int i = 0; i < num_shards; i++) {
            shards.push_back(std::make_unique<LRUCache>(shard_cap));
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
