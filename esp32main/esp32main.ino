#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>

Preferences preferences;
String savedSsid = "bawal kumonek";
String savedPass = "cdbanluta24";
String savedServerHost = "192.168.18.155";
bool operationsStarted = false;

const char* AP_SSID = "ResQBot-Main-AP";

const int   SERVER_PORT = 5000;

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define OLED_SDA_PIN  21
#define OLED_SCL_PIN  22

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool oledFound = false;
unsigned long lastOledUpdate = 0;

bool pythonConnected = false;
unsigned long lastPythonSuccessTime = 0;
const unsigned long PYTHON_TIMEOUT_MS = 4000;

#define BUZZER_PIN 27

#define RXD2 16
#define TXD2 17
#define FRAME_BYTES 1544
#define MLX_BAUD 115200

uint8_t buffer[FRAME_BYTES];
float frame[768];
bool newFrameAvailable = false;

bool mlxFound = false;
unsigned long lastMlxByteTime = 0;
unsigned long lastMlxInitTime = 0;
unsigned long lastMlxLogTime = 0;
unsigned long totalFramesReceived = 0;
unsigned long totalFramesSent = 0;

#define MOTOR_A_IA 32
#define MOTOR_A_IB 33

#define MOTOR_B_IA 25
#define MOTOR_B_IB 26

#define PWM_FREQ 5000
#define PWM_RES  8

#define CHAN_A_IA 0
#define CHAN_A_IB 1
#define CHAN_B_IA 2
#define CHAN_B_IB 3

int targetMotorA = 0;
int targetMotorB = 0;
float currentMotorA = 0.0;
float currentMotorB = 0.0;
float rampStep = 10.0;
unsigned long lastRampTime = 0;

unsigned long buzzerStopTime = 0;
bool buzzerActive = false;

WebServer server(80);
char jsonBuffer[8192];
unsigned long lastMotorPoll = 0;

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

void initOledDisplay() {
    Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
    if (display.begin(SSD1306_SWITCHCAPVCC, 0x3C) || display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
        oledFound = true;
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.println("=== ResQBot Main ===");
        display.println("OLED Display OK");
        display.println("Connecting WiFi...");
        display.display();
        Serial.println("[OLED] SSD1306 initialized successfully!");
    } else {
        Serial.println("[OLED] Warning: SSD1306 display not found at 0x3C or 0x3D");
    }
}

void updateOledDisplay() {
    if (!oledFound) return;
    if (millis() - lastOledUpdate < 250) return;
    lastOledUpdate = millis();

    if (millis() - lastPythonSuccessTime > PYTHON_TIMEOUT_MS) {
        pythonConnected = false;
    }

    display.clearDisplay();

    display.setTextSize(1);
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print("   ResQBot Station  ");

    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 14);
    if (WiFi.status() == WL_CONNECTED) {
        display.print("WiFi: Connected");
    } else {
        display.print("WiFi: Disconnected");
    }

    display.setCursor(0, 26);
    if (WiFi.status() == WL_CONNECTED) {
        display.print("IP: ");
        display.print(WiFi.localIP().toString());
    } else {
        display.print("IP: Searching...");
    }

    display.setCursor(0, 38);
    if (pythonConnected) {
        display.print("Python App: Connected");
    } else {
        display.print("Python App: Offline");
    }

    display.setCursor(0, 50);
    display.printf("MLX:%s | MA:%d MB:%d", mlxFound ? "OK" : "NO", targetMotorA, targetMotorB);

    display.display();
}

void triggerBuzzer(int durationMs = 150) {
    digitalWrite(BUZZER_PIN, HIGH);
    buzzerStopTime = millis() + durationMs;
    buzzerActive = true;
}

void updateBuzzer() {
    if (buzzerActive && millis() >= buzzerStopTime) {
        digitalWrite(BUZZER_PIN, LOW);
        buzzerActive = false;
    }
}

void writePwm(int pin, int chan, int duty) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    ledcWrite(pin, duty);
#else
    ledcWrite(chan, duty);
#endif
}

void motorWrite(int pinFwd, int chanFwd, int pinRev, int chanRev, int speed) {
    speed = constrain(speed, -255, 255);
    if (speed > 0) {
        writePwm(pinFwd, chanFwd, speed);
        writePwm(pinRev, chanRev, 0);
    } else if (speed < 0) {
        writePwm(pinFwd, chanFwd, 0);
        writePwm(pinRev, chanRev, -speed);
    } else {
        writePwm(pinFwd, chanFwd, 0);
        writePwm(pinRev, chanRev, 0);
    }
}

void setMotorA(int speed) {
    targetMotorA = constrain(speed, -255, 255);
}

void setMotorB(int speed) {
    targetMotorB = constrain(speed, -255, 255);
}

void updateMotorRamping() {
    if (millis() - lastRampTime >= 15) {
        lastRampTime = millis();
        bool changed = false;

        if (currentMotorA < targetMotorA) {
            currentMotorA = min((float)targetMotorA, currentMotorA + rampStep);
            changed = true;
        } else if (currentMotorA > targetMotorA) {
            currentMotorA = max((float)targetMotorA, currentMotorA - rampStep);
            changed = true;
        }

        if (currentMotorB < targetMotorB) {
            currentMotorB = min((float)targetMotorB, currentMotorB + rampStep);
            changed = true;
        } else if (currentMotorB > targetMotorB) {
            currentMotorB = max((float)targetMotorB, currentMotorB - rampStep);
            changed = true;
        }

        if (changed) {
            int speedA = (int)round(currentMotorA);
            int speedB = (int)round(currentMotorB);
            motorWrite(MOTOR_A_IA, CHAN_A_IA, MOTOR_A_IB, CHAN_A_IB, speedA);
            motorWrite(MOTOR_B_IA, CHAN_B_IA, MOTOR_B_IB, CHAN_B_IB, speedB);
        }
    }
}

void setupMotors() {
    pinMode(MOTOR_A_IA, OUTPUT);
    digitalWrite(MOTOR_A_IA, LOW);
    pinMode(MOTOR_A_IB, OUTPUT);
    digitalWrite(MOTOR_A_IB, LOW);

    pinMode(MOTOR_B_IA, OUTPUT);
    digitalWrite(MOTOR_B_IA, LOW);
    pinMode(MOTOR_B_IB, OUTPUT);
    digitalWrite(MOTOR_B_IB, LOW);

#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    ledcAttach(MOTOR_A_IA, PWM_FREQ, PWM_RES);
    ledcAttach(MOTOR_A_IB, PWM_FREQ, PWM_RES);
    ledcAttach(MOTOR_B_IA, PWM_FREQ, PWM_RES);
    ledcAttach(MOTOR_B_IB, PWM_FREQ, PWM_RES);
#else
    ledcSetup(CHAN_A_IA, PWM_FREQ, PWM_RES);
    ledcSetup(CHAN_A_IB, PWM_FREQ, PWM_RES);
    ledcSetup(CHAN_B_IA, PWM_FREQ, PWM_RES);
    ledcSetup(CHAN_B_IB, PWM_FREQ, PWM_RES);

    ledcAttachPin(MOTOR_A_IA, CHAN_A_IA);
    ledcAttachPin(MOTOR_A_IB, CHAN_A_IB);
    ledcAttachPin(MOTOR_B_IA, CHAN_B_IA);
    ledcAttachPin(MOTOR_B_IB, CHAN_B_IB);
#endif

    writePwm(MOTOR_A_IA, CHAN_A_IA, 0);
    writePwm(MOTOR_A_IB, CHAN_A_IB, 0);
    writePwm(MOTOR_B_IA, CHAN_B_IA, 0);
    writePwm(MOTOR_B_IB, CHAN_B_IB, 0);

    targetMotorA = 0;
    targetMotorB = 0;
    currentMotorA = 0.0;
    currentMotorB = 0.0;
}

void initMlxSensor() {
    Serial.printf("[MLX] Initializing GPIO %d (RX) / GPIO %d (TX) @ %d baud...\n", RXD2, TXD2, MLX_BAUD);

    Serial2.end();
    delay(100);

    pinMode(RXD2, INPUT_PULLUP);
    pinMode(TXD2, OUTPUT);

    Serial2.setRxBufferSize(2048);
    Serial2.begin(MLX_BAUD, SERIAL_8N1, RXD2, TXD2);
    delay(200);

    uint8_t cmd1[] = {0xA5, 0x15, 0x01, 0xBB};
    Serial2.write(cmd1, sizeof(cmd1));
    Serial2.flush();
    delay(100);

    uint8_t cmd2[] = {0xA5, 0x35, 0x02, 0xDC};
    Serial2.write(cmd2, sizeof(cmd2));
    Serial2.flush();

    lastMlxInitTime = millis();
}

void parseFrame() {
    float minT = 999.0f, maxT = -999.0f, sumT = 0.0f;
    for (int i = 0; i < 768; i++) {
        uint8_t hi = buffer[4 + i * 2];
        uint8_t lo = buffer[5 + i * 2];
        int16_t rawTemp = (int16_t)((hi << 8) | lo);
        float t = rawTemp / 100.0f;
        frame[i] = t;
        if (t < minT) minT = t;
        if (t > maxT) maxT = t;
        sumT += t;
    }
    newFrameAvailable = true;
    totalFramesReceived++;

    if (totalFramesReceived == 1 || millis() - lastMlxLogTime > 3000) {
        lastMlxLogTime = millis();
        Serial.printf("[MLX] Frame #%lu parsed (768 pixels | Min: %.1f C, Max: %.1f C, Avg: %.1f C)\n",
                      totalFramesReceived, minT, maxT, sumT / 768.0f);
    }
}

enum FrameState { SEARCH_5A_1, SEARCH_5A_2, COLLECT };
FrameState state = SEARCH_5A_1;
int idx = 0;

void pollSensor() {
    while (Serial2.available()) {
        uint8_t b = Serial2.read();
        lastMlxByteTime = millis();

        switch (state) {
            case SEARCH_5A_1:
                if (b == 0x5A) {
                    buffer[0] = b;
                    state = SEARCH_5A_2;
                }
                break;

            case SEARCH_5A_2:
                if (b == 0x5A) {
                    buffer[1] = b;
                    idx = 2;
                    state = COLLECT;
                    if (!mlxFound) {
                        mlxFound = true;
                        Serial.printf("\n[MLX] *** MLX90640 SENSOR FOUND & LOCKED! ***\n");
                        Serial.printf("[MLX] Detected on GPIO %d (RX) / GPIO %d (TX) @ %d baud\n\n",
                                      RXD2, TXD2, MLX_BAUD);
                    }
                } else {
                    state = SEARCH_5A_1;
                }
                break;

            case COLLECT:
                buffer[idx++] = b;
                if (idx >= FRAME_BYTES) {
                    parseFrame();
                    state = SEARCH_5A_1;
                    idx = 0;
                }
                break;
        }
    }
}

void checkMlxHealth() {
    unsigned long now = millis();
    if (!mlxFound && (now - lastMlxInitTime > 3000) && (now - lastMlxByteTime > 3000)) {
        Serial.printf("\n[MLX] No response. Re-initializing GPIO %d / %d @ %d baud...\n", RXD2, TXD2, MLX_BAUD);
        initMlxSensor();
    } else if (mlxFound && (now - lastMlxByteTime > 4000)) {
        Serial.println("[MLX] WARNING: MLX90640 stream interrupted (>4s no data). Re-initializing...");
        mlxFound = false;
        initMlxSensor();
    }
}

void buildJson() {
    int pos = 0;
    jsonBuffer[pos++] = '[';
    for (int i = 0; i < 768; i++) {
        pos += snprintf(jsonBuffer + pos, sizeof(jsonBuffer) - pos,
                         "%.1f%s", frame[i], (i < 767) ? "," : "");
    }
    jsonBuffer[pos++] = ']';
    jsonBuffer[pos] = '\0';
}

void parseServerResponse(String payload) {
    int idxA = payload.indexOf("motorA");
    if (idxA != -1) {
        int colonA = payload.indexOf(':', idxA);
        if (colonA != -1) {
            int valA = payload.substring(colonA + 1).toInt();
            setMotorA(valA);
        }
    }
    int idxB = payload.indexOf("motorB");
    if (idxB != -1) {
        int colonB = payload.indexOf(':', idxB);
        if (colonB != -1) {
            int valB = payload.substring(colonB + 1).toInt();
            setMotorB(valB);
        }
    }
    int idxP = payload.indexOf("persons");
    if (idxP != -1) {
        int colonP = payload.indexOf(':', idxP);
        if (colonP != -1) {
            int numPersons = payload.substring(colonP + 1).toInt();
            if (numPersons > 0) {
                triggerBuzzer(150);
            }
        }
    }
}

void sendThermalData() {
    if (!newFrameAvailable) return;
    if (WiFi.status() != WL_CONNECTED) {
        static unsigned long lastWifiWarn = 0;
        if (millis() - lastWifiWarn > 5000) {
            lastWifiWarn = millis();
            Serial.println("[MLX] Cannot send thermal data: WiFi NOT connected!");
        }
        return;
    }

    buildJson();
    newFrameAvailable = false;

    HTTPClient http;
    String url = "http://" + savedServerHost + ":" + String(SERVER_PORT) + "/upload_mlx";

    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(1500);

    int httpResponseCode = http.POST((uint8_t*)jsonBuffer, strlen(jsonBuffer));

    if (httpResponseCode == 200) {
        lastPythonSuccessTime = millis();
        pythonConnected = true;
        totalFramesSent++;
        if (totalFramesSent == 1 || totalFramesSent % 20 == 0) {
            Serial.printf("[MLX] Posted frame #%lu to %s -> HTTP 200 OK\n", totalFramesSent, url.c_str());
        }
        String response = http.getString();
        parseServerResponse(response);
    } else {
        Serial.printf("[MLX] Error posting MLX data to %s: HTTP code %d\n", url.c_str(), httpResponseCode);
    }

    http.end();
}

void pollServerForMotors() {
    if (millis() - lastMotorPoll > 200) {
        lastMotorPoll = millis();
        if (WiFi.status() == WL_CONNECTED) {
            HTTPClient http;
            String url = "http://" + savedServerHost + ":" + String(SERVER_PORT) + "/motor";
            http.begin(url);
            http.setTimeout(500);
            int code = http.GET();
            if (code == 200) {
                lastPythonSuccessTime = millis();
                pythonConnected = true;
                parseServerResponse(http.getString());
            }
            http.end();
        }
    }
}

void handleRoot() {
    String html = "<!DOCTYPE html><html><head>"
                  "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                  "<title>ResQBot Main Setup</title>"
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
                  "<h2>ResQBot Main Station</h2>"
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
                  "<title>ResQBot Main - Connecting</title>"
                  "<style>"
                  "body{background:#1e1e1e;color:#eee;font-family:sans-serif;text-align:center;padding:30px;margin:0;}"
                  ".card{background:#2d2d2d;max-width:400px;margin:20px auto;padding:25px;border-radius:12px;}"
                  "h2{color:#4CAF50;}"
                  "</style></head><body>"
                  "<div class='card'>"
                  "<h2>Operations Started!</h2>"
                  "<p>Connecting to <b>" + savedSsid + "</b>...</p>"
                  "<p>Server Host: <b>" + savedServerHost + ":5000</b></p>"
                  "<p style='color:#aaa;'>You may close this tab now. The station is now running operations.</p>"
                  "</div></body></html>";
    server.send(200, "text/html", html);
}

void handleData() {
    buildJson();
    server.send(200, "application/json", jsonBuffer);
}

void handleMotor() {
    if (!server.hasArg("ch") || !server.hasArg("speed")) {
        server.send(400, "text/plain", "missing ch or speed");
        return;
    }
    String ch = server.arg("ch");
    int speed = server.arg("speed").toInt();

    if (ch == "A") setMotorA(speed);
    else if (ch == "B") setMotorB(speed);
    else {
        server.send(400, "text/plain", "ch must be A or B");
        return;
    }

    char resp[64];
    snprintf(resp, sizeof(resp), "{\"ch\":\"%s\",\"speed\":%d}", ch.c_str(), speed);
    server.send(200, "application/json", resp);
}

void handleMotorStopAll() {
    setMotorA(0);
    setMotorB(0);
    currentMotorA = 0.0;
    currentMotorB = 0.0;
    server.send(200, "application/json", "{\"status\":\"stopped\"}");
}

void handleBeep() {
    int ms = 150;
    if (server.hasArg("ms")) {
        ms = server.arg("ms").toInt();
    }
    triggerBuzzer(ms);
    server.send(200, "application/json", "{\"status\":\"beeped\"}");
}

void updateOledApMode() {
    if (!oledFound) return;
    if (millis() - lastOledUpdate < 250) return;
    lastOledUpdate = millis();

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
    display.setCursor(0, 0);
    display.print("   ResQBot AP Mode  ");

    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 14);
    display.print("AP: ");
    display.print(AP_SSID);

    display.setCursor(0, 26);
    display.print("IP: ");
    display.print(WiFi.softAPIP().toString());

    display.setCursor(0, 38);
    display.print("Connect to AP and");

    display.setCursor(0, 50);
    display.print("click Continue!");

    display.display();
}

void setup() {
    digitalWrite(BUZZER_PIN, LOW);
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    setupMotors();

    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== ESP32 Main (MLX90640 + L9110 + Active Buzzer) Starting ===");

    loadSavedCredentials();

    initOledDisplay();

    Serial.println("[MLX] Waiting 1.5s for GY-MCU90640 Boot...");
    delay(1500);

    initMlxSensor();

    WiFi.setSleep(false);
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID);
    Serial.println("[AP] SoftAP Started: " + String(AP_SSID));
    Serial.println("[AP] Visit http://" + WiFi.softAPIP().toString() + " to configure and start operations");

    server.on("/", handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/data", handleData);
    server.on("/motor", handleMotor);
    server.on("/motorStop", handleMotorStopAll);
    server.on("/beep", handleBeep);
    server.begin();

    updateOledApMode();
}

void loop() {
    server.handleClient();
    updateMotorRamping();
    updateBuzzer();

    if (!operationsStarted) {
        updateOledApMode();
        delay(10);
        return;
    }

    pollSensor();
    checkMlxHealth();
    sendThermalData();
    if (!newFrameAvailable) {
        pollServerForMotors();
    }
    updateOledDisplay();
}
