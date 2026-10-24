cmake_minimum_required(VERSION 3.10)

project(ConcurrentHashMap LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_executable(concurrent_hash_map
    main.cpp
)

target_link_libraries(concurrent_hash_map
    PRIVATE Threads::Threads
)