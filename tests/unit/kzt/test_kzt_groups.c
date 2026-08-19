/*
 * SPDX-FileCopyrightText: 2026 LAT Project Authors
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <stdint.h>
#include <stdlib.h>

#include "kzt-groups.h"

#define CHECK(condition) do {     \
    if (!(condition)) {           \
        abort();                  \
    }                             \
} while (0)

static void test_guest_gl_fallback_disables_x11_interop(void)
{
    kzt_groups_reset();
    CHECK(kzt_groups_configure("stable", false));
    CHECK(kzt_groups_effective_mask() == KZT_GROUP_STABLE);

    CHECK(kzt_group_disable(KZT_GROUP_GL, "test guest GL fallback"));
    CHECK(kzt_groups_effective_mask() == KZT_GROUP_CORE);
    CHECK(kzt_groups_requested_mask() == KZT_GROUP_STABLE);
}

static void test_other_group_fallbacks_keep_x11_enabled(void)
{
    uint32_t expected = KZT_GROUP_STABLE & ~KZT_GROUP_VULKAN;

    kzt_groups_reset();
    CHECK(kzt_groups_configure("stable", false));

    CHECK(kzt_group_disable(KZT_GROUP_VULKAN,
                            "test Vulkan fallback"));
    CHECK(kzt_groups_effective_mask() == expected);
}

int main(void)
{
    test_guest_gl_fallback_disables_x11_interop();
    test_other_group_fallbacks_keep_x11_enabled();
    return 0;
}
