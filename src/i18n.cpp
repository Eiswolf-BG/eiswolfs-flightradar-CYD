#include "i18n.h"
#include "i18n_en.h"
#include "i18n_de.h"
#include "i18n_fr.h"
#include "i18n_tr.h"
#include "i18n_es.h"
#include "i18n_it.h"
#include "settings_store.h"

namespace I18n {

namespace {
    const char* const* TABLES[LANG_COUNT] = {
        I18N_EN, I18N_DE, I18N_FR, I18N_TR, I18N_ES, I18N_IT
    };

    const char* const LANGUAGE_NAMES[LANG_COUNT] = {
        "English", "Deutsch", "Français", "Türkçe", "Español", "Italiano"
    };
}

const char* t(StringId id) {
    uint8_t lang = SettingsStore::language();
    if (lang >= LANG_COUNT) lang = 0;

    uint8_t idx = (uint8_t)id;
    if (idx >= (uint8_t)StringId::COUNT) return "?";

    return TABLES[lang][idx];
}

const char* languageName(uint8_t index) {
    if (index >= LANG_COUNT) return "?";
    return LANGUAGE_NAMES[index];
}

}