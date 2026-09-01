# Changelog

All notable changes to SharedMemoryCache are documented here.

## [Unreleased]

### Added

- Cross-platform timing support for x86 and ARM architectures
- Architecture-aware CMake flags
- Safe `get_ex()` overload with an explicit output buffer size
- Integration tests for basic operations, edge cases, concurrent access, attachment, and recycling
- Small mmap and SysV test configurations

### Changed

- Updated the README with build, test, roadmap, and platform documentation
- Replaced the non-standard variable-length arrays in validation code with `std::vector`
- Added macOS CPU frequency detection through `sysctlbyname`
- Added cleanup of memory mappings in the cache destructor
- Added smoke, continued, edge, normal, and attach test targets to CMake

### Fixed

- Corrected TTL expiration calculation
- Corrected mmap failure handling for `MAP_FAILED`
- Initialized file-lock ranges before calling `fcntl`
- Added output-buffer capacity checks during reads
- Propagated value-segment consistency errors
- Fixed first-entry LRU list initialization
- Avoided remapping value segments during removal
- Added configuration validation and parsing error handling
- Fixed portable test includes and `int64_t` format strings

### Known Limitations

- SysV shared-memory tests may fail in macOS sandboxed environments because of `shmat` restrictions
- Linux x86 and post-crash robust-mutex behavior still require platform-specific integration testing
