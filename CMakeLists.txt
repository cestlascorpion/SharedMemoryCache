cmake_minimum_required(VERSION 3.10)

project(shmcache LANGUAGES C CXX ASM)

find_package(Threads REQUIRED)

set(PROJECT_WARNINGS
    -Wall
    -Wextra
    -Werror
    -Wconversion
    -Wsizeof-pointer-memaccess
    -Wfloat-equal
    -Wconversion-null
    -Woverflow
    -Wshadow)
set(SHM_SOURCES
    src/common_types.cpp
    src/shm_allocator.cpp
    src/shm_cache.cpp
    src/shm_configure.cpp
    src/shm_hashtable.cpp
    src/shm_lock.cpp
    src/shm_memory.cpp
    src/shm_serialization.cpp
    src/mem/memcpy_folly.S)
set(SHM_HEADERS
    src/common_define.h
    src/common_types.h
    src/shm_allocator.h
    src/shm_cache.h
    src/shm_configure.h
    src/shm_hashtable.h
    src/shm_lock.h
    src/shm_memory.h
    src/shm_serialization.h
    src/mem/memcpy_avx.h)
set(SHM_PUBLIC_HEADERS src/common_define.h src/common_types.h src/shm_cache.h src/shm_serialization.h)
set(SHM_EXTRA_FILES src/mem/memcpy_folly.S src/mem/memcpy_avx.h)
set(SHM_ARCH_OPTIONS)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$")
    list(APPEND SHM_ARCH_OPTIONS -mavx)
endif()

function(add_shmcache_options target)
    set_target_properties(${target} PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    target_compile_definitions(${target} PRIVATE _FILE_OFFSET_BITS=64 _GNU_SOURCE)
    target_compile_options(${target} PRIVATE ${PROJECT_WARNINGS} ${SHM_ARCH_OPTIONS} -g -O3)
    target_include_directories(${target} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>)
    target_link_libraries(${target} PUBLIC Threads::Threads)
endfunction()

add_library(shmcache SHARED ${SHM_SOURCES} ${SHM_HEADERS})
add_shmcache_options(shmcache)
set_target_properties(shmcache PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

add_library(shmcache_static STATIC ${SHM_SOURCES} ${SHM_HEADERS})
add_shmcache_options(shmcache_static)
set_target_properties(shmcache_static PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib OUTPUT_NAME shmcache)

install(TARGETS shmcache shmcache_static LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
install(FILES ${SHM_PUBLIC_HEADERS} DESTINATION include/shmcache)
install(FILES ${SHM_EXTRA_FILES} DESTINATION include/shmcache/mem)

foreach(target hornet bardoom smoke continued attach edge normal)
    add_executable(${target} test/${target}.cpp)
    add_shmcache_options(${target})
    target_link_libraries(${target} PRIVATE shmcache_static)
    set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endforeach()
