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
// Only presentation is installed (dos_screen, dos_panels). The prototype
// image fixes the interaction to the handheld's at compile time
// (OPENU5_FIXED_HANDHELD_INTERACTION): the 1988 interaction model is dead code.
#include <cstdint>

namespace tdeck {

enum class PrototypePresentation : uint8_t { Native, Enhanced };

constexpr char kPrototypePresentationPath[] = "/sd/ultima5/dev/presentation.txt";

/** The marker's choice: a first word "enhanced" (any case) is Enhanced; anything else (or null) Native. */
PrototypePresentation parse_prototype_presentation(const char *text);
/** Reads the marker (its first 32 bytes); no file is Native. */
PrototypePresentation read_prototype_presentation(const char *path = kPrototypePresentationPath);
/** Installs the presentation-only platform profile for `presentation`. */
void install_prototype_presentation(PrototypePresentation presentation);
const char *prototype_presentation_name(PrototypePresentation presentation);

} // namespace tdeck
