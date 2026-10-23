// N11: the ONLY code that reads the player's program ROM as data.
// The native game gets its tables from here, at run time, from the ROM image the player supplies (16384 bytes, 0000-3FFF).
// This file holds addresses and sizes only: no ROM bytes (the table list is docs/notes/rom_data_tables.md, made by step N10).
// Facts that are not expression (speeds, times, points rules) stay in src/logic; here are only the bytes the original routines read.
#pragma once
#include <cstddef>
#include <cstdint>

namespace romdata {

// A read-only window on a table inside the player's ROM image.
struct View {
    const uint8_t *data = nullptr;
    uint16_t first = 0;   // ROM address of the first byte
    uint16_t size = 0;
    uint8_t operator[](size_t i) const { return data[i]; }
    uint8_t at_address(uint16_t rom_address) const { return data[rom_address - first]; }
    bool contains(uint16_t rom_address) const { return rom_address >= first && rom_address < first + size; }
};

// One entry per table of docs/notes/rom_data_tables.md (T01 to T23).
enum class Table : uint8_t {
    T01_small_tables_06be, T02_table_01dc, T03_level_records, T04_speed_timing, T05_fruit_table, T06_lists_2728, T07_points_table,
    T08_code_bytes_2b73, T09_self_test_table, T10_table_32f9, T11_direction_tables, T12_table_338d, T13_maze_map, T14_dot_positions,
    T15_lists_36a5, T16_text_strings_a, T17_table_3af4, T18_table_3ae2, T19_sound_3b08, T20_sound_3b30, T21_sound_lists_3bb0,
    T22_sound_3bd4, T23_text_strings_b,
    Count
};

struct TableInfo {
    const char *id;      // "T13"
    const char *name;
    uint16_t first;      // ROM address
    uint16_t size;
};

const TableInfo &info(Table t);

class RomData {
public:
    // rom must point to the 16384-byte program ROM image and stay valid as long as this object is used.
    explicit RomData(const uint8_t *rom) : rom_(rom) {}

    View table(Table t) const;

    // typed functions: one per table the native game needs (each is table(...) under a clear name)
    View level_records() const { return table(Table::T03_level_records); }
    View speed_and_timing_tables() const { return table(Table::T04_speed_timing); }
    View fruit_table() const { return table(Table::T05_fruit_table); }
    View points_table() const { return table(Table::T07_points_table); }
    View direction_tables() const { return table(Table::T11_direction_tables); }
    View maze_map() const { return table(Table::T13_maze_map); }
    View dot_positions() const { return table(Table::T14_dot_positions); }
    View text_strings_a() const { return table(Table::T16_text_strings_a); }
    View text_strings_b() const { return table(Table::T23_text_strings_b); }
    View sound_tables_3b08() const { return table(Table::T19_sound_3b08); }
    View sound_tables_3b30() const { return table(Table::T20_sound_3b30); }
    View sound_lists_3bb0() const { return table(Table::T21_sound_lists_3bb0); }
    View sound_tables_3bd4() const { return table(Table::T22_sound_3bd4); }

    // the special reads of the map (S01 to S03)
    uint8_t random_source_byte(uint16_t rom_address) const { return rom_[rom_address & 0x3FFF]; }   // S02: the random routine indexes the ROM with its seed
    uint16_t checksum_range_end() const { return 0x3000; }                                         // S01: the power-on test sums 0000-2FFF
    View interrupt_vector() const { return {rom_ + 0x3FFA, 0x3FFA, 4}; }                           // S03

private:
    const uint8_t *rom_;
};

}  // namespace romdata
