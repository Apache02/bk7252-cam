#include "shell/Table.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>


// ─── Row ─────────────────────────────────────────────────────────────────────

Table::Row::Row(const Table *table) : table_(table) {
    int n  = table_->col_count_;
    cells_ = new char *[n];
    for (int i = 0; i < n; i++) cells_[i] = nullptr;
}

Table::Row::~Row() {
    for (int i = 0; i < table_->col_count_; i++) {
        if (cells_[i]) free(cells_[i]);
    }
    delete[] cells_;
}

Table::Row *Table::Row::set(const char *col_name, ...) {
    int i = table_->find_col(col_name);
    if (i < 0) return this;

    const char *fmt = table_->cols_[i].format;

    va_list args, args2;
    va_start(args, col_name);
    va_copy(args2, args);

    int len = vsnprintf(nullptr, 0, fmt, args) + 1;
    if (cells_[i]) free(cells_[i]);
    cells_[i] = static_cast<char *>(malloc(len));
    vsnprintf(cells_[i], len, fmt, args2);

    va_end(args);
    va_end(args2);

    return this;
}

// ─── Table ───────────────────────────────────────────────────────────────────

Table::Table(std::initializer_list<ColumnDef> cols) {
    col_count_ = cols.size();
    cols_      = new ColumnDef[col_count_];
    int i      = 0;
    for (const auto &c : cols) cols_[i++] = c;
}

Table::Table(const ColumnDef *cols, int col_count) : col_count_(col_count) {
    cols_ = new ColumnDef[col_count_];
    for (int i = 0; i < col_count_; i++) cols_[i] = cols[i];
}

Table::~Table() { delete[] cols_; }

Table::Row *Table::createRow() const { return new Row(this); }

void Table::printHeader() const {
    for (int i = 0; i < col_count_; i++) {
        printf("| %-*s ", cols_[i].width, cols_[i].name);
    }
    printf("|\r\n");
    printSeparator();
}

void Table::printRow(const Row *row) const {
    for (int i = 0; i < col_count_; i++) {
        const char *cell = row->cells_[i] ? row->cells_[i] : "";
        if (cols_[i].align == Align::Right) {
            printf("| %*s ", cols_[i].width, cell);
        } else {
            printf("| %-*s ", cols_[i].width, cell);
        }
    }
    printf("|\r\n");
}

int Table::find_col(const char *name) const {
    for (int i = 0; i < col_count_; i++) {
        if (strcmp(cols_[i].name, name) == 0) return i;
    }
    return -1;
}

void Table::printSeparator() const {
    for (int i = 0; i < col_count_; i++) {
        putchar('+');
        putchar('-');
        for (int j = 0; j < cols_[i].width; j++) putchar('-');
        putchar('-');
    }
    putchar('+');
    putchar('\r');
    putchar('\n');
}
