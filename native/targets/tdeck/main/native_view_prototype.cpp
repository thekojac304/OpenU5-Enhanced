#include "native_view_prototype.h"

#include <cctype>
#include <cstdio>
#include <cstring>

#include "openu5/platform_profile.h"

namespace tdeck {
namespace {
bool g_native = false;
bool native_presentation(void *) { return g_native; }
} // namespace

PrototypePresentation parse_prototype_presentation(const char *text) {
    if (!text) return PrototypePresentation::Native;
    while (*text && std::isspace(static_cast<unsigned char>(*text))) ++text;
    constexpr char kEnhanced[] = "enhanced";
    for (size_t i = 0; i < sizeof(kEnhanced) - 1; ++i)
        if (std::tolower(static_cast<unsigned char>(text[i])) != kEnhanced[i]) return PrototypePresentation::Native;
    const char after = text[sizeof(kEnhanced) - 1];
    return after == 0 || std::isspace(static_cast<unsigned char>(after)) ? PrototypePresentation::Enhanced
                                                                         : PrototypePresentation::Native;
}

PrototypePresentation read_prototype_presentation(const char *path) {
    char text[33]{};
    if (FILE *file = path ? std::fopen(path, "rb") : nullptr) {
        const size_t n = std::fread(text, 1, sizeof(text) - 1, file);
        text[n] = 0;
        std::fclose(file);
        return parse_prototype_presentation(text);
    }
    return PrototypePresentation::Native;
}

void install_prototype_presentation(PrototypePresentation presentation) {
    g_native = presentation == PrototypePresentation::Native;
    // The T-Deck's own words and rows (the default profile) plus the two
    // presentation switches. No interaction or journey switch is installed:
    // the handheld's are the defaults, and the build fixes the interaction.
    openu5::PlatformProfile profile{};
    profile.dos_screen = &native_presentation;
    profile.dos_panels = &native_presentation;
    openu5::set_platform_profile(profile);
}

const char *prototype_presentation_name(PrototypePresentation presentation) {
    return presentation == PrototypePresentation::Native ? "native" : "enhanced";
}

} // namespace tdeck
