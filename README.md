# SharedMemoryCache

SharedMemoryCache is a C++14 process-shared key-value cache using POSIX mmap or System V shared memory, fixed-size value blocks, a hash table, LRU metadata, and process-shared synchronization.

## Build

```sh
cmake -S . -B build
cmake --build build
```

On macOS ARM, use a GCC toolchain that supports the required shared-memory and assembly paths.

## Test

```sh
build/bin/smoke
build/bin/continued
build/bin/edge
build/bin/normal
```

The default `attach` configuration uses System V shared memory and is intended for Linux. Use `cfg/attach-mmap.conf` on macOS, where sandboxing can reject `shmat()`. See [data_structure.pdf](data_structure.pdf) for the storage layout.
