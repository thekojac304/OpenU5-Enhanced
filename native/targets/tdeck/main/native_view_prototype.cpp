#include "native_view_prototype.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <new>

#include "alpha_resources.h"
#include "esp_heap_caps.h"
#include "openu5/dos_text_tables.h"
#include "openu5/platform_profile.h"

namespace tdeck {
namespace {
#include "dos_equipment_tables.inc"

bool g_native = false;
bool native_presentation(void *) { return g_native; }
// The DATA.OVL block (PSRAM, made on the first load, kept for the session).
openu5::DosTextBlock *g_dos_text = nullptr;
char g_equipment_glyph[48][2]{};
const char *dos_text_hook(void *, uint8_t table, int index) { return prototype_dos_text(table, index); }
openu5::DosTextBlock *dos_text_block() {
    if (!g_dos_text) {
        void *memory = heap_caps_malloc(sizeof(openu5::DosTextBlock), MALLOC_CAP_SPIRAM);
        if (memory) g_dos_text = new (memory) openu5::DosTextBlock();
    }
    return g_dos_text;
}
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
    // Phase 3: the DOS names. Only the DOS panels ask (compose_dos_panel and
    // the Native shop / list forms, all behind platform_dos_panels()).
    profile.dos_text = &dos_text_hook;
    openu5::set_platform_profile(profile);
}

bool load_prototype_dos_text_json(const char *json, size_t length) {
    auto *block = dos_text_block();
    return block && openu5::dos_text_extract(json, length, *block);
}

bool load_prototype_dos_text(const AlphaResourcePack &pack) {
    const uint32_t length = pack.entry_length("data.json");
    if (!length) return false;
    // The document only while it is read: ~54 KB of PSRAM, freed at once.
    auto *json = static_cast<char *>(heap_caps_malloc(length, MALLOC_CAP_SPIRAM));
    if (!json) return false;
    const bool ok = pack.read_entry("data.json", json, length) == ESP_OK && load_prototype_dos_text_json(json, length);
    heap_caps_free(json);
    return ok;
}

bool prototype_dos_text_loaded() { return g_dos_text && g_dos_text->loaded; }

const char *prototype_dos_text(uint8_t table, int index) {
    switch (openu5::DosText(table)) {
    case openu5::DosText::EquipmentShort: return index >= 0 && index < 48 ? kDosEquipmentShort[index] : nullptr;
    case openu5::DosText::EquipmentGlyph:
        if (index < 0 || index >= 48) return nullptr;
        g_equipment_glyph[index][0] = char(kDosEquipmentGlyph[index]);
        return g_equipment_glyph[index];
    default: return g_dos_text ? openu5::dos_text_lookup(*g_dos_text, openu5::DosText(table), index) : nullptr;
    }
}

const char *prototype_presentation_name(PrototypePresentation presentation) {
    return presentation == PrototypePresentation::Native ? "native" : "enhanced";
}

} // namespace tdeck
