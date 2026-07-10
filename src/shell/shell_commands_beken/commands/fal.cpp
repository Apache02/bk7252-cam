#include "shell/commands_beken.h"
#include "shell/Table.h"
#include "platform/fal.h"
#include <stdio.h>

#undef count_of
#define count_of(x) (sizeof(x) / sizeof(x[0]))

static void format_size(char *buf, size_t buf_len, size_t size) {
    if (size >= 1024 * 1024) {
        const unsigned whole = size / (1024 * 1024);
        const unsigned frac  = (size % (1024 * 1024)) * 100 / (1024 * 1024);
        snprintf(buf, buf_len, "%u.%02u MB", whole, frac);
    } else if (size >= 1024) {
        const unsigned whole = size / 1024;
        const unsigned frac  = (size % 1024) * 100 / 1024;
        snprintf(buf, buf_len, "%u.%02u KB", whole, frac);
    } else {
        snprintf(buf, buf_len, "%u B", static_cast<unsigned int>(size));
    }
}

static const Table::ColumnDef table_def[] = {
    {"name", 16, "%s", Table::Align::Left},          {"flash name", 16, "%s", Table::Align::Left},
    {"start", 10, "0x%08lx", Table::Align::Right},   {"end", 10, "0x%08lx", Table::Align::Right},
    {"size (hex)", 10, "0x%x", Table::Align::Right}, {"size (human)", 12, "%s", Table::Align::Right},
};

int command_partitions(__unused int argc, __unused const char *argv[]) {
    const fal_partition_t *tbl = fal_partition_table();

    if (!tbl) {
        printf("Error: Partitions table not found!\r\n");
        return 1;
    }

    auto table = new Table(table_def, count_of(table_def));

    table->printHeader();

    char size_human[16];
    for (; tbl->magic_word == FAL_PART_MAGIC_WORD; tbl++) {
        format_size(size_human, sizeof(size_human), tbl->len);

        auto *row = table->createRow();
        row->set("name", tbl->name);
        row->set("flash name", tbl->flash_name);
        row->set("start", tbl->offset);
        row->set("end", tbl->offset + tbl->len - 1);
        row->set("size (hex)", tbl->len);
        row->set("size (human)", size_human);

        table->printRow(row);
        delete row;
    }
    delete table;

    printf("\r\n");

    return 0;
}
