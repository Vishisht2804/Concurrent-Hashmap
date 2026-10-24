#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <utility>
#include <vector>

template <typename Key, typename Value>
class ConcurrentHashMap {
private:
    using Entry = std::pair<Key, Value>;

    static constexpr std::size_t stripe_count = 16;

    std::vector<std::vector<Entry>> buckets_;
    mutable std::array<std::shared_mutex, stripe_count> stripes_;

    mutable std::shared_mutex resize_mutex_;
    std::atomic<std::size_t> element_count_{0};

    float max_load_factor_;

    std::size_t bucket_index(const Key& key, std::size_t bucket_count) const {
        return std::hash<Key>{}(key) % bucket_count;
    }

    std::size_t stripe_index(std::size_t bucket) const {
        return bucket % stripe_count;
    }

    void resize_if_needed() {
        std::size_t current_size = element_count_.load();
        std::size_t current_capacity;

        {
            std::shared_lock resize_lock(resize_mutex_);
            current_capacity = buckets_.size();
        }

        if (static_cast<float>(current_size) / current_capacity <= max_load_factor_) {
            return;
        }

        rehash(current_capacity * 2);
    }

public:
    explicit ConcurrentHashMap(
        std::size_t initial_capacity = 16,
        float max_load_factor = 0.75f)
        : buckets_(initial_capacity == 0 ? 1 : initial_capacity),
          max_load_factor_(max_load_factor) {
        if (max_load_factor <= 0.0f) {
            throw std::invalid_argument("max_load_factor must be greater than zero");
        }
    }

    bool insert(const Key& key, const Value& value) {
        {
            std::shared_lock resize_lock(resize_mutex_);

            std::size_t bucket = bucket_index(key, buckets_.size());
            std::size_t stripe = stripe_index(bucket);
            std::unique_lock stripe_lock(stripes_[stripe]);

            auto& chain = buckets_[bucket];

            for (const auto& entry : chain) {
                if (entry.first == key) {
                    return false;
                }
            }

            chain.emplace_back(key, value);
            element_count_.fetch_add(1);
        }

        resize_if_needed();
        return true;
    }

    bool find(const Key& key, Value& result) const {
        std::shared_lock resize_lock(resize_mutex_);

        std::size_t bucket = bucket_index(key, buckets_.size());
        std::size_t stripe = stripe_index(bucket);
        std::shared_lock stripe_lock(stripes_[stripe]);

        const auto& chain = buckets_[bucket];

        for (const auto& entry : chain) {
            if (entry.first == key) {
                result = entry.second;
                return true;
            }
        }

        return false;
    }

    bool update(const Key& key, const Value& value) {
        std::shared_lock resize_lock(resize_mutex_);

        std::size_t bucket = bucket_index(key, buckets_.size());
        std::size_t stripe = stripe_index(bucket);
        std::unique_lock stripe_lock(stripes_[stripe]);

        auto& chain = buckets_[bucket];

        for (auto& entry : chain) {
            if (entry.first == key) {
                entry.second = value;
                return true;
            }
        }

        return false;
    }

    bool erase(const Key& key) {
        std::shared_lock resize_lock(resize_mutex_);

        std::size_t bucket = bucket_index(key, buckets_.size());
        std::size_t stripe = stripe_index(bucket);
        std::unique_lock stripe_lock(stripes_[stripe]);

        auto& chain = buckets_[bucket];

        for (auto iterator = chain.begin(); iterator != chain.end(); ++iterator) {
            if (iterator->first == key) {
                chain.erase(iterator);
                element_count_.fetch_sub(1);
                return true;
            }
        }

        return false;
    }

    std::size_t size() const {
        return element_count_.load();
    }

    void rehash(std::size_t new_capacity) {
        if (new_capacity == 0) {
            new_capacity = 1;
        }

        std::unique_lock resize_lock(resize_mutex_);

        std::size_t current_size = element_count_.load();

        std::size_t minimum_capacity = static_cast<std::size_t>(
            std::ceil(static_cast<float>(current_size) / max_load_factor_));

        if (new_capacity < minimum_capacity) {
            new_capacity = minimum_capacity;
        }

        if (new_capacity == buckets_.size()) {
            return;
        }

        // Normal operations lock resize_mutex_ first, then one stripe.
        // Rehashing keeps resize_mutex_ exclusively locked and then locks
        // every stripe in the same order before replacing the buckets.
        std::array<std::unique_lock<std::shared_mutex>, stripe_count> stripe_locks;

        for (std::size_t i = 0; i < stripe_count; ++i) {
            stripe_locks[i] = std::unique_lock<std::shared_mutex>(stripes_[i]);
        }

        std::vector<std::vector<Entry>> new_buckets(new_capacity);

        for (const auto& chain : buckets_) {
            for (const auto& entry : chain) {
                std::size_t new_bucket =
                    bucket_index(entry.first, new_capacity);

                new_buckets[new_bucket].push_back(entry);
            }
        }

        buckets_ = std::move(new_buckets);
    }
};