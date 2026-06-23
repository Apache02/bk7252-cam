#pragma once

#include "platform/soc.h"


#define INTC_BASE_ADDR (0x10910000)


typedef volatile struct {
    uint32_t irq_status[2];          // 0x00..0x04
    uint32_t irq_raw_status[2];      // 0x08..0x0C
    uint32_t irq_unmask_set[2];      // 0x10..0x14
    uint32_t irq_unmask_clear[2];    // 0x18..0x1C
    uint32_t irq_polarity[2];        // 0x20..0x24
    uint32_t reserved_0x28_0x3C[6]; // 0x28..0x3C
    uint32_t irq_index;              // 0x40
} hw_intc_t;

#define hw_intc ((volatile hw_intc_t *)INTC_BASE_ADDR)
