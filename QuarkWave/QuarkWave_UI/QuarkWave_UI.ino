/*
  QuarkWave Web Interface - Raspberry Pi Pico 2W
  
  Handles:
  - Web server with synth interface
  - WebSocket communication
  - Patch management (save/load via LittleFS)
  - Configuration persistence
  - UART communication with Uno R4 synth
  
  Hardware connections:
  - GPIO 0 (TX) -> Uno R4 RX
  - GPIO 1 (RX) -> Uno R4 TX
  - Common ground
*/

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <WebSocketsServer.h>
#include <MDNS_Generic.h>
#include <WiFiUdp.h>  
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "QuarkWave_UI.h"
#include "secrets.h"

// Global declarations - put these at the top level
WiFiUDP udp;
MDNS mdns(udp);


// Server instances
AsyncWebServer server(80);
WebSocketsServer websocket(8081);

// UART to Uno R4 synth
#define synthSerial Serial1
#define LED_PIN LED_BUILTIN 
#define DEBUG_LOG 0
#define DEBUG_FS 0
#define WIFI_CONNECT_TIMEOUT_MS 20000UL
#define PATCH_CC_BATCH_MAX 16

#if DEBUG_LOG
#define LOG_PRINT(x) Serial.print(x)
#define LOG_PRINTLN(x) Serial.println(x)
#define LOG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
#define LOG_PRINT(x) do {} while (0)
#define LOG_PRINTLN(x) do {} while (0)
#define LOG_PRINTF(...) do {} while (0)
#endif

// Configuration structure
struct Config {
  uint8_t currentPatch;
  uint32_t baudRate;
};

Config config = {
  .currentPatch = 0,
  .baudRate = 115200
};

// mDNS
bool mdnsActive = false;
unsigned long lastMdnsCheck = 0;
const unsigned long MDNS_CHECK_INTERVAL = 30000; // Check every 30 seconds


// Patch structure mirrored by the Uno R4 synth and the browser UI.
struct SynthPatch {
  // Envelope
  float a, d, s, r;
  
  // Oscillator
  float morph;
  
  // Filter
  float cutoff, resonance;
  bool filterOn;
  
  // Motion
  float glide, detune;
  uint8_t unison;
  
  // Tempo/Sync
  uint8_t tempoSrc;   // 0=Internal, 1=Clock
  bool lfoSync;
  bool dlySync;
  float bpmInt;
  
  // Mixer
  float masterGain;
  float noiseAmt;
  
  // Delay
  float dlyTime, dlyFb, dlyMix;

  // Character FX
  float crushAmt, tremRateHz, tremDepth, driveAmt, foldAmt;
  
  // LFO
  float lfoAmtHz, lfoRateHz;
  float lfoToMorph, lfoToAmp, lfoToDetune;  // Group LFO routing together
  
  // Modulation routing
  float velToCutoff, noiseToCutoff;
  
  // LFO2/Vibrato 
  float lfo2RateHz, lfo2AmtSemi;
  uint8_t lfo2Wave;
  
  // Chorus
  float chMix, chDepth;
    
  // Arp
  uint8_t arpMode;
  uint8_t arpDiv; 
  uint8_t arpGate;
  
  // Voice management
  uint8_t stealMode;
  uint8_t velCurve;
  
  // Metadata
  char name[17];  // 16 chars + null terminator
};

// Current patch in memory
SynthPatch currentPatch;

// Default Patches 
struct FactoryPreset {
  const char* name;
  SynthPatch patch;
};

enum ParamScale : uint8_t { PARAM_LINEAR, PARAM_BOOL, PARAM_UNISON };
struct ParamSpec {
  const char* name;
  uint8_t cc;
  float lo;
  float hi;
  ParamScale scale;
};

static const ParamSpec PARAMS[] = {
  {"attack", 73, 0.002f, 0.5f, PARAM_LINEAR},
  {"decay", 75, 0.01f, 1.0f, PARAM_LINEAR},
  {"sustain", 23, 0.10f, 0.85f, PARAM_LINEAR},
  {"release", 72, 0.02f, 1.5f, PARAM_LINEAR},
  {"cutoff", 74, 40.0f, 10000.0f, PARAM_LINEAR},
  {"resonance", 71, 0.5f, 3.0f, PARAM_LINEAR},
  {"morph", 76, 0.0f, 1.0f, PARAM_LINEAR},
  {"filterOn", 91, 0.0f, 1.0f, PARAM_BOOL},
  {"glide", 5, 0.0f, 0.3f, PARAM_LINEAR},
  {"detune", 94, 2.0f, 20.0f, PARAM_LINEAR},
  {"masterGain", 7, 0.2f, 1.0f, PARAM_LINEAR},
  {"noiseAmt", 93, 0.0f, 1.0f, PARAM_LINEAR},
  {"delayTime", 12, 0.02f, 0.166f, PARAM_LINEAR},
  {"delayFeedback", 13, 0.01f, 0.89f, PARAM_LINEAR},
  {"delayMix", 14, 0.0f, 1.0f, PARAM_LINEAR},
  {"crushAmt", 29, 0.0f, 1.0f, PARAM_LINEAR},
  {"tremRateHz", 30, 0.2f, 20.0f, PARAM_LINEAR},
  {"tremDepth", 31, 0.0f, 1.0f, PARAM_LINEAR},
  {"driveAmt", 33, 0.0f, 1.0f, PARAM_LINEAR},
  {"foldAmt", 34, 0.0f, 1.0f, PARAM_LINEAR},
  {"lfoAmtHz", 1, 0.0f, 3000.0f, PARAM_LINEAR},
  {"lfoRateHz", 2, 0.1f, 12.1f, PARAM_LINEAR},
  {"lfoSync", 3, 0.0f, 1.0f, PARAM_BOOL},
  {"velToCutoff", 24, 0.0f, 1.0f, PARAM_LINEAR},
  {"noiseToCutoff", 25, 0.0f, 1.0f, PARAM_LINEAR},
  {"lfoToDetune", 26, 0.0f, 1.0f, PARAM_LINEAR},
  {"unison", 95, 1.0f, 3.0f, PARAM_UNISON},
};

const FactoryPreset FACTORY_PRESETS[] = {
  {"Warm Pad", {
    .a=0.08f, .d=0.7f, .s=0.85f, .r=0.8f,
    .morph=0.35f, .cutoff=1400.f, .resonance=0.8f, .filterOn=true,
    .glide=0.06f, .detune=12.f, .unison=3,
    .tempoSrc=0, .lfoSync=false, .dlySync=true, .bpmInt=120.0f,
    .masterGain=0.7f, .noiseAmt=0.0f,
    .dlyTime=0.14f, .dlyFb=0.35f, .dlyMix=0.12f,
    .lfoAmtHz=350.f, .lfoRateHz=0.6f, 
    .lfoToMorph=0.25f, .lfoToAmp=0.10f, .lfoToDetune=0.0f,
    .velToCutoff=0.0f, .noiseToCutoff=0.0f,
    .lfo2RateHz=5.0f, .lfo2AmtSemi=0.2f, .lfo2Wave=0,
    .chMix=0.0f, .chDepth=5.0f,
    .arpMode=0, .arpDiv=2, .arpGate=60,
    .stealMode=0, .velCurve=0,
    .name="Warm Pad"
  }},
  
  {"Pluck", {
    .a=0.002f, .d=0.12f, .s=0.15f, .r=0.15f,
    .morph=0.65f, .cutoff=3800.f, .resonance=1.1f, .filterOn=true,
    .glide=0.0f, .detune=6.f, .unison=1,
    .tempoSrc=0, .lfoSync=false, .dlySync=true, .bpmInt=120.0f,
    .masterGain=0.7f, .noiseAmt=0.0f,
    .dlyTime=0.10f, .dlyFb=0.40f, .dlyMix=0.18f,
    .lfoAmtHz=0.f, .lfoRateHz=4.0f,
    .lfoToMorph=0.0f, .lfoToAmp=0.0f, .lfoToDetune=0.0f,
    .velToCutoff=0.3f, .noiseToCutoff=0.0f,
    .lfo2RateHz=5.0f, .lfo2AmtSemi=0.2f, .lfo2Wave=0,
    .chMix=0.0f, .chDepth=5.0f,
    .arpMode=0, .arpDiv=2, .arpGate=60,
    .stealMode=1, .velCurve=2,
    .name="Pluck"
  }},
  
  {"Solid Bass", {
    .a=0.004f, .d=0.08f, .s=0.6f, .r=0.12f,
    .morph=0.85f, .cutoff=900.f, .resonance=0.7f, .filterOn=true,
    .glide=0.03f, .detune=0.f, .unison=1,
    .tempoSrc=0, .lfoSync=false, .dlySync=false, .bpmInt=120.0f,
    .masterGain=0.7f, .noiseAmt=0.0f,
    .dlyTime=0.12f, .dlyFb=0.25f, .dlyMix=0.0f,
    .lfoAmtHz=0.f, .lfoRateHz=2.0f,
    .lfoToMorph=0.0f, .lfoToAmp=0.0f, .lfoToDetune=0.0f,
    .velToCutoff=0.4f, .noiseToCutoff=0.0f,
    .lfo2RateHz=5.0f, .lfo2AmtSemi=0.2f, .lfo2Wave=0,
    .chMix=0.0f, .chDepth=5.0f,
    .arpMode=0, .arpDiv=2, .arpGate=60,
    .stealMode=0, .velCurve=0,
    .name="Solid Bass"
  }},
  
  {"PWM Lead", {
    .a=0.003f, .d=0.18f, .s=0.55f, .r=0.25f,
    .morph=0.75f, .cutoff=2600.f, .resonance=1.0f, .filterOn=true,
    .glide=0.04f, .detune=10.f, .unison=2,
    .tempoSrc=0, .lfoSync=false, .dlySync=false, .bpmInt=120.0f,
    .masterGain=0.7f, .noiseAmt=0.0f,
    .dlyTime=0.09f, .dlyFb=0.30f, .dlyMix=0.10f,
    .lfoAmtHz=900.f, .lfoRateHz=5.0f,
    .lfoToMorph=0.35f, .lfoToAmp=0.0f, .lfoToDetune=0.1f,
    .velToCutoff=0.2f, .noiseToCutoff=0.0f,
    .lfo2RateHz=6.5f, .lfo2AmtSemi=0.3f, .lfo2Wave=0,    // Vibrato
    .chMix=0.0f, .chDepth=5.0f,
    .arpMode=0, .arpDiv=2, .arpGate=60,
    .stealMode=1, .velCurve=1,
    .name="PWM Lead"
  }},
  
  {"EP Keys", {
    .a=0.004f, .d=0.35f, .s=0.35f, .r=0.35f,
    .morph=0.25f, .cutoff=2200.f, .resonance=0.95f, .filterOn=true,
    .glide=0.0f, .detune=0.f, .unison=1,
    .tempoSrc=0, .lfoSync=false, .dlySync=true, .bpmInt=120.0f,
    .masterGain=0.7f, .noiseAmt=0.0f,
    .dlyTime=0.12f, .dlyFb=0.35f, .dlyMix=0.15f,
    .lfoAmtHz=250.f, .lfoRateHz=1.2f,
    .lfoToMorph=0.15f, .lfoToAmp=0.05f, .lfoToDetune=0.0f,
    .velToCutoff=0.6f, .noiseToCutoff=0.0f,
    .lfo2RateHz=5.0f, .lfo2AmtSemi=0.2f, .lfo2Wave=0,
    .chMix=0.15f, .chDepth=8.0f,                           // Subtle chorus for EP sound
    .arpMode=0, .arpDiv=2, .arpGate=60,
    .stealMode=0, .velCurve=1,
    .name="EP Keys"
  }},
  
  {"Sweep Pad", {
    .a=0.15f, .d=1.2f, .s=0.9f, .r=1.2f,
    .morph=0.40f, .cutoff=1100.f, .resonance=1.15f, .filterOn=true,
    .glide=0.08f, .detune=14.f, .unison=3,
    .tempoSrc=0, .lfoSync=false, .dlySync=true, .bpmInt=120.0f,
    .masterGain=0.7f, .noiseAmt=0.0f,
    .dlyTime=0.16f, .dlyFb=0.45f, .dlyMix=0.22f,
    .lfoAmtHz=1500.f, .lfoRateHz=0.3f,
    .lfoToMorph=0.35f, .lfoToAmp=0.12f, .lfoToDetune=0.05f,
    .velToCutoff=0.0f, .noiseToCutoff=0.0f,
    .lfo2RateHz=5.0f, .lfo2AmtSemi=0.2f, .lfo2Wave=0,
    .chMix=0.25f, .chDepth=12.0f,                           // Rich chorus for pad
    .arpMode=0, .arpDiv=2, .arpGate=60,
    .stealMode=0, .velCurve=0,
    .name="Sweep Pad"
  }},
  
  {"Noise Perc", {
    .a=0.001f, .d=0.08f, .s=0.0f, .r=0.05f,
    .morph=0.0f, .cutoff=3200.f, .resonance=1.0f, .filterOn=true,
    .glide=0.0f, .detune=0.f, .unison=1,
    .tempoSrc=0, .lfoSync=false, .dlySync=false, .bpmInt=120.0f,
    .masterGain=0.7f, .noiseAmt=0.5f,
    .dlyTime=0.06f, .dlyFb=0.25f, .dlyMix=0.05f,
    .lfoAmtHz=0.f, .lfoRateHz=8.0f,
    .lfoToMorph=0.0f, .lfoToAmp=0.0f, .lfoToDetune=0.0f,
    .velToCutoff=0.8f, .noiseToCutoff=0.0f,
    .lfo2RateHz=5.0f, .lfo2AmtSemi=0.2f, .lfo2Wave=0,
    .chMix=0.0f, .chDepth=5.0f,
    .arpMode=0, .arpDiv=2, .arpGate=60,
    .stealMode=1, .velCurve=3,
    .name="Noise Perc"
  }},
  
  {"Chrs Strngs", {
    .a=0.03f, .d=0.7f, .s=0.8f, .r=0.9f,
    .morph=0.45f, .cutoff=1500.f, .resonance=0.85f, .filterOn=true,
    .glide=0.06f, .detune=16.f, .unison=3,
    .tempoSrc=0, .lfoSync=false, .dlySync=true, .bpmInt=120.0f,
    .masterGain=0.7f, .noiseAmt=0.0f,
    .dlyTime=0.13f, .dlyFb=0.35f, .dlyMix=0.12f,
    .lfoAmtHz=450.f, .lfoRateHz=0.8f,
    .lfoToMorph=0.2f, .lfoToAmp=0.08f, .lfoToDetune=0.03f,
    .velToCutoff=0.3f, .noiseToCutoff=0.0f,
    .lfo2RateHz=5.0f, .lfo2AmtSemi=0.2f, .lfo2Wave=0,
    .chMix=0.35f, .chDepth=10.0f,                           // Rich chorus for strings
    .arpMode=0, .arpDiv=2, .arpGate=60,
    .stealMode=0, .velCurve=1,
    .name="Chrs Strngs"
  }}
};
const int NUM_FACTORY_PRESETS = sizeof(FACTORY_PRESETS) / sizeof(FACTORY_PRESETS[0]);

// Function prototypes
bool setupWiFi();
void setupWebServer();
void setupWebSocket();
void setupFileSystem();
void setupSynthSerial();
bool saveConfig();
bool loadConfig();
bool savePatchToFile(int slot, const SynthPatch& patch);
bool loadPatchFromFile(int slot, SynthPatch& patch);
void handleSynthMessage(const String& message);
void broadcastPatchList();
void initializeDefaultPatches();
void setupLED();
void ledBlink(int times, int delayMs);
void pollSynthSerial();
#if DEBUG_FS
void debugFileSystem();
#endif

void setupLED() {
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);  // Start with LED off
}

void ledBlink(int times, int delayMs = 200) {
    for (int i = 0; i < times; i++) {
        digitalWrite(LED_PIN, HIGH);
        delay(delayMs);
        digitalWrite(LED_PIN, LOW);
        delay(delayMs);
    }
}

bool setupMDNS() {
    LOG_PRINTLN("[setupMDNS] Initializing mDNS...");
    
    // Start mDNS with hostname
    if (!mdns.begin(WiFi.localIP(), "quarkwave")) {
        LOG_PRINTLN("[setupMDNS] ERROR: Failed to start mDNS responder");
        return false;
    }
    
    LOG_PRINTLN("[setupMDNS] mDNS responder started successfully");
    LOG_PRINTF("[setupMDNS] Hostname: quarkwave.local (%s)\n", WiFi.localIP().toString().c_str());
    
    // Add HTTP service using addServiceRecord
    if (mdns.addServiceRecord("quarkwave-http", 80, MDNSServiceTCP)) {
        LOG_PRINTLN("[setupMDNS] HTTP service registered on port 80");
    } else {
        LOG_PRINTLN("[setupMDNS] WARNING: Failed to register HTTP service");
    }
    
    // Add WebSocket service
    if (mdns.addServiceRecord("quarkwave-ws", 8081, MDNSServiceTCP)) {
        LOG_PRINTLN("[setupMDNS] WebSocket service registered on port 8081");
    } else {
        LOG_PRINTLN("[setupMDNS] WARNING: Failed to register WebSocket service");
    }
    
    // Add custom service for QuarkWave discovery
    if (mdns.addServiceRecord("quarkwave", 80, MDNSServiceTCP)) {
        LOG_PRINTLN("[setupMDNS] QuarkWave service registered");
    } else {
        LOG_PRINTLN("[setupMDNS] WARNING: Failed to register QuarkWave service");
    }
    
    mdnsActive = true;
    return true;
}

// Add mDNS health check function
void checkMDNS() {
    if (!mdnsActive) return;
    
    unsigned long now = millis();
    if (now - lastMdnsCheck < MDNS_CHECK_INTERVAL) return;
    
    lastMdnsCheck = now;
    
    // Simple health check - try to query our own hostname
    // Note: This is a basic check, more sophisticated monitoring could be added
    if (WiFi.status() != WL_CONNECTED) {
        LOG_PRINTLN("[checkMDNS] WiFi disconnected, mDNS may not be working");
        mdnsActive = false;
        return;
    }
    
    LOG_PRINTLN("[checkMDNS] mDNS health check passed");
}


void setup() {
    Serial.begin(115200);

    // Intro
    LOG_PRINT(F("QuarkWave\n"));
    LOG_PRINT(F("Web Synthesis Sound Design Studio\n"));
    LOG_PRINT(F("(C) 2025 Iain Bennett\n"));
    LOG_PRINT(F("---------------------------------\n"));
    
    setupLED();  // Initialize LED first
    ledBlink(2);  // 2 blinks = startup
    
    LOG_PRINTLN("[BOOT] QuarkWave Web Interface Starting...");
    
    setupFileSystem();
#if DEBUG_FS
    debugFileSystem();
#endif
    loadConfig();
    setupSynthSerial();
    
    LOG_PRINTLN("[BOOT] Attempting WiFi connection...");
    const bool wifiConnected = setupWiFi();
    
    if (wifiConnected) {
        digitalWrite(LED_PIN, HIGH);  // LED ON = WiFi connected
        LOG_PRINTLN("[BOOT] LED ON - WiFi Connected!");
        
        setupWebServer();
        setupWebSocket();
        
        // mDNS setup
        if (setupMDNS()) {
            ledBlink(3, 100);  // 3 quick blinks = mDNS ready
            LOG_PRINTLN("[BOOT] mDNS ready - LED blinked 3 times");
        } else {
            ledBlink(5, 200);  // 5 slow blinks = mDNS failed
            LOG_PRINTLN("[BOOT] mDNS failed - LED blinked 5 times");
        }
        
        digitalWrite(LED_PIN, HIGH);  // Back to solid on
    } else {
        LOG_PRINTLN("[BOOT] WiFi unavailable; web UI not started");
        for (int i = 0; i < 3; i++) {
            digitalWrite(LED_PIN, HIGH);
            delay(250);
            digitalWrite(LED_PIN, LOW);
            delay(250);
        }
    }
    
    initializeDefaultPatches();
    loadAndSendPatch(config.currentPatch);
    LOG_PRINTLN("[BOOT] System ready!");
}

void loop() {
    if (mdnsActive) {
        mdns.run();
        checkMDNS();  // Add health monitoring
    }

    if (WiFi.status() == WL_CONNECTED) websocket.loop(); 
    
    // Heartbeat blink every 30 seconds if connected
    static unsigned long lastHeartbeat = 0;
    if (WiFi.status() == WL_CONNECTED && millis() - lastHeartbeat > 30000) {
        lastHeartbeat = millis();
        digitalWrite(LED_PIN, LOW);
        delay(50);
        digitalWrite(LED_PIN, HIGH);  // Quick off/on heartbeat
    }
    
    pollSynthSerial();
    yield();
}



bool setupWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    LOG_PRINT("[setupWiFi] Connecting to WiFi");

    const uint32_t startedAt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startedAt < WIFI_CONNECT_TIMEOUT_MS) {
        delay(500);
        LOG_PRINT(".");
    }
    
    LOG_PRINTLN("");
    if (WiFi.status() != WL_CONNECTED) {
        LOG_PRINTLN("[setupWiFi] Timed out");
        return false;
    }

    LOG_PRINT("[setupWiFi] Connected! IP: ");
    LOG_PRINTLN(WiFi.localIP());
    return true;
}

void pollSynthSerial() {
    static char buf[512];
    static size_t pos = 0;

    while (synthSerial.available()) {
        const char c = (char)synthSerial.read();
        if (c == '\n' || c == '\r') {
            if (pos > 0) {
                buf[pos] = '\0';
                handleSynthMessage(String(buf));
                pos = 0;
            }
        } else if (pos < sizeof(buf) - 1) {
            buf[pos++] = c;
        } else {
            pos = 0;
            LOG_PRINTLN("[UART] RX buffer overflow, line discarded");
        }
    }
}

void setupFileSystem() {
    if (!LittleFS.begin()) {
        LOG_PRINTLN("LittleFS mount failed - formatting...");
        if (LittleFS.format()) {
            LOG_PRINTLN("LittleFS formatted successfully");
            LittleFS.begin();
        } else {
            LOG_PRINTLN("LittleFS format failed!");
            return;
        }
    }
    LOG_PRINTLN("[setupFileSystem] LittleFS ready");
}

void setupSynthSerial() {
    Serial1.setTX(0);
    Serial1.setRX(1);
    Serial1.begin(config.baudRate);
    LOG_PRINTLN("[setupSynthSerial] Synth UART initialized");
}

void setupWebServer() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html; charset=utf-8", INDEX_HTML);
    });

    server.begin();
    LOG_PRINTF("[setupWebServer] Web server started (%d bytes HTML)\n", INDEX_HTML_LEN);
}

bool loadFactoryPreset(int index, SynthPatch& patch) {
  if (index < 0 || index >= NUM_FACTORY_PRESETS) return false;
  patch = FACTORY_PRESETS[index].patch;
  return true;
}

void normalizePatchDefaults(SynthPatch& patch) {
    if (patch.tremRateHz <= 0.0f) patch.tremRateHz = 5.0f;
    patch.crushAmt = constrain(patch.crushAmt, 0.0f, 1.0f);
    patch.tremRateHz = constrain(patch.tremRateHz, 0.2f, 20.0f);
    patch.tremDepth = constrain(patch.tremDepth, 0.0f, 1.0f);
    patch.driveAmt = constrain(patch.driveAmt, 0.0f, 1.0f);
    patch.foldAmt = constrain(patch.foldAmt, 0.0f, 1.0f);
    patch.lfo2RateHz = constrain(patch.lfo2RateHz <= 0.0f ? 5.0f : patch.lfo2RateHz, 0.05f, 20.0f);
    patch.lfo2AmtSemi = constrain(patch.lfo2AmtSemi, 0.0f, 2.0f);
    patch.chMix = constrain(patch.chMix, 0.0f, 1.0f);
    patch.chDepth = constrain(patch.chDepth <= 0.0f ? 5.0f : patch.chDepth, 0.0f, 20.0f);
    patch.arpDiv = constrain((int)patch.arpDiv, 0, 7);
    patch.arpGate = constrain((int)patch.arpGate, 5, 95);
    patch.unison = constrain((int)patch.unison, 1, 3);
}

void loadAndSendPatch(int index) {
    SynthPatch patchToLoad;
    bool loaded = false;
    
    if (index >= 100) {
        // Factory preset
        int factoryIndex = index - 100;
        loaded = loadFactoryPreset(factoryIndex, patchToLoad);
    } else {
        // User patch
        loaded = loadPatchFromFile(index, patchToLoad);
    }
    
    if (loaded) {
        normalizePatchDefaults(patchToLoad);
        currentPatch = patchToLoad;
        if (index < 100) {
            config.currentPatch = index;
            saveConfig();
        }
        
        // Send patch data to web client
        sendPatchDataToClient(index >= 100 ? 0 : index); // Show as user patch 0 for factory presets
        
        // Send to synth
        sendPatchToSynth(currentPatch);
    }
}

void sendPatchDataToClient(int index) {
    JsonDocument doc;
    doc["type"] = "patchData";
    JsonObject patch = doc["patch"].to<JsonObject>();
    
    patch["a"] = currentPatch.a;
    patch["d"] = currentPatch.d;
    patch["s"] = currentPatch.s;
    patch["r"] = currentPatch.r;
    patch["cutoff"] = currentPatch.cutoff;
    patch["resonance"] = currentPatch.resonance;
    patch["morph"] = currentPatch.morph;
    patch["filterOn"] = currentPatch.filterOn;
    patch["glide"] = currentPatch.glide;
    patch["detune"] = currentPatch.detune;
    patch["unison"] = currentPatch.unison;
    patch["dlyTime"] = currentPatch.dlyTime;
    patch["dlyFb"] = currentPatch.dlyFb;
    patch["dlyMix"] = currentPatch.dlyMix;
    patch["crushAmt"] = currentPatch.crushAmt;
    patch["tremRateHz"] = currentPatch.tremRateHz;
    patch["tremDepth"] = currentPatch.tremDepth;
    patch["driveAmt"] = currentPatch.driveAmt;
    patch["foldAmt"] = currentPatch.foldAmt;
    patch["noiseAmt"] = currentPatch.noiseAmt;
    patch["name"] = currentPatch.name;
    patch["lfoToMorph"] = currentPatch.lfoToMorph;
    patch["lfoToAmp"] = currentPatch.lfoToAmp;
    patch["lfoToDetune"] = currentPatch.lfoToDetune;
    patch["chMix"] = currentPatch.chMix;
    patch["chDepth"] = currentPatch.chDepth;
    patch["arpMode"] = currentPatch.arpMode;
    patch["arpDiv"] = currentPatch.arpDiv;
    patch["arpGate"] = currentPatch.arpGate;
    patch["lfo2RateHz"] = currentPatch.lfo2RateHz;
    patch["lfo2AmtSemi"] = currentPatch.lfo2AmtSemi;
    patch["lfo2Wave"] = currentPatch.lfo2Wave;
    patch["stealMode"] = currentPatch.stealMode;
    patch["velCurve"] = currentPatch.velCurve;
    patch["velToCutoff"] = currentPatch.velToCutoff;
    patch["noiseToCutoff"] = currentPatch.noiseToCutoff;
    patch["lfoAmtHz"] = currentPatch.lfoAmtHz;
    patch["lfoRateHz"] = currentPatch.lfoRateHz;
    patch["masterGain"] = currentPatch.masterGain;
    patch["lfoSync"] = currentPatch.lfoSync;
    patch["dlySync"] = currentPatch.dlySync;
    patch["tempoSrc"] = currentPatch.tempoSrc;
    patch["bpmInt"] = currentPatch.bpmInt;
    
    String response;
    serializeJson(doc, response);
    websocket.broadcastTXT(response);
}

const ParamSpec* findParamSpec(const String& param) {
  for (const auto& spec : PARAMS)
    if (param == spec.name) return &spec;
  return nullptr;
}

const ParamSpec* findParamSpec(uint8_t cc) {
  for (const auto& spec : PARAMS)
    if (cc == spec.cc) return &spec;
  return nullptr;
}

int paramValueToCC(const ParamSpec& spec, float value) {
  if (spec.scale == PARAM_BOOL) return value > 0.5f ? 127 : 0;
  if (spec.scale == PARAM_UNISON) return value <= 1.5f ? 0 : (value <= 2.5f ? 64 : 127);

  const float span = spec.hi - spec.lo;
  return constrain((int)(((value - spec.lo) / span) * 127.0f), 0, 127);
}

float ccValueToParam(const ParamSpec& spec, uint8_t v) {
  if (spec.scale == PARAM_BOOL) return v >= 64 ? 1.0f : 0.0f;
  if (spec.scale == PARAM_UNISON) return v < 26 ? 1.0f : (v < 90 ? 2.0f : 3.0f);

  return spec.lo + (spec.hi - spec.lo) * (v / 127.0f);
}

void applyControlToPatch(const String& param, float value) {
    if (param == "attack") currentPatch.a = value;
    else if (param == "decay") currentPatch.d = value;
    else if (param == "sustain") currentPatch.s = value;
    else if (param == "release") currentPatch.r = value;
    else if (param == "cutoff") currentPatch.cutoff = value;
    else if (param == "resonance") currentPatch.resonance = value;
    else if (param == "morph") currentPatch.morph = value;
    else if (param == "filterOn") currentPatch.filterOn = (value > 0.5);
    else if (param == "glide") currentPatch.glide = value;
    else if (param == "detune") currentPatch.detune = value;
    else if (param == "unison") currentPatch.unison = (uint8_t)value;
    else if (param == "delayTime") currentPatch.dlyTime = value;
    else if (param == "delayFeedback") currentPatch.dlyFb = value;
    else if (param == "delayMix") currentPatch.dlyMix = value;
    else if (param == "crushAmt") currentPatch.crushAmt = value;
    else if (param == "tremRateHz") currentPatch.tremRateHz = value;
    else if (param == "tremDepth") currentPatch.tremDepth = value;
    else if (param == "driveAmt") currentPatch.driveAmt = value;
    else if (param == "foldAmt") currentPatch.foldAmt = value;
    else if (param == "noiseAmt") currentPatch.noiseAmt = value;
    else if (param == "masterGain") currentPatch.masterGain = value;
    else if (param == "lfoAmtHz") currentPatch.lfoAmtHz = value;
    else if (param == "lfoRateHz") currentPatch.lfoRateHz = value;
    else if (param == "lfoSync") currentPatch.lfoSync = (value > 0.5);
    else if (param == "dlySync") currentPatch.dlySync = (value > 0.5);
    else if (param == "tempoSrc") currentPatch.tempoSrc = (uint8_t)value;
    else if (param == "bpm") currentPatch.bpmInt = value;
    else if (param == "lfoToMorph") currentPatch.lfoToMorph = value;
    else if (param == "lfoToAmp") currentPatch.lfoToAmp = value;  
    else if (param == "lfoToDetune") currentPatch.lfoToDetune = value;
    else if (param == "chMix") currentPatch.chMix = value;
    else if (param == "chDepth") currentPatch.chDepth = value;
    else if (param == "arpMode") currentPatch.arpMode = (uint8_t)value;
    else if (param == "arpDiv") currentPatch.arpDiv = (uint8_t)value;
    else if (param == "arpGate") currentPatch.arpGate = (uint8_t)value;
    else if (param == "lfo2RateHz") currentPatch.lfo2RateHz = value;
    else if (param == "lfo2AmtSemi") currentPatch.lfo2AmtSemi = value;
    else if (param == "lfo2Wave") currentPatch.lfo2Wave = (uint8_t)value;
    else if (param == "stealMode") currentPatch.stealMode = (uint8_t)value;
    else if (param == "velCurve") currentPatch.velCurve = (uint8_t)value;
    else if (param == "velToCutoff") currentPatch.velToCutoff = value;
    else if (param == "noiseToCutoff") currentPatch.noiseToCutoff = value;
}

void handleControlMessage(const String& param, float value) {
    applyControlToPatch(param, value);
    sendControlToUno(param, value);
}

void sendControlToUno(const String& param, float value) {
    const ParamSpec* spec = findParamSpec(param);
    
    if (spec) {
        // Send as CC command
        int ccVal = paramValueToCC(*spec, value);
        JsonDocument doc;
        doc["t"] = "cc";
        doc["id"] = spec->cc;
        doc["v"] = ccVal;
        
        String jsonStr;
        serializeJson(doc, jsonStr);
        synthSerial.println(jsonStr);
        LOG_PRINTF("-> Uno: %s = %.3f (CC%d=%d)\n", param.c_str(), value, spec->cc, ccVal);
        
    } else {
        // Handle special parameters that don't map to CC
        JsonDocument doc;
        
        if (param == "bpm") {
            doc["t"] = "tempo";
            doc["v"] = value;
        } else if (param == "tempoSrc") {
            doc["t"] = "tempo_src";
            doc["v"] = (int)value;
        } else if (param == "dlySync") {
            doc["t"] = "dlysync";
            doc["v"] = (int)value;
        } else if (param == "lfoToMorph") {
            doc["t"] = "mod";
            doc["morph"] = value;
        } else if (param == "lfoToAmp") {
            doc["t"] = "mod";
            doc["amp"] = value;
        } else if (param == "chMix" || param == "chDepth") {
            doc["t"] = "set";
            doc["key"] = "chorus";
            if (param == "chMix") doc["mix"] = value;
            if (param == "chDepth") doc["depth"] = value;
        } else if (param == "lfo2RateHz" || param == "lfo2AmtSemi" || param == "lfo2Wave") {
            doc["t"] = "set";
            doc["key"] = "lfo2";
            if (param == "lfo2RateHz") doc["rate"] = (uint8_t)constrain((int)roundf((value - 0.05f) / 19.95f * 127.0f), 0, 127);
            if (param == "lfo2AmtSemi") doc["amt"] = (uint8_t)(value / 2.0f * 127);
            if (param == "lfo2Wave") doc["wave"] = (uint8_t)value;
        } else if (param == "arpMode" || param == "arpDiv" || param == "arpGate") {
            doc["t"] = "set";
            doc["key"] = "arp";
            if (param == "arpMode") doc["mode"] = (uint8_t)value;
            if (param == "arpDiv") doc["div"] = (uint8_t)value;
            if (param == "arpGate") doc["gate"] = (uint8_t)value;
        } else if (param == "stealMode" || param == "velCurve") {
            if (param == "stealMode") {
                doc["t"] = "set";
                doc["key"] = "steal";
                doc["v"] = (uint8_t)value;
            } else {
                doc["t"] = "set";
                doc["key"] = "velMode";
                doc["v"] = (uint8_t)value;
            }
        } else if (param == "velToCutoff" || param == "noiseToCutoff") {
            doc["t"] = "set";
            doc["key"] = "mod";
            if (param == "velToCutoff") doc["velCut"] = value;
            if (param == "noiseToCutoff") doc["noiseCut"] = value;
        } else {
            // Unknown parameter, skip
            LOG_PRINTF("Unknown parameter: %s\n", param.c_str());
            return;
        }
        
        String jsonStr;
        serializeJson(doc, jsonStr);
        synthSerial.println(jsonStr);
        LOG_PRINTF("-> Uno Special: %s\n", jsonStr.c_str());
    }
}

void savePatchFromCurrent(int index, const String& name) {
    // Copy current patch data and save it
    strncpy(currentPatch.name, name.c_str(), 16);
    currentPatch.name[16] = '\0';
    
    if (savePatchToFile(index, currentPatch)) {
        LOG_PRINTF("Saved patch %d: %s\n", index, name.c_str());
        broadcastPatchList();
    }
}

void saveCurrentAsCommit() {
    if (savePatchToFile(-1, currentPatch)) {
        LOG_PRINTLN("[saveCurrentAsCommit] Current patch committed");
    }
}

void loadCommitPatch() {
    // Load the committed patch
    SynthPatch commitPatch;
    if (loadPatchFromFile(-1, commitPatch)) {  // Load from commit slot
        normalizePatchDefaults(commitPatch);
        currentPatch = commitPatch;
        sendPatchToSynth(currentPatch);
        sendPatchDataToClient(config.currentPatch);
        LOG_PRINTLN("[loadCommitPatch] Committed patch loaded");
    }
}

void setupWebSocket() {
    websocket.begin();
    websocket.onEvent([](uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
        switch (type) {
            case WStype_CONNECTED:
                LOG_PRINTF("[setupWebSocket] WS[%u] Connected from %s\n", num, websocket.remoteIP(num).toString().c_str());
                // Send patch list immediately on connection
                broadcastPatchList();
                // Also send current patch data
                sendPatchDataToClient(config.currentPatch);
                break;
                
            case WStype_DISCONNECTED:
                LOG_PRINTF("[setupWebSocket] WS[%u] Disconnected\n", num);
                break;
                
            case WStype_TEXT: {
                JsonDocument doc;
                DeserializationError error = deserializeJson(doc, payload, length);
                
                if (error) {
                    LOG_PRINTF("[WebSocket] JSON parse error: %s\n", error.c_str());
                    return;
                }
                
                String msgType = doc["type"] | "";
                
                if (msgType == "control") {
                    String param = doc["param"];
                    float value = doc["value"];
                    handleControlMessage(param, value);

                } else if (msgType == "controls") {
                    JsonArray items = doc["items"].as<JsonArray>();
                    if (items.isNull() || items.size() == 0) {
                        LOG_PRINTLN("[WebSocket] Empty controls batch ignored");
                        break;
                    }

                    JsonDocument batchDoc;
                    batchDoc["t"] = "ccs";
                    JsonArray outItems = batchDoc["items"].to<JsonArray>();

                    for (JsonObject item : items) {
                        int id = item["id"] | -1;
                        int value = item["v"] | -1;
                        if (id < 0 || id > 127 || value < 0 || value > 127) continue;

                        const ParamSpec* spec = findParamSpec((uint8_t)id);
                        if (spec) applyControlToPatch(spec->name, ccValueToParam(*spec, (uint8_t)value));

                        JsonObject outItem = outItems.add<JsonObject>();
                        outItem["id"] = id;
                        outItem["v"] = value;
                    }

                    if (outItems.size() > 0) {
                        String jsonStr;
                        serializeJson(batchDoc, jsonStr);
                        synthSerial.println(jsonStr);
                        LOG_PRINTF("[WebSocket] Forwarded %d CCs in batch\n", outItems.size());
                    }
                    
                } else if (msgType == "noteOn") {
                    uint8_t note = doc["note"];
                    uint8_t velocity = doc["velocity"] | 100;
                    JsonDocument noteDoc;
                    noteDoc["t"] = "noteon";
                    noteDoc["n"] = note;
                    noteDoc["v"] = velocity;
                    String jsonStr;
                    serializeJson(noteDoc, jsonStr);
                    synthSerial.println(jsonStr);
                    LOG_PRINTF("[WebSocket] Note On: %d vel %d\n", note, velocity);
                    
                } else if (msgType == "noteOff") {
                    uint8_t note = doc["note"];
                    JsonDocument noteDoc;
                    noteDoc["t"] = "noteoff";
                    noteDoc["n"] = note;
                    String jsonStr;
                    serializeJson(noteDoc, jsonStr);
                    synthSerial.println(jsonStr);
                    LOG_PRINTF("[WebSocket] Note Off: %d\n", note);
                    
                } else if (msgType == "loadPatch") {
                    int index = doc["index"];
                    LOG_PRINTF("[WebSocket] Loading patch %d\n", index);
                    loadAndSendPatch(index);
                    
                } else if (msgType == "savePatch") {
                    int index = doc["index"];
                    String name = doc["name"];
                    LOG_PRINTF("[WebSocket] Saving patch %d as '%s'\n", index, name.c_str());
                    savePatchFromCurrent(index, name);
                    
                } else if (msgType == "getPatchList") {
                    LOG_PRINTLN("[WebSocket] Patch list requested");
                    broadcastPatchList();

                } else if (msgType == "getState") {
                    JsonDocument stateDoc;
                    stateDoc["t"] = "getstate";
                    String jsonStr;
                    serializeJson(stateDoc, jsonStr);
                    synthSerial.println(jsonStr);
                    LOG_PRINTLN("[WebSocket] State requested from Uno R4");
                    
                } else if (msgType == "panic") {
                    JsonDocument panicDoc;
                    panicDoc["t"] = "cc";
                    panicDoc["id"] = 120;
                    panicDoc["v"] = 127;
                    String jsonStr;
                    serializeJson(panicDoc, jsonStr);
                    synthSerial.println(jsonStr);
                    LOG_PRINTLN("[WebSocket] Panic sent to Uno R4");
                    
                } else if (msgType == "commitPatch") {
                    saveCurrentAsCommit();
                    
                } else if (msgType == "loadCommitPatch") {
                    loadCommitPatch();
                    
                } else if (msgType == "visualization") {
                    int mode = doc["mode"] | 0;
                    JsonDocument vizDoc;
                    vizDoc["t"] = "viz";
                    vizDoc["v"] = mode;
                    String jsonStr;
                    serializeJson(vizDoc, jsonStr);
                    synthSerial.println(jsonStr);
                    LOG_PRINTF("[WebSocket] Visualization mode %d sent to Uno R4\n", mode);
                    
                } else {
                    LOG_PRINTF("[WebSocket] Unknown message type: %s\n", msgType.c_str());
                }
                break;
            }
        }
    });
    
    LOG_PRINTLN("[setupWebSocket] WebSocket server started on port 8081");
}

bool saveConfig() {
    File file = LittleFS.open("/config.json", "w");
    if (!file) {
        LOG_PRINTLN("[saveConfig] Failed to open config.json for writing");
        return false;
    }
    
    JsonDocument doc;
    doc["currentPatch"] = config.currentPatch;
    doc["baudRate"] = config.baudRate;
    
    serializeJson(doc, file);
    file.close();
    
    LOG_PRINTLN("Config saved");
    return true;
}

bool loadConfig() {
    File file = LittleFS.open("/config.json", "r");
    if (!file) {
        LOG_PRINTLN("Config file not found, using defaults");
        saveConfig();  // Create default config
        return false;
    }
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        LOG_PRINTLN("Failed to parse config.json");
        return false;
    }
    
    config.currentPatch = constrain((int)(doc["currentPatch"] | 0), 0, 7);
    config.baudRate = doc["baudRate"] | 115200;
    
    LOG_PRINTLN("Config loaded");
    return true;
}

bool savePatchToFile(int slot, const SynthPatch& patch) {
    String filename;
    
    if (slot == -1) {
        filename = "/commit_patch.json";  // Special commit patch file
    } else {
        if (slot < 0 || slot >= 8) return false;
        filename = "/patch_" + String(slot) + ".json";
    }
    
    File file = LittleFS.open(filename, "w");
    if (!file) {
        LOG_PRINTLN("[savePatchToFile] Failed to open patch file for writing");
        return false;
    }
    
    JsonDocument doc;
    doc["version"] = 1;
    doc["name"] = patch.name;
    
    // Envelope
    JsonObject env = doc["envelope"].to<JsonObject>();
    env["attack"] = patch.a;
    env["decay"] = patch.d;
    env["sustain"] = patch.s;
    env["release"] = patch.r;
    
    // Oscillator
    doc["morph"] = patch.morph;
    
    // Filter
    JsonObject filter = doc["filter"].to<JsonObject>();
    filter["cutoff"] = patch.cutoff;
    filter["resonance"] = patch.resonance;
    filter["enabled"] = patch.filterOn;
    
    // Motion
    doc["glide"] = patch.glide;
    doc["detune"] = patch.detune;
    doc["unison"] = patch.unison;
    
    // Tempo
    JsonObject tempo = doc["tempo"].to<JsonObject>();
    tempo["source"] = patch.tempoSrc;
    tempo["bpm"] = patch.bpmInt;
    tempo["lfoSync"] = patch.lfoSync;
    tempo["delaySync"] = patch.dlySync;
    
    // Mix
    JsonObject mix = doc["mix"].to<JsonObject>();
    mix["masterGain"] = patch.masterGain;
    mix["noiseAmount"] = patch.noiseAmt;
    
    // Delay
    JsonObject delay = doc["delay"].to<JsonObject>();
    delay["time"] = patch.dlyTime;
    delay["feedback"] = patch.dlyFb;
    delay["mix"] = patch.dlyMix;

    JsonObject fx = doc["fx"].to<JsonObject>();
    fx["crush"] = patch.crushAmt;
    fx["tremRate"] = patch.tremRateHz;
    fx["tremDepth"] = patch.tremDepth;
    fx["drive"] = patch.driveAmt;
    fx["fold"] = patch.foldAmt;
    
    // LFO
    JsonObject lfo = doc["lfo"].to<JsonObject>();
    lfo["amount"] = patch.lfoAmtHz;
    lfo["rate"] = patch.lfoRateHz;
    lfo["toMorph"] = patch.lfoToMorph;
    lfo["toAmp"] = patch.lfoToAmp;
    lfo["toDetune"] = patch.lfoToDetune;
    
    // Performance modulation
    JsonObject perf = doc["performance"].to<JsonObject>();
    perf["velToCutoff"] = patch.velToCutoff;
    perf["noiseToCutoff"] = patch.noiseToCutoff;
    
    // LFO2/Vibrato
    JsonObject lfo2 = doc["lfo2"].to<JsonObject>();
    lfo2["rate"] = patch.lfo2RateHz;
    lfo2["amount"] = patch.lfo2AmtSemi;
    lfo2["wave"] = patch.lfo2Wave;
    
    // Chorus
    JsonObject chorus = doc["chorus"].to<JsonObject>();
    chorus["mix"] = patch.chMix;
    chorus["depth"] = patch.chDepth;
    
    // Arp
    JsonObject arp = doc["arp"].to<JsonObject>();
    arp["mode"] = patch.arpMode;
    arp["division"] = patch.arpDiv;
    arp["gate"] = patch.arpGate;
    
    // Voice management
    JsonObject voice = doc["voice"].to<JsonObject>();
    voice["stealMode"] = patch.stealMode;
    voice["velCurve"] = patch.velCurve;
    
    serializeJson(doc, file);
    file.close();
    
    LOG_PRINTF("[savePatchToFile] Saved patch to %s\n", filename.c_str());
    return true;
}

#if DEBUG_FS
void debugFileSystem() {
    Serial.println("[debugFileSystem] === File System Debug ===");
    
    // Check LittleFS status
    FSInfo fs_info;
    if (LittleFS.info(fs_info)) {
        Serial.printf("[debugFileSystem] Total: %d bytes, Used: %d bytes\n", 
                     fs_info.totalBytes, fs_info.usedBytes);
    }
    
    // Check config file
    if (LittleFS.exists("/config.json")) {
        File file = LittleFS.open("/config.json", "r");
        if (file) {
            Serial.printf("[debugFileSystem] config.json size: %d bytes\n", file.size());
            file.close();
        }
    } else {
        Serial.println("[debugFileSystem] config.json does not exist");
    }
    
    // Check each patch file
    Serial.println("\n[debugFileSystem] === Patch Files ===");
    for (int i = 0; i < 8; i++) {
        String filename = "/patch_" + String(i) + ".json";
        if (LittleFS.exists(filename)) {
            File file = LittleFS.open(filename, "r");
            if (file) {
                size_t fileSize = file.size();
                Serial.printf("[debugFileSystem] %s exists, size: %d bytes\n", 
                             filename.c_str(), fileSize);
                
                // Try to load and validate the patch
                file.close();
                SynthPatch testPatch;
                if (loadPatchFromFile(i, testPatch)) {
                    Serial.printf("[debugFileSystem] Patch %d loads OK, name: '%s'\n", 
                                 i, testPatch.name);
                } else {
                    Serial.printf("[debugFileSystem] Patch %d FAILED to load\n", i);
                }
            }
        } else {
            Serial.printf("[debugFileSystem] %s does not exist\n", filename.c_str());
        }
    }
    
    // Check commit patch
    if (LittleFS.exists("/commit_patch.json")) {
        Serial.println("[debugFileSystem] commit_patch.json exists");
    }
    
    Serial.println("[debugFileSystem] === Debug Complete ===\n");
}
#endif

bool loadPatchFromFile(int slot, SynthPatch& patch) {
    String filename;
    
    if (slot == -1) {
        filename = "/commit_patch.json";
    } else {
        if (slot < 0 || slot >= 8) return false;
        filename = "/patch_" + String(slot) + ".json";
    }

    LOG_PRINTF("[loadPatchFromFile] Loading patch from: %s\n", filename.c_str());

    File file = LittleFS.open(filename, "r");
    if (!file) {
        LOG_PRINTF("[loadPatchFromFile] File does not exist: %s\n", filename.c_str());
        return false;
    }

    String content = file.readString();
    file.close(); // Close file before parsing
    LOG_PRINTF("[loadPatchFromFile] File content length: %d bytes\n", content.length());
    
    JsonDocument doc;
    // Parse from the string, not from the file
    DeserializationError error = deserializeJson(doc, content);
    
    if (error) {
        LOG_PRINTF("[loadPatchFromFile] JSON parse error: %s\n", error.c_str());
        return false;
    }
    
    const char* nameFromFile = doc["name"];
    if (nameFromFile) {
        strncpy(patch.name, nameFromFile, 16);
        patch.name[16] = '\0';
        LOG_PRINTF("[loadPatchFromFile] Loaded name: '%s'\n", patch.name);
    } else {
        String defaultName = "P" + String(slot);
        strncpy(patch.name, defaultName.c_str(), 16);
        patch.name[16] = '\0';
        LOG_PRINTF("[loadPatchFromFile] No name in file, using default: '%s'\n", patch.name);
    }
    
    // Load all parameters with defaults
    patch.a = doc["envelope"]["attack"] | 0.1f;
    patch.d = doc["envelope"]["decay"] | 0.2f;
    patch.s = doc["envelope"]["sustain"] | 0.7f;
    patch.r = doc["envelope"]["release"] | 0.5f;
    
    patch.morph = doc["morph"] | 0.5f;
    
    patch.cutoff = doc["filter"]["cutoff"] | 2000.0f;
    patch.resonance = doc["filter"]["resonance"] | 0.9f;
    patch.filterOn = doc["filter"]["enabled"] | true;
    
    patch.glide = doc["glide"] | 0.03f;
    patch.detune = doc["detune"] | 8.0f;
    patch.unison = doc["unison"] | 1;
    
    patch.tempoSrc = doc["tempo"]["source"] | 0;
    patch.bpmInt = doc["tempo"]["bpm"] | 120.0f;
    patch.lfoSync = doc["tempo"]["lfoSync"] | false;
    patch.dlySync = doc["tempo"]["delaySync"] | false;
    
    patch.masterGain = doc["mix"]["masterGain"] | 0.7f;
    patch.noiseAmt = doc["mix"]["noiseAmount"] | 0.0f;
    
    patch.dlyTime = doc["delay"]["time"] | 0.12f;
    patch.dlyFb = doc["delay"]["feedback"] | 0.25f;
    patch.dlyMix = doc["delay"]["mix"] | 0.0f;

    patch.crushAmt = doc["fx"]["crush"] | 0.0f;
    patch.tremRateHz = doc["fx"]["tremRate"] | 5.0f;
    patch.tremDepth = doc["fx"]["tremDepth"] | 0.0f;
    patch.driveAmt = doc["fx"]["drive"] | 0.0f;
    patch.foldAmt = doc["fx"]["fold"] | 0.0f;
    
    patch.lfoAmtHz = doc["lfo"]["amount"] | 600.0f;
    patch.lfoRateHz = doc["lfo"]["rate"] | 3.0f;
    patch.lfoToMorph = doc["lfo"]["toMorph"] | 0.0f;
    patch.lfoToAmp = doc["lfo"]["toAmp"] | 0.0f;
    patch.lfoToDetune = doc["lfo"]["toDetune"] | 0.0f;
    
    // Load modulation parameters
    patch.velToCutoff = doc["performance"]["velToCutoff"] | 0.0f;
    patch.noiseToCutoff = doc["performance"]["noiseToCutoff"] | 0.0f;
    
    // Load LFO2/Vibrato parameters  
    patch.lfo2RateHz = doc["lfo2"]["rate"] | 5.0f;
    patch.lfo2AmtSemi = doc["lfo2"]["amount"] | 0.2f;
    patch.lfo2Wave = doc["lfo2"]["wave"] | 0;
    
    // Load Chorus parameters
    patch.chMix = doc["chorus"]["mix"] | 0.0f;
    patch.chDepth = doc["chorus"]["depth"] | 5.0f;
    
    // Load Arp parameters
    patch.arpMode = doc["arp"]["mode"] | 0;
    patch.arpDiv = doc["arp"]["division"] | 2;
    patch.arpGate = doc["arp"]["gate"] | 60;
    
    // Load Voice Management parameters
    patch.stealMode = doc["voice"]["stealMode"] | 0;
    patch.velCurve = doc["voice"]["velCurve"] | 0;
    
    return true;
}

void sendPatchToSynth(const SynthPatch& patch) {
    LOG_PRINTLN("[sendPatchToSynth] Sending complete patch to Uno R4...");

    const struct {
        const char* param;
        float value;
    } ccParams[] = {
        {"attack", patch.a},
        {"decay", patch.d},
        {"sustain", patch.s},
        {"release", patch.r},
        {"cutoff", patch.cutoff},
        {"resonance", patch.resonance},
        {"morph", patch.morph},
        {"filterOn", patch.filterOn ? 1.0f : 0.0f},
        {"glide", patch.glide},
        {"detune", patch.detune},
        {"masterGain", patch.masterGain},
        {"noiseAmt", patch.noiseAmt},
        {"delayTime", patch.dlyTime},
        {"delayFeedback", patch.dlyFb},
        {"delayMix", patch.dlyMix},
        {"crushAmt", patch.crushAmt},
        {"tremRateHz", patch.tremRateHz},
        {"tremDepth", patch.tremDepth},
        {"driveAmt", patch.driveAmt},
        {"foldAmt", patch.foldAmt},
        {"lfoAmtHz", patch.lfoAmtHz},
        {"lfoRateHz", patch.lfoRateHz},
        {"lfoSync", patch.lfoSync ? 1.0f : 0.0f},
        {"velToCutoff", patch.velToCutoff},
        {"noiseToCutoff", patch.noiseToCutoff},
        {"lfoToDetune", patch.lfoToDetune},
        {"unison", (float)patch.unison}
    };

    const size_t ccCount = sizeof(ccParams) / sizeof(ccParams[0]);
    for (size_t i = 0; i < ccCount;) {
        JsonDocument doc;
        doc["t"] = "ccs";
        JsonArray items = doc["items"].to<JsonArray>();

        uint8_t added = 0;
        while (i < ccCount && added < PATCH_CC_BATCH_MAX) {
            const ParamSpec* spec = findParamSpec(ccParams[i].param);
            if (spec) {
                JsonObject item = items.add<JsonObject>();
                item["id"] = spec->cc;
                item["v"] = paramValueToCC(*spec, ccParams[i].value);
                ++added;
            }
            ++i;
        }

        if (items.size() > 0) {
            String jsonStr;
            serializeJson(doc, jsonStr);
            synthSerial.println(jsonStr);
            delay(2);
        }
    }
    
    // Send special commands that don't map to CC
    delay(10);
    sendControlToUno("bpm", patch.bpmInt);
    sendControlToUno("tempoSrc", patch.tempoSrc);
    sendControlToUno("dlySync", patch.dlySync);
    sendControlToUno("lfoToMorph", patch.lfoToMorph);
    sendControlToUno("lfoToAmp", patch.lfoToAmp);
    sendControlToUno("chMix", patch.chMix);
    sendControlToUno("chDepth", patch.chDepth);
    sendControlToUno("lfo2RateHz", patch.lfo2RateHz);
    sendControlToUno("lfo2AmtSemi", patch.lfo2AmtSemi);
    sendControlToUno("lfo2Wave", patch.lfo2Wave);
    sendControlToUno("arpMode", patch.arpMode);
    sendControlToUno("arpDiv", patch.arpDiv);
    sendControlToUno("arpGate", patch.arpGate);
    sendControlToUno("stealMode", patch.stealMode);
    sendControlToUno("velCurve", patch.velCurve);
    
    LOG_PRINTLN("[sendPatchToSynth] Complete patch sent to Uno R4");
}

void handleSynthMessage(const String& message) {
    LOG_PRINTLN("<- Synth: " + message);
    
    if (message.startsWith("STATUS:")) {
        String status = message.substring(7);
        JsonDocument doc;
        doc["type"] = "status";
        doc["message"] = status;

        String response;
        serializeJson(doc, response);
        websocket.broadcastTXT(response);
        return;
    }

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, message);
    if (error || !doc.is<JsonObject>()) {
        JsonDocument rawDoc;
        rawDoc["type"] = "synthMessage";
        rawDoc["message"] = message;

        String response;
        serializeJson(rawDoc, response);
        websocket.broadcastTXT(response);
        return;
    }

    const char* synthType = doc["t"] | doc["type"] | "";
    if (!synthType || !synthType[0]) {
        doc["type"] = "synthMessage";
        doc["message"] = message;
    } else {
        doc["type"] = synthType;
        doc.remove("t");
    }

    String response;
    serializeJson(doc, response);
    websocket.broadcastTXT(response);
}

void broadcastPatchList() {
    LOG_PRINTLN("[broadcastPatchList] Building patch list...");
    
    JsonDocument doc;
    doc["type"] = "patchList";
    
    // User patches (0-7)
    JsonArray userPatches = doc["userPatches"].to<JsonArray>();
    for (int i = 0; i < 8; i++) {
        SynthPatch patch;
        JsonObject patchObj = userPatches.add<JsonObject>();
        patchObj["index"] = i;
        
        String filename = "/patch_" + String(i) + ".json";
        if (LittleFS.exists(filename) && loadPatchFromFile(i, patch)) {
            patchObj["name"] = String(patch.name);
            patchObj["exists"] = true;
            LOG_PRINTF("[broadcastPatchList] User patch %d: %s (exists)\n", i, patch.name);
        } else {
            String defaultName = "P" + String(i);
            patchObj["name"] = defaultName;
            patchObj["exists"] = false;
            LOG_PRINTF("[broadcastPatchList] User patch %d: %s (empty)\n", i, defaultName.c_str());
        }
    }
    
    // Factory presets (100+)
    JsonArray factoryPatches = doc["factoryPatches"].to<JsonArray>();
    for (int i = 0; i < NUM_FACTORY_PRESETS; i++) {
        JsonObject presetObj = factoryPatches.add<JsonObject>();
        presetObj["index"] = i + 100; // Offset by 100
        presetObj["name"] = String(FACTORY_PRESETS[i].name);
        presetObj["exists"] = true;
        presetObj["readonly"] = true;
        LOG_PRINTF("[broadcastPatchList] Factory preset %d: %s\n", i + 100, FACTORY_PRESETS[i].name);
    }

    String response;
    size_t len = serializeJson(doc, response);
    LOG_PRINTF("[broadcastPatchList] JSON size: %d bytes\n", len);
    LOG_PRINTF("[broadcastPatchList] Broadcasting to %d clients\n", websocket.connectedClients());
    
    websocket.broadcastTXT(response);
    
    // Also send current patch index
    JsonDocument currentDoc;
    currentDoc["type"] = "currentPatch";
    currentDoc["index"] = config.currentPatch;
    String currentResponse;
    serializeJson(currentDoc, currentResponse);
    websocket.broadcastTXT(currentResponse);
    
    LOG_PRINTLN("[broadcastPatchList] Patch list broadcast complete");
}

void initializeDefaultPatches() {
    // Create some default patches if none exist
    for (int i = 0; i < 8; i++) {
        String filename = "/patch_" + String(i) + ".json";
        if (!LittleFS.exists(filename)) {
            // Create a basic default patch
            SynthPatch defaultPatch = {};
            snprintf(defaultPatch.name, sizeof(defaultPatch.name), "P%d", i);
            
            // Basic envelope
            defaultPatch.a = 0.1f;
            defaultPatch.d = 0.2f;
            defaultPatch.s = 0.7f;
            defaultPatch.r = 0.5f;
            
            // Basic tone
            defaultPatch.morph = 0.5f;
            defaultPatch.cutoff = 2000.0f;
            defaultPatch.resonance = 0.9f;
            defaultPatch.filterOn = true;
            
            // Basic settings
            defaultPatch.glide = 0.03f;
            defaultPatch.detune = 8.0f;
            defaultPatch.unison = 1;
            defaultPatch.masterGain = 0.7f;
            defaultPatch.bpmInt = 120.0f;
            
            defaultPatch.lfoAmtHz = 600.0f;
            defaultPatch.lfoRateHz = 3.0f;
            defaultPatch.lfoToMorph = 0.0f;
            defaultPatch.lfoToAmp = 0.0f;
            defaultPatch.lfoToDetune = 0.0f;
            defaultPatch.lfoSync = false;
            
            // Initialize tempo
            defaultPatch.tempoSrc = 0;
            defaultPatch.dlySync = false;
            
            // Initialize delay
            defaultPatch.dlyTime = 0.12f;
            defaultPatch.dlyFb = 0.25f;
            defaultPatch.dlyMix = 0.0f;

            defaultPatch.crushAmt = 0.0f;
            defaultPatch.tremRateHz = 5.0f;
            defaultPatch.tremDepth = 0.0f;
            defaultPatch.driveAmt = 0.0f;
            defaultPatch.foldAmt = 0.0f;
            
            // Initialize noise
            defaultPatch.noiseAmt = 0.0f;
            
            defaultPatch.velToCutoff = 0.0f;
            defaultPatch.noiseToCutoff = 0.0f;
            
            defaultPatch.lfo2RateHz = 5.0f;
            defaultPatch.lfo2AmtSemi = 0.2f;
            defaultPatch.lfo2Wave = 0;
            
            defaultPatch.chMix = 0.0f;
            defaultPatch.chDepth = 5.0f;
            
            defaultPatch.arpMode = 0;
            defaultPatch.arpDiv = 2;
            defaultPatch.arpGate = 60;
            
            defaultPatch.stealMode = 0;
            defaultPatch.velCurve = 0;
            
            savePatchToFile(i, defaultPatch);
        }
    }
    
    LOG_PRINTLN("[initializeDefaultPatches] Default patches initialized");
}
