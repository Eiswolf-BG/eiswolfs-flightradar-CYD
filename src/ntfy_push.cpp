#include "ntfy_push.h"
#include "settings_store.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <cstring>

namespace NtfyPush {

namespace {
    SemaphoreHandle_t mutex = nullptr;
    char pendingMessage[160] = {0};
    bool hasPending = false;

    void ensureMutex() {
        if (mutex == nullptr) mutex = xSemaphoreCreateMutex();
    }
}

void request(const char* message) {
    if (!message || !message[0]) return;
    ensureMutex();
    xSemaphoreTake(mutex, portMAX_DELAY);
    strncpy(pendingMessage, message, sizeof(pendingMessage) - 1);
    pendingMessage[sizeof(pendingMessage) - 1] = 0;
    hasPending = true;
    xSemaphoreGive(mutex);
}

void update() {
    ensureMutex();

    char message[160];
    xSemaphoreTake(mutex, portMAX_DELAY);
    bool doSend = hasPending;
    if (doSend) {
        strncpy(message, pendingMessage, sizeof(message));
        hasPending = false;
    }
    xSemaphoreGive(mutex);
    if (!doSend) return;

    // Stumm abbrechen statt Fehler zu melden (Alex' Wunsch: kein WLAN oder
    // kein konfiguriertes Topic sind hier erwartbare, harmlose Zustaende,
    // keine echten Fehler).
    if (WiFi.status() != WL_CONNECTED) return;
    String topic = SettingsStore::ntfyPushTopic();
    if (topic.length() == 0) return;

    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(8000);

    HTTPClient http;
    http.setTimeout(8000);
    String url = "https://ntfy.sh/" + topic;
    if (!http.begin(client, url)) {
        Serial.println("[ntfy] Push fehlgeschlagen: http.begin() lieferte false.");
        return;
    }
    http.addHeader("Content-Type", "text/plain; charset=utf-8");

    int code = http.POST((uint8_t*)message, strlen(message));
    if (code != 200) {
        Serial.printf("[ntfy] Push fehlgeschlagen: HTTP %d\n", code);
    }
    http.end();
}

}
