/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <stdint.h>

struct cmp_result {
    uint32_t taken;
    uint64_t flags;
};

static uint32_t first_failure;

static void record_failure(uint32_t reason, uint32_t case_id)
{
    if (!first_failure) {
        first_failure = reason * 256 + case_id;
    }
}

static void compare32_je_reg(uint64_t lhs, uint64_t rhs,
                             struct cmp_result *result)
{
    uint32_t taken;
    uint64_t flags;

    __asm__ volatile("cmpl %k3, %k2\n\t"
                     "jne .Lje_not_taken_%=\n\t"
                     "movl $1, %0\n\t"
                     "jmp .Lje_done_%=\n\t"
                     ".Lje_not_taken_%=:\n\t"
                     "movl $0, %0\n\t"
                     ".Lje_done_%=:\n\t"
                     "pushfq\n\t"
                     "popq %1\n\t"
                     : "=&r"(taken), "=&r"(flags)
                     : "r"(lhs), "r"(rhs)
                     : "cc", "memory");
    result->taken = taken;
    result->flags = flags;
}

static void compare32_jne_reg(uint64_t lhs, uint64_t rhs,
                              struct cmp_result *result)
{
    uint32_t taken;
    uint64_t flags;

    __asm__ volatile("cmpl %k3, %k2\n\t"
                     "je .Ljne_not_taken_%=\n\t"
                     "movl $1, %0\n\t"
                     "jmp .Ljne_done_%=\n\t"
                     ".Ljne_not_taken_%=:\n\t"
                     "movl $0, %0\n\t"
                     ".Ljne_done_%=:\n\t"
                     "pushfq\n\t"
                     "popq %1\n\t"
                     : "=&r"(taken), "=&r"(flags)
                     : "r"(lhs), "r"(rhs)
                     : "cc", "memory");
    result->taken = taken;
    result->flags = flags;
}

static void compare32_je_mem_reg(uint32_t lhs, uint64_t rhs,
                                 struct cmp_result *result)
{
    uint32_t taken;
    uint64_t flags;

    __asm__ volatile("cmpl %k3, %1\n\t"
                     "jne .Ljem_not_taken_%=\n\t"
                     "movl $1, %0\n\t"
                     "jmp .Ljem_done_%=\n\t"
                     ".Ljem_not_taken_%=:\n\t"
                     "movl $0, %0\n\t"
                     ".Ljem_done_%=:\n\t"
                     "pushfq\n\t"
                     "popq %2\n\t"
                     : "=&r"(taken), "+m"(lhs), "=r"(flags)
                     : "r"(rhs)
                     : "cc", "memory");
    result->taken = taken;
    result->flags = flags;
}

static void compare32_jne_imm(uint64_t lhs, struct cmp_result *result)
{
    uint32_t taken;
    uint64_t flags;

    __asm__ volatile("cmpl $0x80000000, %k2\n\t"
                     "je .Ljnei_not_taken_%=\n\t"
                     "movl $1, %0\n\t"
                     "jmp .Ljnei_done_%=\n\t"
                     ".Ljnei_not_taken_%=:\n\t"
                     "movl $0, %0\n\t"
                     ".Ljnei_done_%=:\n\t"
                     "pushfq\n\t"
                     "popq %1\n\t"
                     : "=&r"(taken), "=r"(flags)
                     : "r"(lhs)
                     : "cc", "memory");
    result->taken = taken;
    result->flags = flags;
}

static void compare64_je_reg(uint64_t lhs, uint64_t rhs,
                             struct cmp_result *result)
{
    uint32_t taken;
    uint64_t flags;

    __asm__ volatile("cmpq %3, %2\n\t"
                     "jne .Lje64_not_taken_%=\n\t"
                     "movl $1, %0\n\t"
                     "jmp .Lje64_done_%=\n\t"
                     ".Lje64_not_taken_%=:\n\t"
                     "movl $0, %0\n\t"
                     ".Lje64_done_%=:\n\t"
                     "pushfq\n\t"
                     "popq %1\n\t"
                     : "=&r"(taken), "=&r"(flags)
                     : "r"(lhs), "r"(rhs)
                     : "cc", "memory");
    result->taken = taken;
    result->flags = flags;
}

static uint64_t expected_flags(uint32_t lhs, uint32_t rhs)
{
    uint32_t result = lhs - rhs;
    uint32_t msb = 0x80000000u;
    uint64_t flags = 0;
    uint32_t low = result & 0xff;
    uint32_t parity = low;

    if (lhs < rhs) {
        flags |= 1u << 0;
    }
    parity ^= parity >> 4;
    parity ^= parity >> 2;
    parity ^= parity >> 1;
    if (!(parity & 1)) {
        flags |= 1u << 2;
    }
    if ((lhs ^ rhs ^ result) & 0x10) {
        flags |= 1u << 4;
    }
    if (!result) {
        flags |= 1u << 6;
    }
    if (result & msb) {
        flags |= 1u << 7;
    }
    if (((lhs ^ rhs) & (lhs ^ result) & msb) != 0) {
        flags |= 1u << 11;
    }
    return flags;
}

#define ARCH_FLAGS_MASK 0x8d5u

static void check_result(const struct cmp_result *actual, uint32_t lhs,
                         uint32_t rhs, uint32_t expected_taken, uint32_t reason,
                         uint32_t case_id)
{
    if (actual->taken != expected_taken ||
        ((uint32_t)actual->flags & ARCH_FLAGS_MASK) !=
            ((uint32_t)expected_flags(lhs, rhs) & ARCH_FLAGS_MASK)) {
        record_failure(reason, case_id);
    }
}

static void run_cases(void)
{
    static const uint32_t values[] = {
        0x00000000u, 0x00000001u, 0x7fffffffu, 0x80000000u,
        0xffffffffu, 0x13572468u, 0x2468ace0u,
    };
    uint32_t lhs_index;
    uint32_t rhs_index;
    struct cmp_result result;

    for (lhs_index = 0; lhs_index < sizeof(values) / sizeof(values[0]);
         lhs_index++) {
        for (rhs_index = 0; rhs_index < sizeof(values) / sizeof(values[0]);
             rhs_index++) {
            uint32_t lhs = values[lhs_index];
            uint32_t rhs = values[rhs_index];
            uint64_t dirty_lhs = UINT64_C(0xdeadbeef00000000) | lhs;
            uint64_t dirty_rhs = UINT64_C(0xfeedface00000000) | rhs;
            uint32_t case_id = lhs_index * 16 + rhs_index;

            compare32_je_reg(dirty_lhs, dirty_rhs, &result);
            check_result(&result, lhs, rhs, lhs == rhs, 1, case_id);
            compare32_jne_reg(dirty_lhs, dirty_rhs, &result);
            check_result(&result, lhs, rhs, lhs != rhs, 2, case_id);
            compare32_je_mem_reg(lhs, dirty_rhs, &result);
            check_result(&result, lhs, rhs, lhs == rhs, 3, case_id);
        }
    }

    compare32_jne_imm(UINT64_C(0x1234567880000000), &result);
    check_result(&result, 0x80000000u, 0x80000000u, 0, 4, 0);
    compare32_jne_imm(UINT64_C(0x123456787fffffff), &result);
    check_result(&result, 0x7fffffffu, 0x80000000u, 1, 5, 0);

    compare64_je_reg(UINT64_C(0x1111111100000001), UINT64_C(0x2222222200000001),
                     &result);
    if (result.taken != 0) {
        record_failure(6, 0);
    }
    compare64_je_reg(UINT64_C(0x1111111100000001), UINT64_C(0x1111111100000001),
                     &result);
    if (result.taken != 1) {
        record_failure(7, 0);
    }
}

int main(void)
{
    run_cases();
    return first_failure;
}
