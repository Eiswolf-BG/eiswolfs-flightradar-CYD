#include "i18n.h"
#include "i18n_en.h"
#include "i18n_de.h"
#include "i18n_fr.h"
#include "i18n_tr.h"
#include "i18n_es.h"
#include "i18n_it.h"
#include "i18n_pt.h"
#include "settings_store.h"

namespace I18n {

namespace {
    // PT (brasilianisches Portugiesisch) bewusst ANGEHAENGT (Index 6) statt
    // mittendrin einsortiert - SettingsStore::language() speichert nur den
    // numerischen Index auf der SD-Karte; ein Einschieben mitten in die
    // Reihenfolge wuerde auf bereits konfigurierten Geraeten die Sprache
    // verschieben (z.B. wuerde ein gespeichertes "5" fuer Italienisch
    // ploetzlich etwas anderes bedeuten).
    const char* const* TABLES[LANG_COUNT] = {
        I18N_EN, I18N_DE, I18N_FR, I18N_TR, I18N_ES, I18N_IT, I18N_PT
    };

    const char* const LANGUAGE_NAMES[LANG_COUNT] = {
        "English", "Deutsch", "Français", "Türkçe", "Español", "Italiano", "Português"
    };
}

const char* t(StringId id) {
    uint8_t lang = SettingsStore::language();
    if (lang >= LANG_COUNT) lang = 0;

    uint16_t idx = (uint16_t)id;
    if (idx >= (uint16_t)StringId::COUNT) return "?";

    return TABLES[lang][idx];
}

const char* languageName(uint8_t index) {
    if (index >= LANG_COUNT) return "?";
    return LANGUAGE_NAMES[index];
}

}