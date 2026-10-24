# Concurrent Hash Map

A small thread-safe hash map implemented in C++17 using:

- Separate chaining
- Fixed lock striping
- Shared locks for lookups
- Exclusive locks for modifications
- Dynamic resizing
- Standard library containers and synchronization primitives

## Build

Using CMake:

```powershell
cmake -S . -B build
cmake --build build --config Release
.\build\Release\concurrent_hash_map.exe
```

With MinGW:

```powershell
g++ -std=c++17 -pthread -O2 main.cpp -o concurrent_hash_map.exe
.\concurrent_hash_map.exe
```

## Design

The map stores entries in a vector of buckets. Each bucket contains a vector
of key-value pairs, providing separate chaining for collisions.

The map has 16 lock stripes, independent of the number of buckets. A bucket
maps to a stripe using `bucket_index % stripe_count`.

Lookups use `std::shared_lock`, so multiple readers can use the same stripe at
the same time. Insert, update, and erase use `std::unique_lock` because they
modify a bucket.

## Rehashing

Normal operations first acquire a shared lock on `resize_mutex_`, then acquire
the required stripe lock. Rehashing acquires `resize_mutex_` exclusively,
which prevents normal operations from using the bucket array while it is being
replaced. It then locks all stripes in increasing order, moves every entry to
the new bucket array, and replaces the old array.

The lock order is always:

1. `resize_mutex_`
2. Stripe locks

This prevents normal operations and rehashing from acquiring locks in
conflicting orders.

## Complexity and limitations

With a good hash function, insert, find, update, and erase are expected O(1).
Rehashing takes O(n), where n is the number of stored entries.

The map uses a fixed number of stripes, and a resize temporarily blocks normal
operations. Poor hash functions can cause long chains and degrade operations
toward O(n). Keys and values must be copyable because entries are copied during
rehashing.
