# SharedMemoryCache

SharedMemoryCache is a process-shared key-value cache implemented with POSIX mmap or System V shared memory. It uses fixed-size value blocks, a hash table, LRU metadata, and process-shared synchronization.

The project is a practice-oriented rewrite of `libshmcache`. The storage layout is documented in [data_structure.pdf](data_structure.pdf).

## Build

The project uses CMake and requires a C++14 compiler.

On Linux:

```bash
cmake -S . -B build
cmake --build build -j2
```

On macOS ARM with Homebrew GCC 16:

```bash
cmake -S . -B /private/tmp/shmcache-build \
  -DCMAKE_C_COMPILER=/opt/homebrew/opt/gcc/bin/gcc-16 \
  -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/gcc/bin/g++-16
cmake --build /private/tmp/shmcache-build -j2
```

The build produces libraries, workload programs, and integration tests under `bin/` and `lib/` in the build directory.

## Tests

Run the standard mmap tests after building:

```bash
build/bin/smoke
build/bin/continued
build/bin/edge
build/bin/normal
```

On macOS ARM, run the tests from the selected build directory:

```bash
/private/tmp/shmcache-build/bin/smoke
/private/tmp/shmcache-build/bin/continued
/private/tmp/shmcache-build/bin/edge
/private/tmp/shmcache-build/bin/normal
/private/tmp/shmcache-build/bin/attach cfg/attach-mmap.conf
```

Test coverage:

- `smoke` covers basic set/get, TTL, buffer bounds, and cleanup
- `continued` covers legacy API rejection, fork-based sharing, and TTL behavior
- `edge` covers values spanning multiple blocks, multi-segment growth, overwrites, and invalid parameters
- `normal` covers expiration/deletion APIs, statistics, and concurrent multi-process reads and writes
- `attach` covers independent-process attachment and key recycling

The default `attach` configuration uses System V shared memory and should be run on Linux. A macOS sandbox may reject `shmat()` because of IPC restrictions; use `cfg/attach-mmap.conf` on macOS.

## Roadmap

### 1. Move large reads outside the global lock

Large values currently hold the global mutex during copying, which serializes readers. The implementation must protect value blocks from concurrent recycling, for example with entry versions plus block pinning or reference counts. A checksum alone cannot prevent use-after-free.

### 2. Re-evaluate mutex versus rwlock

The current mutex also protects LRU updates, popularity counters, statistics, and allocation metadata, so `get()` is not read-only. An rwlock should only be introduced after separating mutable metadata updates from the value-copy path and measuring contention.

### 3. Optional value compression

Compression may reduce memory usage for large, compressible values but adds CPU cost and metadata complexity. It should be guided by workload benchmarks and must store the codec, compressed length, original length, and a fallback for incompressible values.

## Platform Notes

- x86 builds enable `-mavx`; other architectures omit it
- x86 uses `rdtsc` for local timing, while other architectures use `clock_gettime`
- System V shared memory requires platform IPC support and permissions
- Run Linux x86_64 and macOS ARM integration tests before production deployment
