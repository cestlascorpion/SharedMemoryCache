cmake_minimum_required(VERSION 3.10)

project(shmcache)

set(CMAKE_CXX_STANDARD 14)

enable_language(C CXX ASM)

set(CMAKE_CXX_FLAGS "-Wall -Wextra -Werror -Wconversion -Wsizeof-pointer-memaccess \
    -Wfloat-equal -Wconversion-null -Woverflow -Wshadow \
    -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -pthread -g -O3 -mavx")

set(SOURCE
        src/mem/memcpy_folly.S src/mem/memcpy_avx.h src/common_define.h src/common_types.cpp
        src/common_types.h src/shm_cache.cpp src/shm_cache.h src/shm_lock.cpp src/shm_lock.h
        src/shm_hashtable.cpp src/shm_hashtable.h src/shm_configure.cpp src/shm_configure.h
        src/shm_memory.cpp src/shm_memory.h src/shm_allocator.cpp src/shm_allocator.h
        src/shm_serialization.cpp src/shm_serialization.h)

set(HEADER src/common_define.h src/common_types.h src/shm_cache.h src/shm_serialization.h)

set(EXTRA src/mem/memcpy_folly.S src/mem/memcpy_avx.h)

add_library(shmcache SHARED ${SOURCE})

add_library(shmcache_static STATIC ${SOURCE})

set_target_properties(shmcache_static PROPERTIES OUTPUT_NAME "shmcache")

set_target_properties(shmcache_static PROPERTIES CLEAN_DIRECT_OUTPUT 1)

set_target_properties(shmcache PROPERTIES CLEAN_DIRECT_OUTPUT 1)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)

install(TARGETS shmcache shmcache_static LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)

install(FILES ${HEADER} DESTINATION include/shmcache)

install(FILES ${EXTRA} DESTINATION include/shmcache/mem)

add_executable(hornet test/hornet.cpp ${SOURCE})

add_executable(bardoom test/bardoom.cpp ${SOURCE})