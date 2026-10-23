// N11: the table positions (addresses and sizes only; made from docs/notes/rom_data_tables.md of step N10).
#include "rom_data.h"

namespace romdata {

namespace {
const TableInfo kTables[] = {
    {"T01", "small_tables_06be", 0x0068, 24},
    {"T02", "table_01dc", 0x0219, 7},
    {"T03", "level_records", 0x0796, 126},
    {"T04", "speed_timing", 0x0843, 54},
    {"T05", "fruit_table", 0x0EFD, 63},
    {"T06", "lists_2728", 0x2728, 8},
    {"T07", "points_table", 0x2B17, 28},
    {"T08", "code_bytes_2b73", 0x2B73, 5},
    {"T09", "self_test_table", 0x3154, 24},
    {"T10", "table_32f9", 0x32F9, 6},
    {"T11", "direction_tables", 0x32FF, 14},
    {"T12", "table_338d", 0x338D, 168},
    {"T13", "maze_map", 0x3435, 381},
    {"T14", "dot_positions", 0x35B5, 240},
    {"T15", "lists_36a5", 0x36A5, 108},
    {"T16", "text_strings_a", 0x3713, 702},
    {"T17", "table_3af4", 0x3A4F, 147},
    {"T18", "table_3ae2", 0x3AE2, 18},
    {"T19", "sound_3b08", 0x3B08, 40},
    {"T20", "sound_3b30", 0x3B30, 128},
    {"T21", "sound_lists_3bb0", 0x3BB0, 36},
    {"T22", "sound_3bd4", 0x3BD4, 300},
    {"T23", "text_strings_b", 0x3D00, 346},
};
static_assert(sizeof(kTables) / sizeof(kTables[0]) == static_cast<size_t>(Table::Count), "one entry per table");
}  // namespace

const TableInfo &info(Table t) { return kTables[static_cast<size_t>(t)]; }

View RomData::table(Table t) const {
    const TableInfo &i = info(t);
    return {rom_ + i.first, i.first, i.size};
}

}  // namespace romdata
