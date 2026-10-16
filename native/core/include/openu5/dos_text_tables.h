#pragma once
// T-Deck Native View Phase 3 (docs/TDECK_NATIVE_VIEW_PHASE3.md): the DOS
// game's own short names for its right-panel lists (openu5::DosText), read from
// the player's DATA.OVL dump the resource pack already carries (data.json) into
// ONE fixed block -- no allocation, no JSON tree, no std::map. Only what the
// 1988 panels print is kept: the potion colours, the reagents, the spells, the
// 38 (U)se entries (DS 0x1916, assembled as the Windows port assembles them)
// and the DGROUP literals of kDosTextLiteralAddresses.
//
// The equipment tables (DS 0x1962 names, DS 0x1ae8 glyphs) are not in
// data.json: the platform serves them from the copies the repository tracks
// for the web port (cmake/DosTables.cmake), as Windows does.
//
// Strings are bytes: an escaped \uXXXX, or a UTF-8 sequence, below 0x100 is
// that byte (the dump writes DATA.OVL's bytes 0x80..0xff as UTF-8), so every
// literal's DS address is the string pool's offset plus the decoded lengths.
#include <cstddef>
#include <cstdint>

#include "openu5/platform_profile.h"

namespace openu5 {

struct DosTextBlock {
    static constexpr size_t kArenaBytes = 1536;
    static constexpr size_t kLiterals = 9;
    char arena[kArenaBytes]{};
    uint16_t used = 0;
    // Arena offset + 1 of each entry; 0 = none.
    uint16_t potion[8]{}, reagent[8]{}, spell[48]{}, use_item[38]{};
    uint16_t literal[kLiterals]{};
    bool loaded = false;
};

/** The DGROUP literals the panels print (DosText::Literal), by DS address. */
extern const uint16_t kDosTextLiteralAddresses[DosTextBlock::kLiterals];

/**
 * Extracts the tables from data.json (`length` bytes, need not be terminated).
 * True when every table is complete; otherwise `out` is cleared (not loaded).
 */
bool dos_text_extract(const char *json, size_t length, DosTextBlock &out);

/** The block's text, or nullptr (not loaded, no such entry, or an equipment table). */
const char *dos_text_lookup(const DosTextBlock &block, DosText table, int index);

} // namespace openu5
