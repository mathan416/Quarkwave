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
#include <MIDI.h>
#include "QuarkWave_UI.h"
#include "secrets.h"

// Global declarations - put these at the top level
WiFiUDP udp;
MDNS mdns(udp);

// MIDI
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI_UART);

// Server instances
AsyncWebServer server(80);
WebSocketsServer websocket(8081);

// UART to Uno R4 synth
#define synthSerial Serial1
#define LED_PIN LED_BUILTIN 

// Configuration structure
struct Config {
  char deviceName[32];
  uint8_t currentPatch;
  float masterVolume;
  bool autoSave;
  uint32_t baudRate;
};

Config config = {
  .deviceName = "QuarkWave",
  .currentPatch = 0,
  .masterVolume = 0.7f,
  .autoSave = true,
  .baudRate = 115200
};

// mDNS
bool mdnsActive = false;
unsigned long lastMdnsCheck = 0;
const unsigned long MDNS_CHECK_INTERVAL = 30000; // Check every 30 seconds


// Patch structure (matches your Uno R4 SynthPatch)
// Replace your SynthPatch structure with this corrected version:
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

// Update your FACTORY_PRESETS array to include the missing parameters:

const FactoryPreset FACTORY_PRESETS[] = {
  {"Warm Pad", {
    .a=0.08f, .d=0.7f, .s=0.85f, .r=0.8f,
    .morph=0.35f, .cutoff=1400.f, .resonance=0.8f, .filterOn=true,
    .glide=0.06f, .detune=12.f, .unison=3,
    .tempoSrc=0, .lfoSync=false, .dlySync=true, .bpmInt=120.0f,
    .masterGain=0.7f, .noiseAmt=0.0f,
    .dlyTime=0.14f, .dlyFb=0.35f, .dlyMix=0.12f,
    .lfoAmtHz=350.f, .lfoRateHz=0.6f, 
    .lfoToMorph=0.25f, .lfoToAmp=0.10f, .lfoToDetune=0.0f,  // Add these
    .velToCutoff=0.0f, .noiseToCutoff=0.0f,                 // Add these
    .lfo2RateHz=5.0f, .lfo2AmtSemi=0.2f, .lfo2Wave=0,      // Already there
    .chMix=0.0f, .chDepth=5.0f,                             // Already there
    .arpMode=0, .arpDiv=2, .arpGate=60,                     // Add these
    .stealMode=0, .velCurve=0,                              // Add these
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
    .lfoToMorph=0.0f, .lfoToAmp=0.0f, .lfoToDetune=0.0f,   // Add these
    .velToCutoff=0.3f, .noiseToCutoff=0.0f,                // Add these - velocity sensitive pluck
    .lfo2RateHz=5.0f, .lfo2AmtSemi=0.2f, .lfo2Wave=0,
    .chMix=0.0f, .chDepth=5.0f,
    .arpMode=0, .arpDiv=2, .arpGate=60,                    // Add these
    .stealMode=1, .velCurve=2,                             // Add these - last note steal, hard curve
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
    .lfoToMorph=0.0f, .lfoToAmp=0.0f, .lfoToDetune=0.0f,  // Add these
    .velToCutoff=0.4f, .noiseToCutoff=0.0f,               // Add these - velocity sensitive filter
    .lfo2RateHz=5.0f, .lfo2AmtSemi=0.2f, .lfo2Wave=0,
    .chMix=0.0f, .chDepth=5.0f,
    .arpMode=0, .arpDiv=2, .arpGate=60,                   // Add these
    .stealMode=0, .velCurve=0,                            // Add these
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
    .lfoToMorph=0.35f, .lfoToAmp=0.0f, .lfoToDetune=0.1f, // Add these - PWM modulation
    .velToCutoff=0.2f, .noiseToCutoff=0.0f,               // Add these
    .lfo2RateHz=6.5f, .lfo2AmtSemi=0.3f, .lfo2Wave=0,    // Vibrato
    .chMix=0.0f, .chDepth=5.0f,
    .arpMode=0, .arpDiv=2, .arpGate=60,                   // Add these
    .stealMode=1, .velCurve=1,                            // Add these
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
    .lfoToMorph=0.15f, .lfoToAmp=0.05f, .lfoToDetune=0.0f, // Add these
    .velToCutoff=0.6f, .noiseToCutoff=0.0f,                // Add these - very velocity sensitive
    .lfo2RateHz=5.0f, .lfo2AmtSemi=0.2f, .lfo2Wave=0,
    .chMix=0.15f, .chDepth=8.0f,                           // Subtle chorus for EP sound
    .arpMode=0, .arpDiv=2, .arpGate=60,                    // Add these
    .stealMode=0, .velCurve=1,                             // Add these - soft velocity curve
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
    .lfoToMorph=0.35f, .lfoToAmp=0.12f, .lfoToDetune=0.05f, // Add these - sweeping modulation
    .velToCutoff=0.0f, .noiseToCutoff=0.0f,                 // Add these
    .lfo2RateHz=5.0f, .lfo2AmtSemi=0.2f, .lfo2Wave=0,
    .chMix=0.25f, .chDepth=12.0f,                           // Rich chorus for pad
    .arpMode=0, .arpDiv=2, .arpGate=60,                     // Add these
    .stealMode=0, .velCurve=0,                              // Add these
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
    .lfoToMorph=0.0f, .lfoToAmp=0.0f, .lfoToDetune=0.0f,  // Add these
    .velToCutoff=0.8f, .noiseToCutoff=0.0f,               // Add these - very velocity sensitive perc
    .lfo2RateHz=5.0f, .lfo2AmtSemi=0.2f, .lfo2Wave=0,
    .chMix=0.0f, .chDepth=5.0f,
    .arpMode=0, .arpDiv=2, .arpGate=60,                   // Add these
    .stealMode=1, .velCurve=3,                            // Add these - last steal, expo curve
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
    .lfoToMorph=0.2f, .lfoToAmp=0.08f, .lfoToDetune=0.03f,  // Add these - gentle modulation
    .velToCutoff=0.3f, .noiseToCutoff=0.0f,                 // Add these
    .lfo2RateHz=5.0f, .lfo2AmtSemi=0.2f, .lfo2Wave=0,
    .chMix=0.35f, .chDepth=10.0f,                           // Rich chorus for strings
    .arpMode=0, .arpDiv=2, .arpGate=60,                     // Add these
    .stealMode=0, .velCurve=1,                              // Add these - soft curve for strings
    .name="Chrs Strngs"
  }}
};
const int NUM_FACTORY_PRESETS = sizeof(FACTORY_PRESETS) / sizeof(FACTORY_PRESETS[0]);

// Function prototypes
void setupWiFi();
void setupWebServer();
void setupWebSocket();
void setupFileSystem();
void setupSynthSerial();
bool saveConfig();
bool loadConfig();
bool savePatchToFile(int slot, const SynthPatch& patch);
bool loadPatchFromFile(int slot, SynthPatch& patch);
void sendToSynth(const String& command);
void handleSynthMessage(const String& message);
void broadcastPatchList();
void initializeDefaultPatches();
void sendAllCurrentParameters();
void syncWithUno();
void setupLED();
void ledBlink(int times, int delayMs);

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
    Serial.println("[setupMDNS] Initializing mDNS...");
    
    // Start mDNS with hostname
    if (!mdns.begin(WiFi.localIP(), "quarkwave")) {
        Serial.println("[setupMDNS] ERROR: Failed to start mDNS responder");
        return false;
    }
    
    Serial.println("[setupMDNS] mDNS responder started successfully");
    Serial.printf("[setupMDNS] Hostname: quarkwave.local (%s)\n", WiFi.localIP().toString().c_str());
    
    // Add HTTP service using addServiceRecord
    if (mdns.addServiceRecord("quarkwave-http", 80, MDNSServiceTCP)) {
        Serial.println("[setupMDNS] HTTP service registered on port 80");
    } else {
        Serial.println("[setupMDNS] WARNING: Failed to register HTTP service");
    }
    
    // Add WebSocket service
    if (mdns.addServiceRecord("quarkwave-ws", 8081, MDNSServiceTCP)) {
        Serial.println("[setupMDNS] WebSocket service registered on port 8081");
    } else {
        Serial.println("[setupMDNS] WARNING: Failed to register WebSocket service");
    }
    
    // Add custom service for QuarkWave discovery
    if (mdns.addServiceRecord("quarkwave", 80, MDNSServiceTCP)) {
        Serial.println("[setupMDNS] QuarkWave service registered");
    } else {
        Serial.println("[setupMDNS] WARNING: Failed to register QuarkWave service");
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
        Serial.println("[checkMDNS] WiFi disconnected, mDNS may not be working");
        mdnsActive = false;
        return;
    }
    
    // Could add more sophisticated health checks here
    Serial.println("[checkMDNS] mDNS health check passed");
}

void sendAllCurrentParameters() {
    Serial.println("[sendAllCurrentParameters] Syncing all parameters with Uno R4...");
    
    // Helper function to add small delays between MIDI messages
    auto sendCC = [](uint8_t cc, uint8_t value) {
        Serial1.write(0xB0);  // Control Change on channel 1
        Serial1.write(cc);
        Serial1.write(value);
        delay(2);  // Small delay to avoid overwhelming the receiver
    };
    
    auto sendNote = [](bool noteOn, uint8_t note, uint8_t velocity) {
        Serial1.write(noteOn ? 0x90 : 0x80);  // Note On/Off on channel 1
        Serial1.write(note);
        Serial1.write(velocity);
        delay(2);
    };
    
    // === Envelope Parameters ===
    sendCC(73, paramValueToCC("attack", currentPatch.a));        // Attack
    sendCC(75, paramValueToCC("decay", currentPatch.d));         // Decay
    sendCC(23, paramValueToCC("sustain", currentPatch.s));       // Sustain
    sendCC(72, paramValueToCC("release", currentPatch.r));       // Release
    
    // === Filter Parameters ===
    sendCC(74, paramValueToCC("cutoff", currentPatch.cutoff));           // Cutoff
    sendCC(71, paramValueToCC("resonance", currentPatch.resonance));     // Resonance
    sendCC(91, currentPatch.filterOn ? 127 : 0);                         // Filter On/Off
    
    // === Oscillator Parameters ===
    sendCC(76, paramValueToCC("morph", currentPatch.morph));     // Morph
    sendCC(94, paramValueToCC("detune", currentPatch.detune));   // Detune
    sendCC(95, paramValueToCC("unison", (float)currentPatch.unison)); // Unison
    
    // === Amplitude Parameters ===
    sendCC(7, paramValueToCC("masterGain", currentPatch.masterGain));  // Volume
    sendCC(5, paramValueToCC("glide", currentPatch.glide));             // Glide
    sendCC(93, paramValueToCC("noiseAmt", currentPatch.noiseAmt));      // Noise Amount
    
    // === Delay Parameters ===
    sendCC(12, paramValueToCC("delayTime", currentPatch.dlyTime));         // Delay Time
    sendCC(13, paramValueToCC("delayFeedback", currentPatch.dlyFb));       // Delay Feedback
    sendCC(14, paramValueToCC("delayMix", currentPatch.dlyMix));           // Delay Mix
    
    // === LFO Parameters ===
    sendCC(1, paramValueToCC("lfoAmtHz", currentPatch.lfoAmtHz));     // LFO Amount
    sendCC(2, paramValueToCC("lfoRateHz", currentPatch.lfoRateHz));   // LFO Rate
    sendCC(3, currentPatch.lfoSync ? 127 : 0);                        // LFO Sync
    
    // === Modulation Matrix ===
    sendCC(24, paramValueToCC("velToCutoff", currentPatch.velToCutoff));     // Vel -> Cutoff
    sendCC(25, paramValueToCC("noiseToCutoff", currentPatch.noiseToCutoff)); // Noise -> Cutoff
    sendCC(26, paramValueToCC("lfoToDetune", currentPatch.lfoToDetune));     // LFO -> Detune
    
    // === Performance Parameters ===
    sendCC(16, (uint8_t)currentPatch.velCurve);      // Velocity Curve
    sendCC(17, (uint8_t)currentPatch.stealMode);     // Voice Steal Mode
    
    // === Arpeggiator Parameters ===
    sendCC(18, (uint8_t)currentPatch.arpMode * 25);  // Arp Mode (mapped to CC range)
    sendCC(19, (uint8_t)currentPatch.arpDiv * 18);   // Arp Division (mapped to CC range)
    sendCC(20, (uint8_t)currentPatch.arpGate);       // Arp Gate
    
    // === Chorus Parameters ===
    sendCC(21, paramValueToCC("chMix", currentPatch.chMix));  // Chorus Mix
    
    // === LFO2/Vibrato Parameters ===
    sendCC(27, (uint8_t)((currentPatch.lfo2RateHz - 0.1f) / 20.0f * 127));  // LFO2 Rate
    sendCC(28, (uint8_t)(currentPatch.lfo2AmtSemi / 2.0f * 127));            // LFO2 Amount
    
    // === Special Parameters (using JSON-like commands via SysEx or custom handling) ===
    
    // Tempo Source
    sendSpecialCommand("tempoSrc", currentPatch.tempoSrc);
    
    // BPM (internal)
    sendSpecialCommand("bpm", currentPatch.bpmInt);
    
    // Delay Sync
    sendSpecialCommand("dlySync", currentPatch.dlySync ? 1.0f : 0.0f);
    
    // LFO Routing
    sendSpecialCommand("lfoToMorph", currentPatch.lfoToMorph);
    sendSpecialCommand("lfoToAmp", currentPatch.lfoToAmp);
    
    // Chorus Depth
    sendSpecialCommand("chDepth", currentPatch.chDepth);
    
    // LFO2 Wave
    sendSpecialCommand("lfo2Wave", (float)currentPatch.lfo2Wave);
    
    Serial.println("[sendAllCurrentParameters] Parameter sync complete");
}

// Helper function for parameters that don't map to standard MIDI CC
/*
void sendSpecialCommand(const String& param, float value) {
    // Convert back to JSON for special commands that don't have CC equivalents
    // This is a hybrid approach - CC for standard params, JSON for special ones
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
    } else if (param == "chDepth") {
        doc["t"] = "set";
        doc["key"] = "chorus";
        doc["depth"] = value;
    } else if (param == "lfo2Wave") {
        doc["t"] = "set";
        doc["key"] = "lfo2";
        doc["wave"] = (uint8_t)value;
    }
    
    String jsonStr;
    serializeJson(doc, jsonStr);
    
    // Send as MIDI System Exclusive message
    String sysexData = "F0 7D 00 " + jsonStr + " F7";  // Wrap JSON in SysEx
    // Or just send raw JSON if you keep some JSON processing on Uno side
    Serial1.println(jsonStr);
    delay(5);
}
*/

// Replace JSON commands with MIDI Program Changes and SysEx:
void sendSpecialCommand(const String& param, float value) {
    Serial.printf("[sendSpecialCommand] %s = %.3f\n", param.c_str(), value);
    
    if (param == "tempoSrc") {
        // PC 100=External, PC 101=Internal
        MIDI_UART.sendProgramChange(value > 0 ? 100 : 101, 1);
        
    } else if (param == "dlySync") {
        // PC 102=Sync On, PC 103=Sync Off
        MIDI_UART.sendProgramChange(value > 0 ? 102 : 103, 1);
        
    } else if (param == "bpm") {
        // Use SysEx for BPM (needs more precision than PC)
        sendBPMSysEx((uint16_t)value);
        
    } else if (param == "lfoToMorph") {
        // Use CC-like SysEx for modulation routing
        sendModRoutingSysEx(0, (uint8_t)(value * 127)); // 0 = LFO->Morph
        
    } else if (param == "lfoToAmp") {
        sendModRoutingSysEx(1, (uint8_t)(value * 127)); // 1 = LFO->Amp
        
    } else if (param == "chDepth") {
        // Chorus depth via SysEx
        sendChorusSysEx((uint8_t)value);
        
    } else if (param == "lfo2Wave") {
        // PC 110-112 for LFO2 wave types
        MIDI_UART.sendProgramChange(110 + (uint8_t)value, 1);
        
    } else {
        Serial.printf("[sendSpecialCommand] Unknown parameter: %s\n", param.c_str());
    }
    
    delay(2);
}

// Helper functions for SysEx messages
void sendBPMSysEx(uint16_t bpm) {
    uint8_t sysex[] = {
        0xF0,           // SysEx start
        0x7D,           // Non-commercial manufacturer ID
        0x00,           // QuarkWave device ID
        0x01,           // Command: Set BPM
        (uint8_t)(bpm & 0x7F),        // BPM low 7 bits
        (uint8_t)((bpm >> 7) & 0x7F), // BPM high 7 bits
        0xF7            // SysEx end
    };
    MIDI_UART.sendSysEx(sizeof(sysex), sysex, true);
}

void sendModRoutingSysEx(uint8_t routingType, uint8_t amount) {
    uint8_t sysex[] = {
        0xF0,           // SysEx start
        0x7D,           // Non-commercial manufacturer ID  
        0x00,           // QuarkWave device ID
        0x02,           // Command: Modulation routing
        routingType,    // 0=LFO->Morph, 1=LFO->Amp, etc.
        amount & 0x7F,  // Amount (7-bit)
        0xF7            // SysEx end
    };
    MIDI_UART.sendSysEx(sizeof(sysex), sysex, true);
}

void sendChorusSysEx(uint8_t depth) {
    uint8_t sysex[] = {
        0xF0,           // SysEx start
        0x7D,           // Non-commercial manufacturer ID
        0x00,           // QuarkWave device ID
        0x03,           // Command: Chorus depth
        depth & 0x7F,   // Depth (7-bit)
        0xF7            // SysEx end
    };
    MIDI_UART.sendSysEx(sizeof(sysex), sysex, true);
}

void syncWithUno() {
  // Option 1: Send Program Change 0 to trigger default patch load
  MIDI_UART.sendProgramChange(0, 1);  
  delay(100);  // Let Uno reset
  
  // Then send all current UI parameter values
  sendAllCurrentParameters();
}


void setup() {
    Serial.begin(115200);
    delay(2000);

    // Intro
    Serial.print(F("QuarkWave\n"));
    Serial.print(F("Web Synthesis Sound Design Studio\n"));
    Serial.print(F("(C) 2025 Iain Bennett\n"));
    Serial.print(F("---------------------------------\n"));
    
    setupLED();  // Initialize LED first
    ledBlink(2);  // 2 blinks = startup
    
    Serial.println("[BOOT] QuarkWave Web Interface Starting...");
    
    setupFileSystem();
    debugFileSystem();
    setupSynthSerial();
    
    Serial.println("[BOOT] Attempting WiFi connection...");
    setupWiFi();
    
    if (WiFi.status() == WL_CONNECTED) {
        digitalWrite(LED_PIN, HIGH);  // LED ON = WiFi connected
        Serial.println("[BOOT] LED ON - WiFi Connected!");
        
        setupWebServer();
        setupWebSocket();
        
        // mDNS setup
        if (setupMDNS()) {
            ledBlink(3, 100);  // 3 quick blinks = mDNS ready
            Serial.println("[BOOT] mDNS ready - LED blinked 3 times");
        } else {
            ledBlink(5, 200);  // 5 slow blinks = mDNS failed
            Serial.println("[BOOT] mDNS failed - LED blinked 5 times");
        }
        
        digitalWrite(LED_PIN, HIGH);  // Back to solid on
    } else {
        // Slow blink pattern for WiFi failure
        for (int i = 0; i < 10; i++) {
            digitalWrite(LED_PIN, HIGH);
            delay(1000);
            digitalWrite(LED_PIN, LOW);
            delay(1000);
        }
    }
    
    initializeDefaultPatches();
    Serial.println("[BOOT] System ready!");
}

void loop() {
    if (mdnsActive) {
        mdns.run();
        checkMDNS();  // Add health monitoring
    }

    websocket.loop(); 
    
    // Heartbeat blink every 30 seconds if connected
    static unsigned long lastHeartbeat = 0;
    if (WiFi.status() == WL_CONNECTED && millis() - lastHeartbeat > 30000) {
        lastHeartbeat = millis();
        digitalWrite(LED_PIN, LOW);
        delay(50);
        digitalWrite(LED_PIN, HIGH);  // Quick off/on heartbeat
    }
    
    // Handle UART messages...
    if (Serial1.available()) {
        String message = Serial1.readStringUntil('\n');
        message.trim();
        if (message.length() > 0) {
            handleSynthMessage(message);
        }
    }
    
    delay(1);
}



void setupWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("[setupWiFi] Connecting to WiFi");
    //Serial.println(WIFI_SSID);
    //Serial.println(WIFI_PASS);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    
    Serial.println();
    Serial.print("[setupWiFi] Connected! IP: ");
    Serial.println(WiFi.localIP());
}

void setupFileSystem() {
    if (!LittleFS.begin()) {
        Serial.println("LittleFS mount failed - formatting...");
        if (LittleFS.format()) {
            Serial.println("LittleFS formatted successfully");
            LittleFS.begin();
        } else {
            Serial.println("LittleFS format failed!");
            return;
        }
    }
    Serial.println("[setupFileSystem] LittleFS ready");
}

void setupSynthSerial() {
    Serial1.setTX(0);
    Serial1.setRX(1);
    Serial1.begin(31250);
    MIDI_UART.begin(MIDI_CHANNEL_OMNI); 
    Serial.println("[setupSynthSerial] MIDI UART initialized");
}

void setupWebServer() {
    // Serve main page
    Serial.printf("[setupWebServer] INDEX_HTML size: %d bytes\n", strlen(INDEX_HTML));
    Serial.printf("[setupWebServer] INDEX_HTML_LEN: %d bytes\n", INDEX_HTML_LEN );

    
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/html; charset=utf-8", INDEX_HTML);
    });
    
    // API endpoints
    server.on("/api/patches", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        JsonArray patches = doc["patches"].to<JsonArray>();
        
        for (int i = 0; i < 8; i++) {
            SynthPatch patch;
            JsonObject patchObj = patches.add<JsonObject>();
            patchObj["index"] = i;
            
            if (loadPatchFromFile(i, patch)) {
                patchObj["name"] = patch.name;
                patchObj["exists"] = true;
            } else {
                patchObj["name"] = "Empty " + String(i);
                patchObj["exists"] = false;
            }
        }
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    // mDNS status endpoint
    server.on("/api/mdns", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        doc["mdns_active"] = mdnsActive;
        doc["hostname"] = "quarkwave.local";
        doc["ip"] = WiFi.localIP().toString();
        
        if (mdnsActive) {
            JsonArray services = doc["services"].to<JsonArray>();
            services.add("http:80");
            services.add("ws:8081");
            services.add("quarkwave:80");
        }
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });
    
    server.begin();
    Serial.println("[setupWebServer] Web server started");
}

bool loadFactoryPreset(int index, SynthPatch& patch) {
  if (index < 0 || index >= NUM_FACTORY_PRESETS) return false;
  patch = FACTORY_PRESETS[index].patch;
  return true;
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

int getParamCC(const String& param) {
  if (param == "attack") return 73;
  if (param == "decay") return 75;
  if (param == "sustain") return 23;
  if (param == "release") return 72;
  if (param == "cutoff") return 74;
  if (param == "resonance") return 71;
  if (param == "morph") return 76;
  if (param == "filterOn") return 91;
  if (param == "glide") return 5;
  if (param == "detune") return 94;
  if (param == "masterGain") return 7;
  if (param == "noiseAmt") return 93;
  if (param == "delayTime") return 12;
  if (param == "delayFeedback") return 13;
  if (param == "delayMix") return 14;
  if (param == "lfoAmtHz") return 1;
  if (param == "lfoRateHz") return 2;
  if (param == "lfoSync") return 3;
  if (param == "velToCutoff") return 24;
  if (param == "noiseToCutoff") return 25;
  if (param == "lfoToDetune") return 26;
  if (param == "unison") return 95;
  return -1;
}

int paramValueToCC(const String& param, float value) {
  if (param == "attack") return constrain((int)((value - 0.002) / 0.498 * 127), 0, 127);
  if (param == "decay") return constrain((int)((value - 0.01) / 0.99 * 127), 0, 127);
  if (param == "sustain") return constrain((int)((value - 0.10) / 0.75 * 127), 0, 127);
  if (param == "release") return constrain((int)((value - 0.02) / 1.48 * 127), 0, 127);
  if (param == "cutoff") return constrain((int)((value - 40) / 9960 * 127), 0, 127);
  if (param == "resonance") return constrain((int)((value - 0.5) / 2.5 * 127), 0, 127);
  if (param == "morph") return constrain((int)(value * 127), 0, 127);
  if (param == "filterOn") return value > 0.5 ? 127 : 0;
  if (param == "glide") return constrain((int)(value / 0.3 * 127), 0, 127);
  if (param == "detune") return constrain((int)((value - 2) / 18 * 127), 0, 127);
  if (param == "masterGain") return constrain((int)((value - 0.2) / 0.8 * 127), 0, 127);
  if (param == "noiseAmt") return constrain((int)(value * 127), 0, 127);
  if (param == "delayTime") return constrain((int)((value - 0.02) / 0.146 * 127), 0, 127);
  if (param == "delayFeedback") return constrain((int)((value - 0.01) / 0.88 * 127), 0, 127);
  if (param == "delayMix") return constrain((int)(value * 127), 0, 127);
  if (param == "lfoAmtHz") return constrain((int)(value / 3000 * 127), 0, 127);
  if (param == "lfoRateHz") return constrain((int)((value - 0.1) / 12 * 127), 0, 127);
  if (param == "lfoSync") return value > 0.5 ? 127 : 0;
  if (param == "velToCutoff") return constrain((int)(value * 127), 0, 127);
  if (param == "noiseToCutoff") return constrain((int)(value * 127), 0, 127);
  if (param == "lfoToDetune") return constrain((int)(value * 127), 0, 127);
  if (param == "unison") {
    if (value <= 1.5) return 0;    // 1 voice
    if (value <= 2.5) return 64;   // 2 voices  
    return 127;                    // 3 voices
  }
  return constrain((int)(value * 127), 0, 127);
}

void handleControlMessage(const String& param, float value) {
    // Update current patch data
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
    
    // Send to Uno R4 as JSON
    sendControlToUno(param, value);
}

/*
void sendControlToUno(const String& param, float value) {
    int ccNum = getParamCC(param);
    
    if (ccNum >= 0) {
        // Send as CC command
        int ccVal = paramValueToCC(param, value);
        JsonDocument doc;
        doc["t"] = "cc";
        doc["id"] = ccNum;
        doc["v"] = ccVal;
        
        String jsonStr;
        serializeJson(doc, jsonStr);
        synthSerial.println(jsonStr);
        Serial.printf("-> Uno: %s = %.3f (CC%d=%d)\n", param.c_str(), value, ccNum, ccVal);
        
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
            if (param == "lfo2RateHz") doc["rate"] = (uint8_t)(value / 20.0f * 127);
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
            Serial.printf("Unknown parameter: %s\n", param.c_str());
            return;
        }
        
        String jsonStr;
        serializeJson(doc, jsonStr);
        synthSerial.println(jsonStr);
        Serial.printf("-> Uno Special: %s\n", jsonStr.c_str());
    }
}
*/

// Replace JSON commands with MIDI:
void sendControlToUno(const String& param, float value) {
    int ccNum = getParamCC(param);
    if (ccNum >= 0) {
        int ccVal = paramValueToCC(param, value);
        MIDI_UART.sendControlChange(ccNum, ccVal, 1);
        Serial.printf("-> Uno: %s = %.3f (CC%d=%d)\n", param.c_str(), value, ccNum, ccVal);
    }   
}

void savePatchFromCurrent(int index, const String& name) {
    // Copy current patch data and save it
    strncpy(currentPatch.name, name.c_str(), 16);
    currentPatch.name[16] = '\0';
    
    if (savePatchToFile(index, currentPatch)) {
        Serial.printf("Saved patch %d: %s\n", index, name.c_str());
        broadcastPatchList();
    }
}

void saveCurrentAsCommit() {
    // Save current patch as a special "committed" patch
    // You can save it as a special file or designated slot
    if (savePatchToFile(-1, currentPatch)) {  // Use -1 for commit patch
        Serial.println("[saveCurrentAsCommit] Current patch committed");
    }
    // Or save to a special commit file:
    // File file = LittleFS.open("/commit_patch.json", "w");
    // ... save currentPatch data to this file
}

void loadCommitPatch() {
    // Load the committed patch
    SynthPatch commitPatch;
    if (loadPatchFromFile(-1, commitPatch)) {  // Load from commit slot
        currentPatch = commitPatch;
        sendPatchToSynth(currentPatch);
        sendPatchDataToClient(config.currentPatch);
        Serial.println("[loadCommitPatch] Committed patch loaded");
    }
    // Or load from special commit file:
    // File file = LittleFS.open("/commit_patch.json", "r");
    // ... load and apply patch
}

void setupWebSocket() {
    websocket.begin();
    websocket.onEvent([](uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
        switch (type) {
            case WStype_CONNECTED:
                Serial.printf("[setupWebSocket] WS[%u] Connected from %s\n", num, websocket.remoteIP(num).toString().c_str());
                // Send patch list immediately on connection
                broadcastPatchList();
                // Also send current patch data
                sendPatchDataToClient(config.currentPatch);
                break;
                
            case WStype_DISCONNECTED:
                Serial.printf("[setupWebSocket] WS[%u] Disconnected\n", num);
                break;
                
            case WStype_TEXT: {
                String message = String((char*)payload);
                Serial.printf("[WebSocket] Received: %s\n", message.c_str());
                
                JsonDocument doc;
                DeserializationError error = deserializeJson(doc, message);
                
                if (error) {
                    Serial.printf("[WebSocket] JSON parse error: %s\n", error.c_str());
                    return;
                }
                
                String msgType = doc["type"] | doc["t"]; // Support both 'type' and 't'
                
                if (msgType == "control") {
                    String param = doc["param"];
                    float value = doc["value"];
                    handleControlMessage(param, value);
                    
                } else if (msgType == "noteOn") {
                    uint8_t note = doc["note"];
                    uint8_t velocity = doc["velocity"] | 100;
                    MIDI_UART.sendNoteOn(note, velocity, 1);
                    Serial.printf("[WebSocket] Note On: %d vel %d\n", note, velocity);
                    Serial.printf("[WebSocket] Note On: %d vel %d\n", note, velocity);
                    
                } else if (msgType == "noteOff") {
                    uint8_t note = doc["note"];
                    JsonDocument noteDoc;
                    noteDoc["t"] = "noteoff";
                    noteDoc["n"] = note;
                    String jsonStr;
                    serializeJson(noteDoc, jsonStr);
                    synthSerial.println(jsonStr);
                    Serial.printf("[WebSocket] Note Off: %d\n", note);
                    
                } else if (msgType == "loadPatch") {
                    int index = doc["index"];
                    Serial.printf("[WebSocket] Loading patch %d\n", index);
                    loadAndSendPatch(index);
                    
                } else if (msgType == "savePatch") {
                    int index = doc["index"];
                    String name = doc["name"];
                    Serial.printf("[WebSocket] Saving patch %d as '%s'\n", index, name.c_str());
                    savePatchFromCurrent(index, name);
                    
                } else if (msgType == "getPatchList") {
                    Serial.println("[WebSocket] Patch list requested");
                    broadcastPatchList();
                    
                } else if (msgType == "panic") {
                    MIDI_UART.sendControlChange(120, 127, 1);  // All Sound Off
                    Serial.println("[WebSocket] Panic sent to Uno R4");
                    
                } else if (msgType == "commitPatch") {
                    saveCurrentAsCommit();
                    
                } else if (msgType == "loadCommitPatch") {
                    loadCommitPatch();
                    
                } else if (msgType == "visualization" || msgType == "viz") {
                    int mode = doc["mode"] | doc["v"];
                    MIDI_UART.sendProgramChange(mode + 10, 1); 
                    Serial.printf("[WebSocket] Visualization mode %d sent to Uno R4\n", mode);
                    
                } else {
                    Serial.printf("[WebSocket] Unknown message type: %s\n", msgType.c_str());
                }
                break;
            }
        }
    });
    
    Serial.println("[setupWebSocket] WebSocket server started on port 8081");
}

bool saveConfig() {
    File file = LittleFS.open("/config.json", "w");
    if (!file) {
        Serial.println("[saveConfig] Failed to open config.json for writing");
        return false;
    }
    
    JsonDocument doc;
    doc["deviceName"] = config.deviceName;
    doc["currentPatch"] = config.currentPatch;
    doc["masterVolume"] = config.masterVolume;
    doc["autoSave"] = config.autoSave;
    doc["baudRate"] = config.baudRate;
    
    serializeJson(doc, file);
    file.close();
    
    Serial.println("Config saved");
    return true;
}

bool loadConfig() {
    File file = LittleFS.open("/config.json", "r");
    if (!file) {
        Serial.println("Config file not found, using defaults");
        saveConfig();  // Create default config
        return false;
    }
    
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        Serial.println("Failed to parse config.json");
        return false;
    }
    
    strncpy(config.deviceName, doc["deviceName"] | "QuarkWave", 31);
    config.currentPatch = doc["currentPatch"] | 0;
    config.masterVolume = doc["masterVolume"] | 0.7f;
    config.autoSave = doc["autoSave"] | true;
    config.baudRate = doc["baudRate"] | 115200;
    
    Serial.println("Config loaded");
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
        Serial.println("[savePatchToFile] Failed to open patch file for writing");
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
    
    Serial.printf("[savePatchToFile] Saved patch to %s\n", filename.c_str());
    return true;
}

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

bool loadPatchFromFile(int slot, SynthPatch& patch) {
    String filename;
    
    if (slot == -1) {
        filename = "/commit_patch.json";
    } else {
        if (slot < 0 || slot >= 8) return false;
        filename = "/patch_" + String(slot) + ".json";
    }

    Serial.printf("[loadPatchFromFile] Loading patch from: %s\n", filename.c_str());

    File file = LittleFS.open(filename, "r");
    if (!file) {
        Serial.printf("[loadPatchFromFile] File does not exist: %s\n", filename.c_str());
        return false;
    }

    String content = file.readString();
    file.close(); // Close file before parsing
    Serial.printf("[loadPatchFromFile] File content length: %d bytes\n", content.length());
    
    JsonDocument doc;
    // Parse from the string, not from the file
    DeserializationError error = deserializeJson(doc, content);
    
    if (error) {
        Serial.printf("[loadPatchFromFile] JSON parse error: %s\n", error.c_str());
        return false;
    }
    
    const char* nameFromFile = doc["name"];
    if (nameFromFile) {
        strncpy(patch.name, nameFromFile, 16);
        patch.name[16] = '\0';
        Serial.printf("[loadPatchFromFile] Loaded name: '%s'\n", patch.name);
    } else {
        String defaultName = "P" + String(slot);
        strncpy(patch.name, defaultName.c_str(), 16);
        patch.name[16] = '\0';
        Serial.printf("[loadPatchFromFile] No name in file, using default: '%s'\n", patch.name);
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

void sendToSynth(const String& command) {
    synthSerial.println(command);
    Serial.println("-> Synth: " + command);
}

void sendPatchToSynth(const SynthPatch& patch) {
    Serial.println("[sendPatchToSynth] Sending complete patch to Uno R4...");
    
    // Build batch CC command
    JsonDocument doc;
    doc["t"] = "ccs";
    JsonArray items = doc["items"].to<JsonArray>();
    
    // Add all CC-mappable parameters
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
        {"lfoAmtHz", patch.lfoAmtHz},
        {"lfoRateHz", patch.lfoRateHz},
        {"lfoSync", patch.lfoSync ? 1.0f : 0.0f},
        {"velToCutoff", patch.velToCutoff},
        {"noiseToCutoff", patch.noiseToCutoff},
        {"lfoToDetune", patch.lfoToDetune},
        {"unison", (float)patch.unison}
    };
    
    for (const auto& p : ccParams) {
        int ccNum = getParamCC(p.param);
        if (ccNum >= 0) {
            JsonObject item = items.add<JsonObject>();
            item["id"] = ccNum;
            item["v"] = paramValueToCC(p.param, p.value);
        }
    }
    
    String jsonStr;
    serializeJson(doc, jsonStr);
    synthSerial.println(jsonStr);
    
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
    
    Serial.println("[sendPatchToSynth] Complete patch sent to Uno R4");
}

void handleSynthMessage(const String& message) {
    Serial.println("<- Synth: " + message);
    
    // Handle status messages from synth
    if (message.startsWith("STATUS:")) {
        String status = message.substring(7);
        // Could broadcast status to web clients
    }
}

void broadcastPatchList() {
    Serial.println("[broadcastPatchList] Building patch list...");
    
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
            Serial.printf("[broadcastPatchList] User patch %d: %s (exists)\n", i, patch.name);
        } else {
            String defaultName = "P" + String(i);
            patchObj["name"] = defaultName;
            patchObj["exists"] = false;
            Serial.printf("[broadcastPatchList] User patch %d: %s (empty)\n", i, defaultName.c_str());
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
        Serial.printf("[broadcastPatchList] Factory preset %d: %s\n", i + 100, FACTORY_PRESETS[i].name);
    }

    String response;
    size_t len = serializeJson(doc, response);
    Serial.printf("[broadcastPatchList] JSON size: %d bytes\n", len);
    Serial.printf("[broadcastPatchList] Broadcasting to %d clients\n", websocket.connectedClients());
    
    websocket.broadcastTXT(response);
    
    // Also send current patch index
    JsonDocument currentDoc;
    currentDoc["type"] = "currentPatch";
    currentDoc["index"] = config.currentPatch;
    String currentResponse;
    serializeJson(currentDoc, currentResponse);
    websocket.broadcastTXT(currentResponse);
    
    Serial.println("[broadcastPatchList] Patch list broadcast complete");
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
            
            // Initialize modulation parameters (ADD THESE)
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
            
            // Initialize noise
            defaultPatch.noiseAmt = 0.0f;
            
            // Initialize modulation routing (ADD THESE)
            defaultPatch.velToCutoff = 0.0f;
            defaultPatch.noiseToCutoff = 0.0f;
            
            // Initialize LFO2/Vibrato (ADD THESE)
            defaultPatch.lfo2RateHz = 5.0f;
            defaultPatch.lfo2AmtSemi = 0.2f;
            defaultPatch.lfo2Wave = 0;
            
            // Initialize Chorus (ADD THESE)
            defaultPatch.chMix = 0.0f;
            defaultPatch.chDepth = 5.0f;
            
            // Initialize Arp (ADD THESE)
            defaultPatch.arpMode = 0;
            defaultPatch.arpDiv = 2;
            defaultPatch.arpGate = 60;
            
            // Initialize Voice Management (ADD THESE)
            defaultPatch.stealMode = 0;
            defaultPatch.velCurve = 0;
            
            savePatchToFile(i, defaultPatch);
        }
    }
    
    Serial.println("[initializeDefaultPatches] Default patches initialized");
}