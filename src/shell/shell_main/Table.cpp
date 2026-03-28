#include "shell/Table.h"
#include <stdio.h>

// ─── Row ─────────────────────────────────────────────────────────────────────

Table::Row::Row(const Table *table) : table_(table) {
    int n = table_->col_count_;
    cells_ = new char *[n];
    for (int i = 0; i < n; i++) cells_[i] = nullptr;
}

Table::Row::~Row() {
    for (int i = 0; i < table_->col_count_; i++) {
        if (cells_[i]) free(cells_[i]);
    }
    delete[] cells_;
}

// ─── Table ───────────────────────────────────────────────────────────────────

Table::Table(std::initializer_list<ColumnDef> cols) {
    col_count_ = cols.size();
    cols_ = new ColumnDef[col_count_];
    int i = 0;
    for (const auto &c: cols) cols_[i++] = c;
}

Table::Table(const ColumnDef *cols, int col_count)
    : col_count_(col_count) {
    cols_ = new ColumnDef[col_count_];
    for (int i = 0; i < col_count_; i++) cols_[i] = cols[i];
}

Table::~Table() {
    delete[] cols_;
}

Table::Row *Table::createRow() const {
    return new Row(this);
}

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
        printf("+-");
        for (int j = 0; j < cols_[i].width; j++) putchar('-');
        printf("-");
    }
    printf("+\r\n");
}
