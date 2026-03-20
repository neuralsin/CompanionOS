/**
 * WiFi Diagnostic — NO TFT
 * =========================
 * Flash this ALONE to test if WiFi works without the TFT involved.
 * Open Serial Monitor at 115200 baud and watch the output.
 *
 * Board : NodeMCU 1.0 / Wemos D1 Mini
 * CPU   : 80 MHz is fine for this test
 */

#include <ESP8266WiFi.h>

const char* WIFI_SSID     = "YOUR_WIFI_SSID";      // ← change
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";  // ← change

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n\n========== WiFi Diagnostic ==========");

    // Print chip info — confirms serial is working
    Serial.printf("Chip ID     : %08X\n", ESP.getChipId());
    Serial.printf("Flash size  : %u KB\n", ESP.getFlashChipSize() / 1024);
    Serial.printf("Free heap   : %u bytes\n", ESP.getFreeHeap());
    Serial.println("=====================================\n");

    Serial.printf("Connecting to: \"%s\"\n", WIFI_SSID);

    WiFi.persistent(false);
    WiFi.disconnect(true);
    delay(200);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    uint32_t start = millis();
    int dots = 0;
    while (WiFi.status() != WL_CONNECTED) {
        yield();
        delay(300);
        Serial.print(".");
        dots++;

        // Every 10 dots, print the current status code so we know what's happening
        if (dots % 10 == 0) {
            Serial.printf("\n  Status code: %d  (3=connected, 6=wrong password, 1=idle)\n  ", WiFi.status());
        }

        if (millis() - start > 15000) {
            Serial.println("\n\n[RESULT] FAILED to connect after 15 seconds.");
            Serial.printf("[STATUS] Last WiFi status code: %d\n", WiFi.status());
            Serial.println("[STATUS] Codes: 0=idle, 1=no SSID, 2=scan done, 3=connected,");
            Serial.println("               4=connect failed, 5=lost, 6=wrong password");
            Serial.println("\nCheck: Is your router 2.4 GHz? ESP8266 cannot use 5 GHz.");
            Serial.println("Check: Any special characters in SSID or password?");
            Serial.println("Retrying in 5 seconds...");
            delay(5000);
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
            start = millis();
            dots  = 0;
        }
    }

    Serial.println("\n\n[RESULT] SUCCESS — WiFi connected!");
    Serial.printf("[INFO]   IP address : %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[INFO]   Gateway    : %s\n", WiFi.gatewayIP().toString().c_str());
    Serial.printf("[INFO]   Signal     : %d dBm\n", WiFi.RSSI());
    Serial.println("\nWiFi is working fine. The issue is in the main sketch (likely TFT pin conflict).");
}

void loop() {
    // Keep printing status every 2 seconds so you can see if it stays connected
    delay(2000);
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[OK] Still connected. IP: %s  Signal: %d dBm\n",
                      WiFi.localIP().toString().c_str(), WiFi.RSSI());
    } else {
        Serial.printf("[WARN] Disconnected! Status: %d\n", WiFi.status());
    }
}
