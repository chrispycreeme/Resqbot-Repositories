#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Preferences.h>

Preferences preferences;
String savedSsid = "bawal kumonek";
String savedPass = "cdbanluta24";
String savedServerHost = "192.168.18.155";
bool operationsStarted = false;

const char* AP_SSID = "ResQBot-CAM-AP";

const int   SERVER_PORT = 5000;

WebServer server(80);

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#define LED_FLASH_PIN      4

unsigned long lastFrameTime = 0;
int frameCounter = 0;

void loadSavedCredentials() {
    preferences.begin("wifi-config", true);
    savedSsid = preferences.getString("ssid", "bawal kumonek");
    savedPass = preferences.getString("pass", "cdbanluta24");
    savedServerHost = preferences.getString("server_host", "192.168.18.155");
    preferences.end();
    Serial.println("[NVS] Loaded WiFi SSID: " + savedSsid);
    Serial.println("[NVS] Loaded Server Host: " + savedServerHost);
}

void saveCredentials(const String& ssid, const String& pass, const String& serverHost) {
    preferences.begin("wifi-config", false);
    preferences.putString("ssid", ssid);
    preferences.putString("pass", pass);
    preferences.putString("server_host", serverHost);
    preferences.end();
    savedSsid = ssid;
    savedPass = pass;
    savedServerHost = serverHost;
    Serial.println("[NVS] Saved new WiFi SSID: " + savedSsid);
    Serial.println("[NVS] Saved new Server Host: " + savedServerHost);
}

void setupCamera() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer   = LEDC_TIMER_0;
    config.pin_d0       = Y2_GPIO_NUM;
    config.pin_d1       = Y3_GPIO_NUM;
    config.pin_d2       = Y4_GPIO_NUM;
    config.pin_d3       = Y5_GPIO_NUM;
    config.pin_d4       = Y6_GPIO_NUM;
    config.pin_d5       = Y7_GPIO_NUM;
    config.pin_d6       = Y8_GPIO_NUM;
    config.pin_d7       = Y9_GPIO_NUM;
    config.pin_xclk     = XCLK_GPIO_NUM;
    config.pin_pclk     = PCLK_GPIO_NUM;
    config.pin_vsync    = VSYNC_GPIO_NUM;
    config.pin_href     = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn     = PWDN_GPIO_NUM;
    config.pin_reset    = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;

    if (psramFound()) {
        config.frame_size = FRAMESIZE_VGA;
        config.jpeg_quality = 10;
        config.fb_count = 2;
        config.xclk_freq_hz = 20000000;
    } else {
        config.frame_size = FRAMESIZE_QVGA;
        config.jpeg_quality = 12;
        config.fb_count = 1;
        config.xclk_freq_hz = 16000000;
    }

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x\n", err);
        return;
    }

    sensor_t *s = esp_camera_sensor_get();
    if (s != NULL) {
        s->set_brightness(s, 2);
        s->set_contrast(s, 2);
        s->set_saturation(s, 2);
        s->set_whitebal(s, 1);
        s->set_awb_gain(s, 1);
        s->set_wb_mode(s, 0);
        s->set_exposure_ctrl(s, 1);
        s->set_aec2(s, 1);
        s->set_ae_level(s, 0);
        s->set_gain_ctrl(s, 1);
        s->set_gainceiling(s, (gainceiling_t)GAINCEILING_4X);
        s->set_bpc(s, 1);
        s->set_wpc(s, 1);
        s->set_raw_gma(s, 1);
        s->set_lenc(s, 1);
        s->set_dcw(s, 1);
    }

    Serial.println("Camera initialized successfully.");

    delay(100);
    camera_fb_t* fb = esp_camera_fb_get();
    if (fb) {
        esp_camera_fb_return(fb);
    }
}

void connectWiFi() {
    if (WiFi.status() == WL_CONNECTED) return;

    Serial.print("Connecting to WiFi: ");
    Serial.println(savedSsid);
    WiFi.begin(savedSsid.c_str(), savedPass.c_str());

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi connected.");
        Serial.print("ESP32-CAM Local IP: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\nWiFi connection failed!");
    }
}

void handleRoot() {
    String html = "<!DOCTYPE html><html><head>"
                  "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                  "<title>ResQBot CAM Setup</title>"
                  "<style>"
                  "body{background:#1e1e1e;color:#eee;font-family:sans-serif;text-align:center;padding:20px;margin:0;}"
                  ".card{background:#2d2d2d;max-width:400px;margin:20px auto;padding:25px;border-radius:12px;box-shadow:0 4px 12px rgba(0,0,0,0.5);}"
                  "h2{color:#64B5F6;margin-top:0;}"
                  "label{display:block;text-align:left;margin-top:12px;font-weight:bold;color:#ccc;}"
                  "input[type=text],input[type=password]{width:100%;box-sizing:border-box;padding:12px;margin-top:6px;border:1px solid #444;border-radius:6px;background:#111;color:#fff;font-size:16px;}"
                  "input[type=submit]{background:#4CAF50;color:#fff;border:none;padding:14px;font-size:16px;font-weight:bold;border-radius:6px;cursor:pointer;width:100%;margin-top:20px;}"
                  "input[type=submit]:hover{background:#45a049;}"
                  ".status{margin-top:15px;font-size:14px;color:#888;}"
                  "</style></head><body>"
                  "<div class='card'>"
                  "<h2>ResQBot Camera Module</h2>"
                  "<p style='color:#aaa;'>WiFi & Host Server Setup</p>"
                  "<form action='/save' method='POST'>"
                  "<label>WiFi Network (SSID):</label>"
                  "<input type='text' name='ssid' value='" + savedSsid + "' required>"
                  "<label>WiFi Password:</label>"
                  "<input type='password' name='pass' value='" + savedPass + "'>"
                  "<label>Host PC / Server IP Address:</label>"
                  "<input type='text' name='server_host' value='" + savedServerHost + "' required>"
                  "<input type='submit' value='Connect & Continue Operations'>"
                  "</form>"
                  "<div class='status'>AP IP: " + WiFi.softAPIP().toString() + "</div>"
                  "</div></body></html>";
    server.send(200, "text/html", html);
}

void handleSave() {
    if (server.hasArg("ssid")) {
        String ssid = server.arg("ssid");
        String pass = server.hasArg("pass") ? server.arg("pass") : "";
        String serverHost = server.hasArg("server_host") ? server.arg("server_host") : savedServerHost;
        saveCredentials(ssid, pass, serverHost);
    }

    operationsStarted = true;

    WiFi.begin(savedSsid.c_str(), savedPass.c_str());
    Serial.println("[WiFi] Connecting to " + savedSsid);

    String html = "<!DOCTYPE html><html><head>"
                  "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                  "<title>ResQBot CAM - Connecting</title>"
                  "<style>"
                  "body{background:#1e1e1e;color:#eee;font-family:sans-serif;text-align:center;padding:30px;margin:0;}"
                  ".card{background:#2d2d2d;max-width:400px;margin:20px auto;padding:25px;border-radius:12px;}"
                  "h2{color:#4CAF50;}"
                  "</style></head><body>"
                  "<div class='card'>"
                  "<h2>Operations Started!</h2>"
                  "<p>Connecting to <b>" + savedSsid + "</b>...</p>"
                  "<p>Server Host: <b>" + savedServerHost + ":5000</b></p>"
                  "<p style='color:#aaa;'>You may close this tab now. The camera module is now streaming.</p>"
                  "</div></body></html>";
    server.send(200, "text/html", html);
}

void sendFrame() {
    if (WiFi.status() != WL_CONNECTED) {
        connectWiFi();
        if (WiFi.status() != WL_CONNECTED) return;
    }

    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        static unsigned long lastFailLog = 0;
        if (millis() - lastFailLog > 3000) {
            lastFailLog = millis();
            Serial.println("[ESP32-CAM] Frame capture failed! Retrying...");
        }
        delay(50);
        return;
    }

    HTTPClient http;
    String url = "http://" + savedServerHost + ":" + String(SERVER_PORT) + "/upload_cam";

    http.begin(url);
    http.addHeader("Content-Type", "image/jpeg");
    http.setTimeout(1000);

    int httpCode = http.POST(fb->buf, fb->len);

    if (httpCode <= 0) {
        Serial.printf("[ESP32-CAM] HTTP POST error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
    esp_camera_fb_return(fb);

    frameCounter++;
    if (millis() - lastFrameTime >= 3000) {
        float fps = (float)frameCounter / ((millis() - lastFrameTime) / 1000.0f);
        Serial.printf("[ESP32-CAM] Streaming active @ %.1f FPS (HTTP status: %d)\n", fps, httpCode);
        frameCounter = 0;
        lastFrameTime = millis();
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("=== ESP32-CAM Starting ===");

    pinMode(LED_FLASH_PIN, OUTPUT);
    digitalWrite(LED_FLASH_PIN, HIGH);

    loadSavedCredentials();
    setupCamera();

    WiFi.setSleep(false);
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID);
    Serial.println("[AP] SoftAP Started: " + String(AP_SSID));
    Serial.println("[AP] Visit http://" + WiFi.softAPIP().toString() + " to configure and start operations");

    server.on("/", handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.begin();
}

void loop() {
    server.handleClient();

    if (!operationsStarted) {
        delay(10);
        return;
    }

    sendFrame();
    delay(10);
}
