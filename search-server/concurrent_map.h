#pragma once

#include <cstdlib>
#include <map>
#include <string>
#include <vector>
#include <mutex>


using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap {
private:
    struct Bucket {
        std::mutex m;
        std::map<Key, Value> one_bucket;
    };

public:
   // static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);


    struct Access {

        std::lock_guard<std::mutex> guard;
        Value& ref_to_value;

        Access(const Key& key, Bucket& bucket)
        :   guard(bucket.m)
        ,   ref_to_value(bucket.one_bucket[key]) {            
        } 
    };

    explicit ConcurrentMap(size_t bucket_count)
    :   buckets_(bucket_count) {
    }

    Access operator[](const Key& key) {
        auto& bucket = buckets_[static_cast<uint64_t>(key) % buckets_.size()];
        return {key, bucket};
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> ordinary_map;

        for(auto& [mutex, map] : buckets_) {
            std::lock_guard g(mutex);
            ordinary_map.insert(map.begin(), map.end());
        }
        return ordinary_map;
    };

private:
    std::vector<Bucket> buckets_ = {};
};