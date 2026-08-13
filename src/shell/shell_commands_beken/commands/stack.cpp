#include "shell/commands_beken.h"
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include "shell/Table.h"
#include "shell/console_colors.h"
#include "platform/stacks.h"

#undef count_of
#define count_of(x) (sizeof(x) / sizeof(x[0]))

// clang-format off
static const struct {
    const char    *name;
    uint32_t       color;
    const uint8_t *start;
    size_t         size;
} stack_regions[] = {
    {"unused", STACK_COLOR_UNUSED, _stack_unused, _UNUSED_STACK_SIZE_},
    {"svc", STACK_COLOR_SVC, _stack_svc, _SVC_STACK_SIZE_},
    {"irq", STACK_COLOR_IRQ, _stack_irq, _IRQ_STACK_SIZE_},
    {"fiq", STACK_COLOR_FIQ, _stack_fiq, _FIQ_STACK_SIZE_},
    {"sys", STACK_COLOR_SYS, _stack_sys, _SYS_STACK_SIZE_},
};
// clang-format on

// clang-format off
static const Table::ColumnDef table_def[] = {
    {"name", 10, "%s", Table::Align::Left},
    {"color", 10, "0x%08lx", Table::Align::Right},
    {"addr", 10, "0x%08lx", Table::Align::Right},
    {"size (hex)", 10, "0x%lx", Table::Align::Right},
    {"used", 8, "0x%lx", Table::Align::Right},
    {"%", 4, "%3d%%", Table::Align::Right},
};
// clang-format on

int command_stack(__unused int argc, __unused const char *argv[]) {
    auto *table = new Table(table_def, count_of(table_def));

    table->printHeader();

    auto *begin_row = table->createRow();
    begin_row->set("addr", &_stack_begin);
    table->printRow(begin_row);
    delete begin_row;

    for (const auto &r : stack_regions) {
        int       count = r.size / sizeof(uint32_t);
        uint32_t *base  = reinterpret_cast<uint32_t *>(const_cast<uint8_t *>(r.start));

        // Scan up from the bottom for the deepest word the stack ever disturbed.
        // Counting down from the top instead would stop early: a C frame can
        // leave slots uninitialised, so the fill pattern reappears inside the
        // region that was actually used.
        int untouched = 0;
        while ((untouched < count) && base[untouched] == r.color) untouched++;
        int used = count - untouched;

        auto *row = table->createRow();
        row->set("name", r.name);
        row->set("color", r.color);
        row->set("addr", r.start);
        row->set("size (hex)", r.size);
        row->set("used", used * sizeof(uint32_t));
        row->set("%", static_cast<int>(used * sizeof(uint32_t) * 100 / r.size));
        table->printRow(row);
        delete row;
    }

    auto *end_row = table->createRow();
    end_row->set("addr", &_stack_end);
    table->printRow(end_row);
    delete end_row;

    delete table;

    printf("\r\n");

    return 0;
}
