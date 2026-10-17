/* SPDX-License-Identifier: GPL-2.0-only */
#include <inttypes.h>
#include <sys/mman.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ADD_EAX_IMM8(value) 0x83, 0xc0, (value)

static __attribute__((noinline)) uint64_t dead_high_chain(uint32_t value)
{
    uint64_t result;

    __asm__ volatile(
        "mov %[stale], %%rax\n\t"
        "mov %[value], %%eax\n\t"
        "add $0x123, %%eax\n\t"
        "xor $0x55, %%eax\n\t"
        "lea 3(%%rax,%%rax,4), %%eax\n\t"
        "mov %%rax, %[result]"
        : [result] "=m"(result)
        : [stale] "r"(UINT64_C(0x1234567800000000)),
          [value] "r"(value)
        : "rax", "cc");
    return result;
}

static __attribute__((noinline)) uint64_t live_high_guard(uint32_t value)
{
    uint64_t result;

    __asm__ volatile(
        "mov %[stale], %%rax\n\t"
        "mov %[value], %%eax\n\t"
        "mov %%rax, %%rdx\n\t"
        "mov %%rdx, %[result]"
        : [result] "=m"(result)
        : [stale] "r"(UINT64_C(0x1234567800000000)),
          [value] "r"(value)
        : "rax", "rdx");
    return result;
}

typedef uint64_t (*generated_fn)(void);

static size_t emit_stale_producer(uint8_t *code)
{
    static const uint8_t prefix[] = {
        0xb8, 0x01, 0x00, 0x00, 0x00,
    };
    static const uint8_t adds[] = {
        ADD_EAX_IMM8(1), ADD_EAX_IMM8(1), ADD_EAX_IMM8(1),
        ADD_EAX_IMM8(1), ADD_EAX_IMM8(1), ADD_EAX_IMM8(1),
        ADD_EAX_IMM8(1), ADD_EAX_IMM8(1),
    };
    uint8_t *cursor = code;

    memcpy(cursor, prefix, sizeof(prefix));
    cursor += sizeof(prefix);
    memcpy(cursor, adds, sizeof(adds));
    cursor += sizeof(adds);
    *cursor++ = 0xc3;
    return cursor - code;
}

static size_t emit_stale_consumer(uint8_t *code)
{
    static const uint8_t prefix[] = {
        0x48, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x78, 0x56,
        0x34, 0x12,
    };
    static const uint8_t adds[] = {
        ADD_EAX_IMM8(2), ADD_EAX_IMM8(2), ADD_EAX_IMM8(2),
        ADD_EAX_IMM8(2), ADD_EAX_IMM8(2), ADD_EAX_IMM8(2),
        ADD_EAX_IMM8(2), ADD_EAX_IMM8(2),
    };
    static const uint8_t suffix[] = {
        0x48, 0x89, 0xc2, 0xc3,
    };
    uint8_t *cursor = code;

    memcpy(cursor, prefix, sizeof(prefix));
    cursor += sizeof(prefix);
    memcpy(cursor, adds, sizeof(adds));
    cursor += sizeof(adds);
    memcpy(cursor, suffix, sizeof(suffix));
    cursor += sizeof(suffix);
    return cursor - code;
}

static generated_fn make_generated(uint8_t *code, size_t size,
                                   size_t (*emit)(uint8_t *))
{
    memset(code, 0x90, size);
    emit(code);
    __builtin___clear_cache((char *)code, (char *)code + size);
    return (generated_fn)code;
}

static int stale_local_flags_are_cleared(void)
{
    const size_t page_size = 4096;
    /* Dynamic TBs reuse the translator's static IR1 arena across pages. */
    uint8_t *producer_code = mmap(NULL, page_size, PROT_READ | PROT_WRITE |
                                  PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    uint8_t *consumer_code = mmap(NULL, page_size, PROT_READ | PROT_WRITE |
                                  PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    generated_fn producer;
    generated_fn consumer;
    uint64_t value;

    if (producer_code == MAP_FAILED || consumer_code == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    producer = make_generated(producer_code, page_size, emit_stale_producer);
    consumer = make_generated(consumer_code, page_size, emit_stale_consumer);
    (void)producer();
    value = consumer();
    (void)munmap(producer_code, page_size);
    (void)munmap(consumer_code, page_size);

    printf("stale=%016" PRIx64 "\n", value);
    return value != UINT64_C(0x10);
}

int main(void)
{
    const uint32_t value = UINT32_C(0xfffffff0);
    uint32_t folded = (value + UINT32_C(0x123)) ^ UINT32_C(0x55);
    uint64_t expected_dead = (uint64_t)(folded * UINT32_C(5) + 3);
    uint64_t expected_live = value;
    uint64_t dead = dead_high_chain(value);
    uint64_t live = live_high_guard(value);
    int stale_failed = stale_local_flags_are_cleared();

    printf("dead=%016" PRIx64 " live=%016" PRIx64 "\n", dead, live);
    return dead != expected_dead || live != expected_live || stale_failed;
}
