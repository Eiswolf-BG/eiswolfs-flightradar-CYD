#include "changelog.h"
#include "settings_store.h"

namespace Config {

namespace {
    // Reihenfolge MUSS zu I18n::TABLES (i18n.cpp) und damit zu
    // SettingsStore::language() passen: EN, DE, FR, TR, ES, IT, PT, NL.
    constexpr uint8_t CHANGELOG_LANG_COUNT = 8;

    const char* const CHANGELOG_EN =
        "- Fix: MQTT username and password were always saved in "
        "uppercase regardless of what was typed. The on-screen "
        "keyboard's shift key now behaves like a smartphone (active "
        "only for the next letter), and the exact case you type is "
        "preserved\n"
        "- Fix: The MQTT password field now shows plain text instead "
        "of asterisks, so typos are immediately visible";

    const char* const CHANGELOG_DE =
        "- Fix: MQTT-Benutzername und -Passwort wurden beim "
        "Eintippen unabhängig von der tatsächlichen Eingabe immer "
        "in Großbuchstaben gespeichert. Die Umschalttaste der "
        "Bildschirmtastatur verhält sich jetzt wie bei einem "
        "Smartphone (nur für den nächsten Buchstaben aktiv), und "
        "die exakte Schreibweise wird übernommen\n"
        "- Fix: Das MQTT-Passwortfeld zeigt die Eingabe jetzt im "
        "Klartext statt mit Sternchen an, damit Tippfehler sofort "
        "auffallen";

    const char* const CHANGELOG_FR =
        "- Correction : le nom d'utilisateur et le mot de passe "
        "MQTT étaient toujours enregistrés en majuscules, quelle "
        "que soit la saisie réelle. La touche Maj du clavier à "
        "l'écran se comporte désormais comme sur un smartphone "
        "(active uniquement pour la lettre suivante), et la casse "
        "exacte saisie est conservée\n"
        "- Correction : le champ du mot de passe MQTT affiche "
        "désormais le texte en clair au lieu d'astérisques, pour "
        "repérer immédiatement les fautes de frappe";

    const char* const CHANGELOG_TR =
        "- Düzeltme: MQTT kullanıcı adı ve şifresi, gerçek girişten "
        "bağımsız olarak her zaman büyük harfle kaydediliyordu. "
        "Ekran klavyesinin shift tuşu artık bir akıllı telefon gibi "
        "davranıyor (sadece bir sonraki harf için etkin) ve yazılan "
        "tam büyük/küçük harf korunuyor\n"
        "- Düzeltme: MQTT şifre alanı artık yıldız yerine düz metin "
        "gösteriyor, böylece yazım hataları hemen fark ediliyor";

    const char* const CHANGELOG_ES =
        "- Corrección: el nombre de usuario y la contraseña MQTT "
        "siempre se guardaban en mayúsculas, sin importar lo que se "
        "escribiera. La tecla de mayúsculas del teclado en pantalla "
        "ahora se comporta como en un smartphone (activa solo para "
        "la siguiente letra), y se conserva exactamente la "
        "escritura introducida\n"
        "- Corrección: el campo de contraseña MQTT ahora muestra el "
        "texto en claro en lugar de asteriscos, para detectar "
        "errores de escritura de inmediato";

    const char* const CHANGELOG_IT =
        "- Correzione: il nome utente e la password MQTT venivano "
        "sempre salvati in maiuscolo, indipendentemente da quanto "
        "digitato. Il tasto maiuscole della tastiera a schermo ora "
        "si comporta come su uno smartphone (attivo solo per la "
        "lettera successiva), e viene mantenuta esattamente la "
        "scrittura digitata\n"
        "- Correzione: il campo password MQTT ora mostra il testo "
        "in chiaro invece degli asterischi, così gli errori di "
        "digitazione sono immediatamente visibili";

    const char* const CHANGELOG_PT =
        "- Correção: o nome de usuário e a senha MQTT eram sempre "
        "salvos em maiúsculas, independentemente do que era "
        "digitado. A tecla shift do teclado na tela agora se "
        "comporta como em um smartphone (ativa apenas para a "
        "próxima letra), e a grafia exata digitada é preservada\n"
        "- Correção: o campo de senha MQTT agora mostra o texto em "
        "claro em vez de asteriscos, para que erros de digitação "
        "sejam identificados imediatamente";

    const char* const CHANGELOG_NL =
        "- Fix: MQTT-gebruikersnaam en -wachtwoord werden altijd in "
        "hoofdletters opgeslagen, ongeacht wat er werkelijk werd "
        "ingevoerd. De shift-toets van het schermtoetsenbord "
        "gedraagt zich nu als bij een smartphone (alleen actief "
        "voor de volgende letter), en de exacte schrijfwijze wordt "
        "behouden\n"
        "- Fix: het MQTT-wachtwoordveld toont nu platte tekst in "
        "plaats van sterretjes, zodat typfouten direct opvallen";

    const char* const TABLE[CHANGELOG_LANG_COUNT] = {
        CHANGELOG_EN, CHANGELOG_DE, CHANGELOG_FR, CHANGELOG_TR, CHANGELOG_ES, CHANGELOG_IT, CHANGELOG_PT, CHANGELOG_NL
    };
}

const char* changelogLatest() {
    uint8_t lang = SettingsStore::language();
    if (lang >= CHANGELOG_LANG_COUNT) lang = 0;
    return TABLE[lang];
}

}
