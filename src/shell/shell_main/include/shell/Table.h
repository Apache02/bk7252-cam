#pragma once

#include <initializer_list>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>


class Table {
public:
    enum class Align { Left, Right };

    struct ColumnDef {
        const char *name;
        int         width;
        const char *format;
        Align       align;
    };

    // ─── Row ─────────────────────────────────────────────────────────────────

    class Row {
    public:
        explicit Row(const Table *table);
        ~Row();

        template<typename... Args>
        Row &set(const char *col_name, Args... args) {
            int i = table_->find_col(col_name);
            if (i < 0) return *this;

            const char *fmt = table_->cols_[i].format;
            int len = snprintf(nullptr, 0, fmt, args...) + 1;
            if (cells_[i]) free(cells_[i]);
            cells_[i] = static_cast<char *>(malloc(len));
            snprintf(cells_[i], len, fmt, args...);
            return *this;
        }

    private:
        friend class Table;
        const Table  *table_;
        char        **cells_;
    };

    // ─── Table ────────────────────────────────────────────────────────────────

    Table(std::initializer_list<ColumnDef> cols);
    Table(const ColumnDef *cols, int col_count);
    ~Table();

    Row  *createRow() const;
    void  printHeader() const;
    void  printRow(const Row *row) const;
    void  printSeparator() const;

private:
    ColumnDef *cols_;
    int        col_count_;

    int  find_col(const char *name) const;
};