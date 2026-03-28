#include "shell/commands_beken.h"
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include "shell/Table.h"


#define count_of(a)  (sizeof(a) / sizeof((a)[0]))

extern "C" uint8_t _stack_unused;
extern "C" uint8_t _stack_svc;
extern "C" uint8_t _stack_irq;
extern "C" uint8_t _stack_fiq;
extern "C" uint8_t _stack_sys;

static const uint8_t *const ram_end = reinterpret_cast<uint8_t *>(0x00440000);

static const struct {
    const char *name;
    uint32_t color;
    const uint8_t *start;
    const uint8_t *end;
} stack_regions[] = {
    {"unused", 0xaaaaaaaa, &_stack_unused, &_stack_svc},
    {"svc", 0xbbbbbbbb, &_stack_svc, &_stack_irq},
    {"irq", 0xcccccccc, &_stack_irq, &_stack_fiq},
    {"fiq", 0xdddddddd, &_stack_fiq, &_stack_sys},
    {"sys", 0xeeeeeeee, &_stack_sys, ram_end},
};

static const Table::ColumnDef table_def[] = {
    {"name", 10, "%s", Table::Align::Left},
    {"color", 10, "0x%08lx", Table::Align::Right},
    {"start", 10, "0x%08lx", Table::Align::Right},
    {"end", 10, "0x%08lx", Table::Align::Right},
    {"size", 8, "0x%lx", Table::Align::Right},
    {"used", 8, "0x%lx", Table::Align::Right},
    {"p", 4, "%3d%%", Table::Align::Right},
};


int command_stack(__unused int argc, __unused const char *argv[]) {
    auto *table = new Table(table_def, count_of(table_def));

    table->printHeader();

    for (const auto &r: stack_regions) {
        size_t size = r.end - r.start;
        int count = size / sizeof(uint32_t);
        uint32_t *top = reinterpret_cast<uint32_t *>(const_cast<uint8_t *>(r.end));
        int used = 0;
        while ((used < count) && top[-1 - used] != r.color) used++;

        auto *row = table->createRow();
        row->set("name", r.name);
        row->set("color", r.color);
        row->set("start", reinterpret_cast<uint32_t>(r.start));
        row->set("end", reinterpret_cast<uint32_t>(r.end - 1));
        row->set("size", static_cast<uint32_t>(size));
        row->set("used", used * sizeof(uint32_t));
        row->set("p", static_cast<int>(used * sizeof(uint32_t) * 100 / size));
        table->printRow(row);
        delete row;
    }

    delete table;

    return 0;
}
