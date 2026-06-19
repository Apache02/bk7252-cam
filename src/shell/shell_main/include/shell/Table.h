#pragma once

#include <initializer_list>

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

        Row *set(const char *col_name, ...);

      private:
        friend class Table;
        const Table *table_;
        char       **cells_;
    };

    // ─── Table ────────────────────────────────────────────────────────────────

    Table(std::initializer_list<ColumnDef> cols);
    Table(const ColumnDef *cols, int col_count);
    ~Table();

    Row *createRow() const;
    void printHeader() const;
    void printRow(const Row *row) const;
    void printSeparator() const;

  private:
    ColumnDef *cols_;
    int        col_count_;

    int find_col(const char *name) const;
};