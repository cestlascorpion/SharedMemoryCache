#ifndef SHMCACHE_COMMON_DEFINE_H
#define SHMCACHE_COMMON_DEFINE_H

#define SHM_MAX_PATH_SIZE 256

#define SHM_MEM_TYPE_MMAP 0
#define SHM_MEM_TYPE_SHM 1

#define SHM_STATUS_INIT 0
#define SHM_STATUS_NORMAL 0x12345678

#define SHM_MAX_MEM_MB 4096
#define SHM_MIN_MEM_MB 256
#define SHM_MAX_KEY_NUM 10000
#define SHM_MAX_KEY_SIZE 128
#define SHM_MAX_VAL_SIZE 32 * 1024 * 1024
#define SHM_HT_SEGMENT_ID 1

#define SHM_TRYLOCK_INTERVAL 100
#define SHM_TRYLOCK_TICKS 1000

#define SHM_SEGMENT_SIZE 128 * 1024 * 1024
#define SHM_BLOCK_SIZE 256 * 1024

#define SHM_MEM_ALIGN_BYTE(x) (((x) + 7u) & (~7u))
#define SHM_MEM_ALIGN(x, align) (((x) + ((align)-1u)) & (~((align)-1u)))

#define SHM_RDTSC(low, high) __asm__ __volatile__("rdtsc" : "=a"(low), "=d"(high))

#define SHM_ORIGIN_MEMCPY
#if defined(__x86_64__) && defined(__linux__) && !defined(__CYGWIN__) && !defined(SHM_ORIGIN_MEMCPY)
#define SHM_FOLLY_MEMCPY
#endif
#if !defined(SHM_ORIGIN_MEMCPY) && !defined(SHM_FOLLY_MEMCPY)
#define SHM_AVX_MEMCPY
#endif
#if !defined(SHM_FOLLY_MEMCPY) && !defined(SHM_AVX_MEMCPY) && !defined(SHM_ORIGIN_MEMCPY)
#define SHM_ORIGIN_MEMCPY
#endif

#endif // SHMCACHE_COMMON_DEFINE_H
