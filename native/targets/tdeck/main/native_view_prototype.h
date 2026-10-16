#pragma once
// T-Deck Native View Phase 2 (docs/TDECK_NATIVE_VIEW_PHASE2.md): the
// presentation the DEVELOPMENT prototype image (OPENU5_NATIVE_VIEW_PROTOTYPE)
// shows, chosen once at boot. The production image does not compile this.
//
// The choice is a developer marker file on the SD card, which the firmware
// only reads (never writes): its first word "enhanced" (any case) selects the
// handheld screen; anything else, or no file, selects the Native (DOS
// 320 x 200) screen -- the prototype exists to test it. It is not a player
// setting: nothing in settings.json or a save records it, and changing it
// takes a reboot, so the presentation never changes in the middle of play.
//
// Only presentation is installed (dos_screen, dos_panels, dos_text). The
// prototype image fixes the interaction to the handheld's at compile time
// (OPENU5_FIXED_HANDHELD_INTERACTION): the 1988 interaction model is dead code.
//
// Phase 3 (docs/TDECK_NATIVE_VIEW_PHASE3.md): the DOS game's own short names
// for the Native panels (openu5::DosText). The DATA.OVL tables come from the
// resource pack's data.json, read ONCE at boot while the pack is open and
// reduced to one fixed ~1.8 KB block in PSRAM (openu5::DosTextBlock; the
// document's buffer is freed at once); the equipment names and glyphs are
// compiled in from the copies the repository tracks for the web port
// (dos_equipment_tables.inc, cmake/DosTables.cmake), as on Windows. They are
// loaded whichever presentation boots, so a later switch finds them ready.
#include <cstddef>
#include <cstdint>

namespace tdeck {
class AlphaResourcePack;

enum class PrototypePresentation : uint8_t { Native, Enhanced };

constexpr char kPrototypePresentationPath[] = "/sd/ultima5/dev/presentation.txt";

/** The marker's choice: a first word "enhanced" (any case) is Enhanced; anything else (or null) Native. */
PrototypePresentation parse_prototype_presentation(const char *text);
/** Reads the marker (its first 32 bytes); no file is Native. */
PrototypePresentation read_prototype_presentation(const char *path = kPrototypePresentationPath);
/** Installs the presentation-only platform profile for `presentation`. */
void install_prototype_presentation(PrototypePresentation presentation);
const char *prototype_presentation_name(PrototypePresentation presentation);

/**
 * Reads the DATA.OVL tables from the open pack's data.json (see above). False
 * when the pack has none or it does not parse: the panels then keep the
 * device's own names (the equipment tables still apply).
 */
bool load_prototype_dos_text(const AlphaResourcePack &pack);
/** The same from a data.json document (the tests'); `json` need not be terminated. */
bool load_prototype_dos_text_json(const char *json, size_t length);
/** Whether the DATA.OVL tables are loaded. */
bool prototype_dos_text_loaded();
/** The profile's dos_text hook (openu5::DosText `table`, its index). */
const char *prototype_dos_text(uint8_t table, int index);

} // namespace tdeck
