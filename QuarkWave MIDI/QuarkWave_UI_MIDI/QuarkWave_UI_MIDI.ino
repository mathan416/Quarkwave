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
  - GPIO 1 (RX) <- Uno R4 TX only through a 5 V-to-3.3 V level shifter
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
#include <AppleMIDI.h>
#include "QuarkWave_UI.h"
#include "secrets.h"

struct ParamCCMap;
struct SynthPatch;

// Global declarations
WiFiUDP udp;
MDNS mdns(udp);

// MIDI
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI_UART);
APPLEMIDI_CREATE_INSTANCE(WiFiUDP, rtpMIDI, "QuarkWave", 5004);

static bool rtpInitialized = false;
static bool rtpConnected = false;
static bool rtpSoundActivity = false;
static bool rtpActivityReported = false;
static uint32_t lastRtpMarkerMs = 0;
void broadcastUnoStatus();

// The gateway accepts only QuarkWave's public sound commands, never link,
// reset, or matrix-display commands from an external RTP peer.
static bool rtpSoundProgram(uint8_t program) {
  return (program >= 100 && program <= 103) ||
         (program >= 110 && program <= 112);
}

static bool rtpSoundSysEx(const byte* data, unsigned size) {
  if (!data || size < 6 || data[0] != 0xF0 || data[1] != 0x7D ||
      data[2] != 0x00 || data[size - 1] != 0xF7) return false;
  if (data[3] == 0x01 && size == 7 && data[4] < 128 && data[5] < 128) {
    const uint16_t bpm = data[4] | (data[5] << 7);
    return bpm >= 40 && bpm <= 240;
  }
  return (data[3] == 0x02 && size == 7 && data[4] <= 1 && data[5] < 128) ||
         (data[3] == 0x03 && size == 6 && data[4] <= 20);
}

static void markRtpSoundActivity() {
  rtpSoundActivity = true;
  const uint32_t now = millis();
  if (rtpActivityReported && now - lastRtpMarkerMs < 250) return;
  // This marker precedes the forwarded sound command in the same UART stream.
  // It distinguishes a wireless controller from a Pico patch transaction.
  const uint8_t marker[] = {0xF0, 0x7D, 0x00, 0x14, 0xF7};
  MIDI_UART.sendSysEx(sizeof(marker), marker, true);
  rtpActivityReported = true;
  lastRtpMarkerMs = now;
}

static void setupRtpGateway() {
  if (rtpInitialized) return;
  rtpInitialized = true;
  rtpMIDI.begin(MIDI_CHANNEL_OMNI);
  rtpMIDI.turnThruOff();
  rtpMIDI.setHandleNoteOn([](byte channel, byte note, byte velocity) {
    markRtpSoundActivity();
    MIDI_UART.sendNoteOn(note, velocity, channel);
  });
  rtpMIDI.setHandleNoteOff([](byte channel, byte note, byte velocity) {
    markRtpSoundActivity();
    MIDI_UART.sendNoteOff(note, velocity, channel);
  });
  rtpMIDI.setHandleControlChange([](byte channel, byte cc, byte value) {
    markRtpSoundActivity();
    MIDI_UART.sendControlChange(cc, value, channel);
  });
  rtpMIDI.setHandlePitchBend([](byte channel, int bend) {
    markRtpSoundActivity();
    MIDI_UART.sendPitchBend(bend, channel);
  });
  rtpMIDI.setHandleProgramChange([](byte channel, byte program) {
    if (!rtpSoundProgram(program)) return;
    markRtpSoundActivity();
    MIDI_UART.sendProgramChange(program, channel);
  });
  rtpMIDI.setHandleSystemExclusive([](byte* data, unsigned size) {
    if (!rtpSoundSysEx(data, size)) return;
    markRtpSoundActivity();
    MIDI_UART.sendSysEx(size, data, true);
  });
  rtpMIDI.setHandleClock([]() { MIDI_UART.sendClock(); });
  rtpMIDI.setHandleStart([]() { MIDI_UART.sendStart(); });
  rtpMIDI.setHandleContinue([]() { MIDI_UART.sendContinue(); });
  rtpMIDI.setHandleStop([]() { MIDI_UART.sendStop(); });
  ApplertpMIDI.setHandleConnected([](const APPLEMIDI_NAMESPACE::ssrc_t&, const char*) {
    rtpConnected = true;
    Serial.println("[RTP] Controller connected to Pico");
    broadcastUnoStatus();
  });
  ApplertpMIDI.setHandleDisconnected([](const APPLEMIDI_NAMESPACE::ssrc_t&) {
    rtpConnected = false;
    Serial.println("[RTP] Controller disconnected from Pico");
    broadcastUnoStatus();
  });
  Serial.println("[RTP] Pico gateway listening on UDP 5004");
}

// Server instances
AsyncWebServer server(80);
WebSocketsServer websocket(8081);
static bool websocketStarted = false;
static const uint8_t MAX_TRACKED_WS_CLIENTS = 8;
static uint32_t wsHeldNotes[MAX_TRACKED_WS_CLIENTS][4] = { 0 };
static uint8_t wsNoteOwners[128] = { 0 };
static bool unoOnline = false;
static uint8_t unoFlags = 0; // bit 0: standalone activity; bit 1: Pico patch applied
static bool unoSyncing = false;
static bool unoSyncFailed = false;
static uint8_t unoSyncRetries = 0;
static uint32_t lastUnoReplyMs = 0;
static uint32_t lastUnoProbeMs = 0;
static uint32_t lastUnoSyncMs = 0;

static bool anyWsNotesHeld() {
  for (uint8_t count : wsNoteOwners) if (count) return true;
  return false;
}

// Returns true when the aggregate MIDI note state actually changes.
static bool trackWsNote(uint8_t client, uint8_t note, bool held) {
  if (client >= MAX_TRACKED_WS_CLIENTS || note >= 128) return false;
  const uint32_t mask = 1UL << (note & 31);
  uint32_t& word = wsHeldNotes[client][note >> 5];
  const bool wasHeld = (word & mask) != 0;
  if (held == wasHeld) return false;
  if (held) {
    word |= mask;
    if (wsNoteOwners[note] < 255) ++wsNoteOwners[note];
    return wsNoteOwners[note] == 1;
  }
  word &= ~mask;
  if (wsNoteOwners[note] > 0) --wsNoteOwners[note];
  return wsNoteOwners[note] == 0;
}

static void releaseWsClientNotes(uint8_t client) {
  if (client >= MAX_TRACKED_WS_CLIENTS) return;
  for (uint8_t note = 0; note < 128; ++note) {
    if ((wsHeldNotes[client][note >> 5] & (1UL << (note & 31))) &&
        trackWsNote(client, note, false))
      MIDI_UART.sendNoteOff(note, 0, 1);
  }
}

static void clearAllWsNotes() {
  memset(wsHeldNotes, 0, sizeof(wsHeldNotes));
  memset(wsNoteOwners, 0, sizeof(wsNoteOwners));
}

// UART to Uno R4 synth
#define synthSerial Serial1
#define LED_PIN LED_BUILTIN 

#ifndef DEBUG_MIDI_IN
#define DEBUG_MIDI_IN 0
#endif

#ifndef DEBUG_WEB_EVENTS
#define DEBUG_WEB_EVENTS 0
#endif

#if DEBUG_WEB_EVENTS
#define WEB_LOGF(...) Serial.printf(__VA_ARGS__)
#define WEB_LOGLN(msg) Serial.println(msg)
#else
#define WEB_LOGF(...) do {} while (0)
#define WEB_LOGLN(msg) do {} while (0)
#endif

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
  .currentPatch = 100,
  .masterVolume = 0.7f,
  .autoSave = true,
  .baudRate = 115200
};

// mDNS
bool mdnsActive = false;
unsigned long lastMdnsCheck = 0;
const unsigned long MDNS_CHECK_INTERVAL = 30000; // Check every 30 seconds


// Patch structure (matches the Uno R4 SynthPatch)
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

  // Lightweight FX
  float bitcrushMix, bitcrushBits, bitcrushRateDiv;
  float tremDepth, tremRateHz;
  float driveAmount, foldAmount;
    
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
    .lfoToMorph=0.25f, .lfoToAmp=0.10f, .lfoToDetune=0.0f,
    .velToCutoff=0.0f, .noiseToCutoff=0.0f,
    .lfo2RateHz=5.0f, .lfo2AmtSemi=0.2f, .lfo2Wave=0,
    .chMix=0.0f, .chDepth=5.0f,
    .bitcrushMix=0.0f, .bitcrushBits=16.0f, .bitcrushRateDiv=1.0f,
    .tremDepth=0.0f, .tremRateHz=4.0f,
    .driveAmount=0.0f, .foldAmount=0.0f,
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
    .bitcrushMix=0.0f, .bitcrushBits=16.0f, .bitcrushRateDiv=1.0f,
    .tremDepth=0.0f, .tremRateHz=4.0f,
    .driveAmount=0.0f, .foldAmount=0.0f,
    .arpMode=0, .arpDiv=2, .arpGate=60,
    .stealMode=1, .velCurve=2,
    .name="Pluck"
  }},
  
  {"Solid Bass", {
    .a=0.004f, .d=0.08f, .s=0.6f, .r=0.12f,
    .morph=0.85f, .cutoff=900.f, .resonance=0.7f, .filterOn=true,
    .glide=0.03f, .detune=2.f, .unison=1,
    .tempoSrc=0, .lfoSync=false, .dlySync=false, .bpmInt=120.0f,
    .masterGain=0.7f, .noiseAmt=0.0f,
    .dlyTime=0.12f, .dlyFb=0.25f, .dlyMix=0.0f,
    .lfoAmtHz=0.f, .lfoRateHz=2.0f,
    .lfoToMorph=0.0f, .lfoToAmp=0.0f, .lfoToDetune=0.0f,
    .velToCutoff=0.4f, .noiseToCutoff=0.0f,
    .lfo2RateHz=5.0f, .lfo2AmtSemi=0.2f, .lfo2Wave=0,
    .chMix=0.0f, .chDepth=5.0f,
    .bitcrushMix=0.0f, .bitcrushBits=16.0f, .bitcrushRateDiv=1.0f,
    .tremDepth=0.0f, .tremRateHz=4.0f,
    .driveAmount=0.0f, .foldAmount=0.0f,
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
    .lfo2RateHz=6.5f, .lfo2AmtSemi=0.3f, .lfo2Wave=0,
    .chMix=0.0f, .chDepth=5.0f,
    .bitcrushMix=0.0f, .bitcrushBits=16.0f, .bitcrushRateDiv=1.0f,
    .tremDepth=0.0f, .tremRateHz=4.0f,
    .driveAmount=0.0f, .foldAmount=0.0f,
    .arpMode=0, .arpDiv=2, .arpGate=60,
    .stealMode=1, .velCurve=1,
    .name="PWM Lead"
  }},
  
  {"EP Keys", {
    .a=0.004f, .d=0.35f, .s=0.35f, .r=0.35f,
    .morph=0.25f, .cutoff=2200.f, .resonance=0.95f, .filterOn=true,
    .glide=0.0f, .detune=2.f, .unison=1,
    .tempoSrc=0, .lfoSync=false, .dlySync=true, .bpmInt=120.0f,
    .masterGain=0.7f, .noiseAmt=0.0f,
    .dlyTime=0.12f, .dlyFb=0.35f, .dlyMix=0.15f,
    .lfoAmtHz=250.f, .lfoRateHz=1.2f,
    .lfoToMorph=0.15f, .lfoToAmp=0.05f, .lfoToDetune=0.0f,
    .velToCutoff=0.6f, .noiseToCutoff=0.0f,
    .lfo2RateHz=5.0f, .lfo2AmtSemi=0.2f, .lfo2Wave=0,
    .chMix=0.15f, .chDepth=8.0f,
    .bitcrushMix=0.0f, .bitcrushBits=16.0f, .bitcrushRateDiv=1.0f,
    .tremDepth=0.0f, .tremRateHz=4.0f,
    .driveAmount=0.0f, .foldAmount=0.0f,
    .arpMode=0, .arpDiv=2, .arpGate=60,
    .stealMode=0, .velCurve=1,
    .name="EP Keys"
  }},
  
  {"Sweep Pad", {
    .a=0.15f, .d=1.0f, .s=0.85f, .r=1.2f,
    .morph=0.40f, .cutoff=1100.f, .resonance=1.15f, .filterOn=true,
    .glide=0.08f, .detune=14.f, .unison=3,
    .tempoSrc=0, .lfoSync=false, .dlySync=true, .bpmInt=120.0f,
    .masterGain=0.7f, .noiseAmt=0.0f,
    .dlyTime=0.16f, .dlyFb=0.45f, .dlyMix=0.22f,
    .lfoAmtHz=1500.f, .lfoRateHz=0.3f,
    .lfoToMorph=0.35f, .lfoToAmp=0.12f, .lfoToDetune=0.05f,
    .velToCutoff=0.0f, .noiseToCutoff=0.0f,
    .lfo2RateHz=5.0f, .lfo2AmtSemi=0.2f, .lfo2Wave=0,
    .chMix=0.25f, .chDepth=12.0f,
    .bitcrushMix=0.0f, .bitcrushBits=16.0f, .bitcrushRateDiv=1.0f,
    .tremDepth=0.0f, .tremRateHz=4.0f,
    .driveAmount=0.0f, .foldAmount=0.0f,
    .arpMode=0, .arpDiv=2, .arpGate=60,
    .stealMode=0, .velCurve=0,
    .name="Sweep Pad"
  }},
  
  {"Noise Perc", {
    .a=0.002f, .d=0.08f, .s=0.10f, .r=0.05f,
    .morph=0.0f, .cutoff=3200.f, .resonance=1.0f, .filterOn=true,
    .glide=0.0f, .detune=2.f, .unison=1,
    .tempoSrc=0, .lfoSync=false, .dlySync=false, .bpmInt=120.0f,
    .masterGain=0.7f, .noiseAmt=0.5f,
    .dlyTime=0.06f, .dlyFb=0.25f, .dlyMix=0.05f,
    .lfoAmtHz=0.f, .lfoRateHz=8.0f,
    .lfoToMorph=0.0f, .lfoToAmp=0.0f, .lfoToDetune=0.0f,
    .velToCutoff=0.8f, .noiseToCutoff=0.0f,
    .lfo2RateHz=5.0f, .lfo2AmtSemi=0.2f, .lfo2Wave=0,
    .chMix=0.0f, .chDepth=5.0f,
    .bitcrushMix=0.0f, .bitcrushBits=16.0f, .bitcrushRateDiv=1.0f,
    .tremDepth=0.0f, .tremRateHz=4.0f,
    .driveAmount=0.0f, .foldAmount=0.0f,
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
    .chMix=0.35f, .chDepth=10.0f,
    .bitcrushMix=0.0f, .bitcrushBits=16.0f, .bitcrushRateDiv=1.0f,
    .tremDepth=0.0f, .tremRateHz=4.0f,
    .driveAmount=0.0f, .foldAmount=0.0f,
    .arpMode=0, .arpDiv=2, .arpGate=60,
    .stealMode=0, .velCurve=1,
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
void broadcastPatchList();
bool loadFactoryPreset(int index, SynthPatch& patch);
void normalizePatch(SynthPatch& patch);
void randomizeCurrentPatch();
void sendPatchToSynth(const SynthPatch& patch);
void sendPatchDataToClient(int index);
bool loadBootPatch();
void sendAllCurrentParameters();
void syncWithUno();
void broadcastUnoStatus();
void pollMidiIn();
void tickUnoLink();
bool startUnoSync(bool explicitRequest);
void sendSpecialCommand(const char* param, float value);
int getParamCC(const char* param);
int paramValueToCC(const char* param, float value);
void handleControlMessage(const char* param, float value);
void sendControlToUno(const char* param, float value);
bool savePatchFromCurrent(int index, const char* name, bool overwrite, String& error);
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

    if (!mdns.addServiceRecord("QuarkWave._apple-midi", 5004, MDNSServiceUDP))
        Serial.println("[setupMDNS] WARNING: RTP-MIDI discovery not advertised");
    
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
        MIDI_UART.sendControlChange(cc, value, 1);
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
    sendCC(16, paramValueToCC("velCurve", (float)currentPatch.velCurve));
    sendCC(17, paramValueToCC("stealMode", (float)currentPatch.stealMode));
    
    // === Arpeggiator Parameters ===
    sendCC(18, paramValueToCC("arpMode", (float)currentPatch.arpMode));  // Arp Mode
    sendCC(19, paramValueToCC("arpDiv", (float)currentPatch.arpDiv));    // Arp Division
    sendCC(20, paramValueToCC("arpGate", (float)currentPatch.arpGate));  // Arp Gate
    
    // === Chorus Parameters ===
    sendCC(21, paramValueToCC("chMix", currentPatch.chMix));  // Chorus Mix

    // === Lightweight FX Parameters ===
    sendCC(29, paramValueToCC("bitcrushMix", currentPatch.bitcrushMix));
    sendCC(30, paramValueToCC("bitcrushBits", currentPatch.bitcrushBits));
    sendCC(31, paramValueToCC("bitcrushRateDiv", currentPatch.bitcrushRateDiv));
    sendCC(77, paramValueToCC("tremDepth", currentPatch.tremDepth));
    sendCC(78, paramValueToCC("tremRateHz", currentPatch.tremRateHz));
    sendCC(79, paramValueToCC("driveAmount", currentPatch.driveAmount));
    sendCC(80, paramValueToCC("foldAmount", currentPatch.foldAmount));
    
    // === LFO2/Vibrato Parameters ===
    sendCC(27, paramValueToCC("lfo2RateHz", currentPatch.lfo2RateHz));       // LFO2 Rate
    sendCC(28, paramValueToCC("lfo2AmtSemi", currentPatch.lfo2AmtSemi));      // LFO2 Amount
    
    // === Special Parameters ===
    
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

// Send non-CC controls as MIDI Program Changes or SysEx.
void sendSpecialCommand(const char* param, float value) {
    WEB_LOGF("[sendSpecialCommand] %s = %.3f\n", param, value);
    
    if (strcmp(param, "tempoSrc") == 0) {
        // PC 100=External, PC 101=Internal
        MIDI_UART.sendProgramChange(value > 0 ? 100 : 101, 1);
        
    } else if (strcmp(param, "dlySync") == 0) {
        // PC 102=Sync On, PC 103=Sync Off
        MIDI_UART.sendProgramChange(value > 0 ? 102 : 103, 1);
        
    } else if (strcmp(param, "bpm") == 0) {
        // Use SysEx for BPM (needs more precision than PC)
        sendBPMSysEx((uint16_t)value);
        
    } else if (strcmp(param, "lfoToMorph") == 0) {
        // Use CC-like SysEx for modulation routing
        sendModRoutingSysEx(0, (uint8_t)roundf(constrain(value, 0.0f, 1.0f) * 127.0f));
        
    } else if (strcmp(param, "lfoToAmp") == 0) {
        sendModRoutingSysEx(1, (uint8_t)roundf(constrain(value, 0.0f, 1.0f) * 127.0f));
        
    } else if (strcmp(param, "chDepth") == 0) {
        // Chorus depth via SysEx
        sendChorusSysEx((uint8_t)roundf(constrain(value, 0.0f, 20.0f)));
        
    } else if (strcmp(param, "lfo2Wave") == 0) {
        // PC 110-112 for LFO2 wave types
        MIDI_UART.sendProgramChange(110 + (uint8_t)constrain((int)roundf(value), 0, 2), 1);
        
    } else {
        WEB_LOGF("[sendSpecialCommand] Unknown parameter: %s\n", param);
    }
    
    delay(2);
}

// Helper functions for SysEx messages
void sendBPMSysEx(uint16_t bpm) {
    bpm = (uint16_t)constrain((int)bpm, 40, 240);
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
    const uint8_t amt7 = static_cast<uint8_t>(amount & 0x7F);
    const uint8_t sysex[] = {
        0xF0, 0x7D, 0x00, 0x02,
        routingType,
        amt7,
        0xF7
    };
    MIDI_UART.sendSysEx(sizeof(sysex), sysex, true);
}

void sendChorusSysEx(uint8_t depth) {
    const uint8_t d7 = static_cast<uint8_t>(depth & 0x7F);
    const uint8_t sysex[] = {
        0xF0, 0x7D, 0x00, 0x03,
        d7,
        0xF7
    };
    MIDI_UART.sendSysEx(sizeof(sysex), sysex, true);
}

void syncWithUno() {
  // Reset the synth to a known baseline before sending the loaded patch.
  MIDI_UART.sendProgramChange(0, 1);  
  delay(100);  // Let Uno reset
  
  sendAllCurrentParameters();
  const uint8_t complete[] = {0xF0, 0x7D, 0x00, 0x12, 0xF7};
  MIDI_UART.sendSysEx(sizeof(complete), complete, true);
}

void broadcastUnoStatus() {
  if (!websocketStarted) return;
  JsonDocument doc;
  doc["type"] = "unoStatus";
  doc["connected"] = unoOnline;
  doc["syncing"] = unoSyncing;
  doc["syncFailed"] = unoSyncFailed;
  doc["rtpConnected"] = rtpConnected;
  doc["mode"] = !unoOnline ? "offline" : unoSyncing ? "syncing"
    : (unoFlags & 0x01) ? "standalone" : (unoFlags & 0x02) ? "pico" : "unsynced";
  String response;
  serializeJson(doc, response);
  websocket.broadcastTXT(response);
}

bool startUnoSync(bool explicitRequest) {
  if (!unoOnline || unoSyncing) return false;
  if (!explicitRequest && ((unoFlags & 0x03) || anyWsNotesHeld() || unoSyncFailed)) return false;
  unoSyncing = true;
  unoSyncFailed = false;
  unoSyncRetries = 0;
  clearAllWsNotes();
  broadcastUnoStatus();
  syncWithUno();
  lastUnoSyncMs = millis();
  return true;
}

void handleUnoLinkMessage(uint8_t command, uint8_t flags) {
  if (command != 0x11 && command != 0x13) return;
  const bool changed = !unoOnline || unoFlags != (flags & 0x03);
  const bool wasSyncing = unoSyncing;
  unoOnline = true;
  unoFlags = flags & 0x03;
  if (!(unoFlags & 0x01)) rtpActivityReported = false;
  if (command == 0x11 && rtpSoundActivity && !(unoFlags & 0x03))
    markRtpSoundActivity();
  lastUnoReplyMs = millis();
  if (command == 0x13 || ((unoFlags & 0x02) && unoSyncing && millis() - lastUnoSyncMs > 250)) {
    unoSyncing = false;
    unoSyncFailed = false;
    unoSyncRetries = 0;
    rtpSoundActivity = false;
  }
  if (changed || command == 0x13 || wasSyncing != unoSyncing) broadcastUnoStatus();
  if (command == 0x11 && !unoSyncing && !unoSyncFailed && !rtpSoundActivity &&
      !(unoFlags & 0x03) && !anyWsNotesHeld())
    startUnoSync(false);
}

void tickUnoLink() {
  const uint32_t now = millis();
  if (lastUnoProbeMs == 0 || now - lastUnoProbeMs >= 1000) {
    lastUnoProbeMs = now;
    const uint8_t hello[] = {0xF0, 0x7D, 0x00, 0x10, 0xF7};
    MIDI_UART.sendSysEx(sizeof(hello), hello, true);
  }
  if (unoOnline && now - lastUnoReplyMs > 3500) {
    unoOnline = false;
    unoFlags = 0;
    unoSyncing = false;
    unoSyncFailed = false;
    broadcastUnoStatus();
  } else if (unoOnline && unoSyncing && now - lastUnoSyncMs > 2000) {
    if (unoSyncRetries == 0) {
      unoSyncRetries = 1;
      syncWithUno();
      lastUnoSyncMs = millis();
    } else {
      unoSyncing = false;
      unoSyncFailed = true;
      broadcastUnoStatus();
    }
  }
}

bool loadBootPatch() {
    int index = config.currentPatch;
    if ((index >= 0 && index < 8 && loadPatchFromFile(index, currentPatch)) ||
        (index >= 100 && index < 100 + NUM_FACTORY_PRESETS &&
         loadFactoryPreset(index - 100, currentPatch))) {
        Serial.printf("[loadBootPatch] Loaded patch %d: '%s'\n", index, currentPatch.name);
        return true;
    }

    loadFactoryPreset(0, currentPatch);
    config.currentPatch = 100;
    saveConfig();
    Serial.printf("[loadBootPatch] Using factory fallback patch: '%s'\n", currentPatch.name);
    return false;
}

void pollMidiIn() {
  // Only QuarkWave link replies travel on the Uno -> Pico UART. Parse their
  // fixed six-byte envelope directly: the generic MIDI library has sometimes
  // reported a valid raw reply as a two-byte SysEx and lost the handshake.
  static uint8_t frame[6];
  static uint8_t count = 0;
  uint8_t drained = 0;
  while (Serial1.available() > 0 && drained++ < 32) {
    const uint8_t value = static_cast<uint8_t>(Serial1.read());
    if (value == 0xF0) {
      frame[0] = value;
      count = 1;
      continue;
    }
    if (value >= 0xF8) continue;  // Real-time MIDI may interrupt SysEx.
    if (count == 0) continue;
    if ((value & 0x80) && value != 0xF7) {
      count = 0;
      continue;
    }
    frame[count++] = value;
    if (value == 0xF7) {
      if (count == sizeof(frame) && frame[1] == 0x7D && frame[2] == 0x00 &&
          (frame[3] == 0x11 || frame[3] == 0x13) && frame[4] < 0x80)
        handleUnoLinkMessage(frame[3], frame[4]);
      count = 0;
    } else if (count == sizeof(frame)) {
      count = 0;  // An overlong or malformed reply cannot change link state.
    }
  }
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
    loadConfig();
    loadBootPatch();
    setupSynthSerial();
    tickUnoLink();
    
    Serial.println("[BOOT] Attempting WiFi connection...");
    setupWiFi();
    
    if (WiFi.status() == WL_CONNECTED) {
        digitalWrite(LED_PIN, HIGH);  // LED ON = WiFi connected
        Serial.println("[BOOT] LED ON - WiFi Connected!");
        
        setupWebServer();
        setupWebSocket();
        setupRtpGateway();
        
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
    Serial.println("[BOOT] System ready!");
}

void loop() {
    if (rtpInitialized && WiFi.status() == WL_CONNECTED) rtpMIDI.read();
    if (mdnsActive) {
        mdns.run();
        checkMDNS();  // Add health monitoring
    }

    if (websocketStarted) websocket.loop();
    
    // Keep the heartbeat nonblocking so it cannot swallow MIDI Clock pulses.
    static unsigned long lastHeartbeat = 0;
    static unsigned long heartbeatOffAt = 0;
    if (WiFi.status() == WL_CONNECTED && millis() - lastHeartbeat > 30000) {
        lastHeartbeat = millis();
        digitalWrite(LED_PIN, LOW);
        heartbeatOffAt = lastHeartbeat;
    }
    if (heartbeatOffAt && millis() - heartbeatOffAt >= 50) {
        digitalWrite(LED_PIN, HIGH);
        heartbeatOffAt = 0;
    }
    
    pollMidiIn();
    tickUnoLink();
    
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
        pollMidiIn();
        tickUnoLink();
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
    MIDI_UART.turnThruOff();  // Return-link status must not echo back to the Uno.
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
            
            const String filename = "/patch_" + String(i) + ".json";
            if (loadPatchFromFile(i, patch)) {
                patchObj["name"] = patch.name;
                patchObj["exists"] = true;
            } else if (LittleFS.exists(filename)) {
                patchObj["name"] = "Unreadable " + String(i);
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
  strncpy(patch.name, FACTORY_PRESETS[index].name, sizeof(patch.name) - 1);
  patch.name[sizeof(patch.name) - 1] = '\0';
  normalizePatch(patch);
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
        config.currentPatch = index;
        saveConfig();
        
        // Send patch data to web client
        sendPatchDataToClient(index);
        
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
    patch["bitcrushMix"] = currentPatch.bitcrushMix;
    patch["bitcrushBits"] = currentPatch.bitcrushBits;
    patch["bitcrushRateDiv"] = currentPatch.bitcrushRateDiv;
    patch["tremDepth"] = currentPatch.tremDepth;
    patch["tremRateHz"] = currentPatch.tremRateHz;
    patch["driveAmount"] = currentPatch.driveAmount;
    patch["foldAmount"] = currentPatch.foldAmount;
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

enum ParamScale : uint8_t {
  SCALE_LINEAR,
  SCALE_BOOL,
  SCALE_UNISON,
  SCALE_VEL_CURVE,
  SCALE_STEAL_MODE
};

struct ParamCCMap {
  const char* name;
  uint8_t cc;
  ParamScale scale;
  float minValue;
  float span;
};

static const ParamCCMap PARAM_CC_MAP[] = {
  {"attack", 73, SCALE_LINEAR, 0.002f, 0.498f},
  {"decay", 75, SCALE_LINEAR, 0.01f, 0.99f},
  {"sustain", 23, SCALE_LINEAR, 0.10f, 0.75f},
  {"sustainPedal", 64, SCALE_BOOL, 0.0f, 1.0f},
  {"release", 72, SCALE_LINEAR, 0.02f, 1.48f},
  {"cutoff", 74, SCALE_LINEAR, 40.0f, 9960.0f},
  {"resonance", 71, SCALE_LINEAR, 0.5f, 2.5f},
  {"morph", 76, SCALE_LINEAR, 0.0f, 1.0f},
  {"filterOn", 91, SCALE_BOOL, 0.0f, 1.0f},
  {"glide", 5, SCALE_LINEAR, 0.0f, 0.3f},
  {"detune", 94, SCALE_LINEAR, 2.0f, 18.0f},
  {"masterGain", 7, SCALE_LINEAR, 0.2f, 0.8f},
  {"noiseAmt", 93, SCALE_LINEAR, 0.0f, 1.0f},
  {"delayTime", 12, SCALE_LINEAR, 0.02f, 0.146f},
  {"delayFeedback", 13, SCALE_LINEAR, 0.01f, 0.88f},
  {"delayMix", 14, SCALE_LINEAR, 0.0f, 1.0f},
  {"lfoAmtHz", 1, SCALE_LINEAR, 0.0f, 3000.0f},
  {"lfoRateHz", 2, SCALE_LINEAR, 0.1f, 12.0f},
  {"lfoSync", 3, SCALE_BOOL, 0.0f, 1.0f},
  {"velToCutoff", 24, SCALE_LINEAR, 0.0f, 1.0f},
  {"noiseToCutoff", 25, SCALE_LINEAR, 0.0f, 1.0f},
  {"lfoToDetune", 26, SCALE_LINEAR, 0.0f, 1.0f},
  {"unison", 95, SCALE_UNISON, 1.0f, 2.0f},
  {"chMix", 21, SCALE_LINEAR, 0.0f, 1.0f},
  {"arpMode", 18, SCALE_LINEAR, 0.0f, 4.0f},
  {"arpDiv", 19, SCALE_LINEAR, 0.0f, 7.0f},
  {"arpGate", 20, SCALE_LINEAR, 5.0f, 90.0f},
  {"lfo2RateHz", 27, SCALE_LINEAR, 0.1f, 20.0f},
  {"lfo2AmtSemi", 28, SCALE_LINEAR, 0.0f, 2.0f},
  {"bitcrushMix", 29, SCALE_LINEAR, 0.0f, 1.0f},
  {"bitcrushBits", 30, SCALE_LINEAR, 4.0f, 12.0f},
  {"bitcrushRateDiv", 31, SCALE_LINEAR, 1.0f, 15.0f},
  {"tremDepth", 77, SCALE_LINEAR, 0.0f, 1.0f},
  {"tremRateHz", 78, SCALE_LINEAR, 0.1f, 12.0f},
  {"driveAmount", 79, SCALE_LINEAR, 0.0f, 1.0f},
  {"foldAmount", 80, SCALE_LINEAR, 0.0f, 1.0f},
  {"velCurve", 16, SCALE_VEL_CURVE, 0.0f, 3.0f},
  {"stealMode", 17, SCALE_STEAL_MODE, 0.0f, 1.0f}
};

static const ParamCCMap* findParamCCMap(const char* param) {
  if (!param) return nullptr;
  for (const auto& item : PARAM_CC_MAP) {
    if (strcmp(param, item.name) == 0) return &item;
  }
  return nullptr;
}

int getParamCC(const char* param) {
  const ParamCCMap* item = findParamCCMap(param);
  return item ? item->cc : -1;
}

int paramValueToCC(const char* param, float value) {
  const ParamCCMap* item = findParamCCMap(param);
  if (!item) return constrain((int)(value * 127), 0, 127);
  if (!isfinite(value)) value = item->minValue;

  switch (item->scale) {
    case SCALE_BOOL:
      return value > 0.5f ? 127 : 0;

    case SCALE_UNISON:
      if (value <= 1.5f) return 0;
      if (value <= 2.5f) return 64;
      return 127;

    case SCALE_VEL_CURVE: {
      static const int lut[4] = {0, 43, 85, 127};
      int v = constrain((int)roundf(value), 0, 3);
      return lut[v];
    }

    case SCALE_STEAL_MODE: {
      static const int lut[2] = {0, 127};
      int v = constrain((int)roundf(value), 0, 1);
      return lut[v];
    }

    case SCALE_LINEAR:
    default:
      return constrain((int)roundf((constrain(value, item->minValue, item->minValue + item->span) - item->minValue) / item->span * 127.0f), 0, 127);
  }
}

// Decode the CC exactly as the Uno receiver does, so Pico patch data reflects sound.
static float ccToEffectiveValue(const ParamCCMap& item, uint8_t cc) {
  const float t = cc / 127.0f;
  switch (item.scale) {
    case SCALE_BOOL: return cc >= 64 ? 1.0f : 0.0f;
    case SCALE_UNISON: return 1.0f + roundf(t * 2.0f);
    case SCALE_VEL_CURVE: return roundf(t * 3.0f);
    case SCALE_STEAL_MODE: return cc >= 64 ? 1.0f : 0.0f;
    case SCALE_LINEAR: break;
  }
  const float value = item.minValue + item.span * t;
  switch (item.cc) {
    case 18: return cc < 26 ? 0 : cc < 51 ? 1 : cc < 77 ? 2 : cc < 102 ? 3 : 4;
    case 19: case 20: case 30: case 31: return roundf(value);
    default: return value;
  }
}

static float effectiveControlValue(const char* param, float value) {
  if (!isfinite(value)) value = 0;
  if (const ParamCCMap* item = findParamCCMap(param))
    return ccToEffectiveValue(*item, (uint8_t)paramValueToCC(param, value));
  if (strcmp(param, "bpm") == 0) return roundf(constrain(value, 40.0f, 240.0f));
  if (strcmp(param, "chDepth") == 0) return roundf(constrain(value, 0.0f, 20.0f));
  if (strcmp(param, "lfo2Wave") == 0) return roundf(constrain(value, 0.0f, 2.0f));
  if (strcmp(param, "tempoSrc") == 0 || strcmp(param, "dlySync") == 0)
    return value > 0.5f ? 1.0f : 0.0f;
  if (strcmp(param, "lfoToMorph") == 0 || strcmp(param, "lfoToAmp") == 0)
    return roundf(constrain(value, 0.0f, 1.0f) * 127.0f) / 127.0f;
  return value;
}

void normalizePatch(SynthPatch& p) {
#define NORMALIZE(field, name) p.field = effectiveControlValue(name, p.field)
  NORMALIZE(a, "attack"); NORMALIZE(d, "decay"); NORMALIZE(s, "sustain"); NORMALIZE(r, "release");
  NORMALIZE(morph, "morph"); NORMALIZE(cutoff, "cutoff"); NORMALIZE(resonance, "resonance");
  NORMALIZE(filterOn, "filterOn"); NORMALIZE(glide, "glide"); NORMALIZE(detune, "detune");
  NORMALIZE(unison, "unison"); NORMALIZE(tempoSrc, "tempoSrc"); NORMALIZE(lfoSync, "lfoSync");
  NORMALIZE(dlySync, "dlySync"); NORMALIZE(bpmInt, "bpm"); NORMALIZE(masterGain, "masterGain");
  NORMALIZE(noiseAmt, "noiseAmt"); NORMALIZE(dlyTime, "delayTime"); NORMALIZE(dlyFb, "delayFeedback");
  NORMALIZE(dlyMix, "delayMix"); NORMALIZE(lfoAmtHz, "lfoAmtHz"); NORMALIZE(lfoRateHz, "lfoRateHz");
  NORMALIZE(lfoToMorph, "lfoToMorph"); NORMALIZE(lfoToAmp, "lfoToAmp");
  NORMALIZE(lfoToDetune, "lfoToDetune"); NORMALIZE(velToCutoff, "velToCutoff");
  NORMALIZE(noiseToCutoff, "noiseToCutoff"); NORMALIZE(lfo2RateHz, "lfo2RateHz");
  NORMALIZE(lfo2AmtSemi, "lfo2AmtSemi"); NORMALIZE(lfo2Wave, "lfo2Wave");
  NORMALIZE(chMix, "chMix"); NORMALIZE(chDepth, "chDepth");
  NORMALIZE(bitcrushMix, "bitcrushMix"); NORMALIZE(bitcrushBits, "bitcrushBits");
  NORMALIZE(bitcrushRateDiv, "bitcrushRateDiv"); NORMALIZE(tremDepth, "tremDepth");
  NORMALIZE(tremRateHz, "tremRateHz"); NORMALIZE(driveAmount, "driveAmount");
  NORMALIZE(foldAmount, "foldAmount"); NORMALIZE(arpMode, "arpMode");
  NORMALIZE(arpDiv, "arpDiv"); NORMALIZE(arpGate, "arpGate");
  NORMALIZE(stealMode, "stealMode"); NORMALIZE(velCurve, "velCurve");
#undef NORMALIZE
}

static float randomMappedValue(const char* param) {
  const ParamCCMap* item = findParamCCMap(param);
  return item ? ccToEffectiveValue(*item, (uint8_t)random(0, 128)) : 0.0f;
}

void randomizeCurrentPatch() {
  randomSeed(micros() ^ (uint32_t)millis());
#define RANDOM_CC(field, name) currentPatch.field = randomMappedValue(name)
  RANDOM_CC(a, "attack"); RANDOM_CC(d, "decay"); RANDOM_CC(s, "sustain"); RANDOM_CC(r, "release");
  RANDOM_CC(morph, "morph"); RANDOM_CC(cutoff, "cutoff"); RANDOM_CC(resonance, "resonance");
  RANDOM_CC(filterOn, "filterOn"); RANDOM_CC(glide, "glide"); RANDOM_CC(detune, "detune");
  RANDOM_CC(unison, "unison"); RANDOM_CC(lfoSync, "lfoSync");
  RANDOM_CC(masterGain, "masterGain"); RANDOM_CC(noiseAmt, "noiseAmt");
  RANDOM_CC(dlyTime, "delayTime"); RANDOM_CC(dlyFb, "delayFeedback"); RANDOM_CC(dlyMix, "delayMix");
  RANDOM_CC(lfoAmtHz, "lfoAmtHz"); RANDOM_CC(lfoRateHz, "lfoRateHz");
  RANDOM_CC(lfoToDetune, "lfoToDetune"); RANDOM_CC(velToCutoff, "velToCutoff");
  RANDOM_CC(noiseToCutoff, "noiseToCutoff"); RANDOM_CC(lfo2RateHz, "lfo2RateHz");
  RANDOM_CC(lfo2AmtSemi, "lfo2AmtSemi"); RANDOM_CC(chMix, "chMix");
  RANDOM_CC(bitcrushMix, "bitcrushMix"); RANDOM_CC(bitcrushBits, "bitcrushBits");
  RANDOM_CC(bitcrushRateDiv, "bitcrushRateDiv"); RANDOM_CC(tremDepth, "tremDepth");
  RANDOM_CC(tremRateHz, "tremRateHz"); RANDOM_CC(driveAmount, "driveAmount");
  RANDOM_CC(foldAmount, "foldAmount"); RANDOM_CC(arpMode, "arpMode");
  RANDOM_CC(arpDiv, "arpDiv"); RANDOM_CC(arpGate, "arpGate");
  RANDOM_CC(stealMode, "stealMode"); RANDOM_CC(velCurve, "velCurve");
#undef RANDOM_CC
  currentPatch.tempoSrc = (uint8_t)random(0, 2);
  currentPatch.dlySync = random(0, 2) != 0;
  currentPatch.bpmInt = (float)random(40, 241);
  currentPatch.lfoToMorph = random(0, 128) / 127.0f;
  currentPatch.lfoToAmp = random(0, 128) / 127.0f;
  currentPatch.lfo2Wave = (uint8_t)random(0, 3);
  currentPatch.chDepth = (float)random(0, 21);
  normalizePatch(currentPatch);
  sendPatchToSynth(currentPatch);
  sendPatchDataToClient(config.currentPatch);
}

void handleControlMessage(const char* param, float value) {
    if (!param) return;
    value = effectiveControlValue(param, value);

    // Update current patch data
    if (strcmp(param, "attack") == 0) currentPatch.a = value;
    else if (strcmp(param, "decay") == 0) currentPatch.d = value;
    else if (strcmp(param, "sustain") == 0) currentPatch.s = value;
    else if (strcmp(param, "release") == 0) currentPatch.r = value;
    else if (strcmp(param, "cutoff") == 0) currentPatch.cutoff = value;
    else if (strcmp(param, "resonance") == 0) currentPatch.resonance = value;
    else if (strcmp(param, "morph") == 0) currentPatch.morph = value;
    else if (strcmp(param, "filterOn") == 0) currentPatch.filterOn = (value > 0.5);
    else if (strcmp(param, "glide") == 0) currentPatch.glide = value;
    else if (strcmp(param, "detune") == 0) currentPatch.detune = value;
    else if (strcmp(param, "unison") == 0) currentPatch.unison = (uint8_t)value;
    else if (strcmp(param, "delayTime") == 0) currentPatch.dlyTime = value;
    else if (strcmp(param, "delayFeedback") == 0) currentPatch.dlyFb = value;
    else if (strcmp(param, "delayMix") == 0) currentPatch.dlyMix = value;
    else if (strcmp(param, "noiseAmt") == 0) currentPatch.noiseAmt = value;
    else if (strcmp(param, "masterGain") == 0) currentPatch.masterGain = value;
    else if (strcmp(param, "lfoAmtHz") == 0) currentPatch.lfoAmtHz = value;
    else if (strcmp(param, "lfoRateHz") == 0) currentPatch.lfoRateHz = value;
    else if (strcmp(param, "lfoSync") == 0) currentPatch.lfoSync = (value > 0.5);
    else if (strcmp(param, "dlySync") == 0) currentPatch.dlySync = (value > 0.5);
    else if (strcmp(param, "tempoSrc") == 0) currentPatch.tempoSrc = (uint8_t)value;
    else if (strcmp(param, "bpm") == 0) currentPatch.bpmInt = constrain(value, 40.0f, 240.0f);
    else if (strcmp(param, "lfoToMorph") == 0) currentPatch.lfoToMorph = value;
    else if (strcmp(param, "lfoToAmp") == 0) currentPatch.lfoToAmp = value;  
    else if (strcmp(param, "lfoToDetune") == 0) currentPatch.lfoToDetune = value;
    else if (strcmp(param, "chMix") == 0) currentPatch.chMix = value;
    else if (strcmp(param, "chDepth") == 0) currentPatch.chDepth = value;
    else if (strcmp(param, "bitcrushMix") == 0) currentPatch.bitcrushMix = value;
    else if (strcmp(param, "bitcrushBits") == 0) currentPatch.bitcrushBits = value;
    else if (strcmp(param, "bitcrushRateDiv") == 0) currentPatch.bitcrushRateDiv = value;
    else if (strcmp(param, "tremDepth") == 0) currentPatch.tremDepth = value;
    else if (strcmp(param, "tremRateHz") == 0) currentPatch.tremRateHz = value;
    else if (strcmp(param, "driveAmount") == 0) currentPatch.driveAmount = value;
    else if (strcmp(param, "foldAmount") == 0) currentPatch.foldAmount = value;
    else if (strcmp(param, "arpMode") == 0) currentPatch.arpMode = (uint8_t)value;
    else if (strcmp(param, "arpDiv") == 0) currentPatch.arpDiv = (uint8_t)value;
    else if (strcmp(param, "arpGate") == 0) currentPatch.arpGate = (uint8_t)value;
    else if (strcmp(param, "lfo2RateHz") == 0) currentPatch.lfo2RateHz = value;
    else if (strcmp(param, "lfo2AmtSemi") == 0) currentPatch.lfo2AmtSemi = value;
    else if (strcmp(param, "lfo2Wave") == 0) currentPatch.lfo2Wave = (uint8_t)value;
    else if (strcmp(param, "stealMode") == 0) currentPatch.stealMode = (uint8_t)value;
    else if (strcmp(param, "velCurve") == 0) currentPatch.velCurve = (uint8_t)value;
    else if (strcmp(param, "velToCutoff") == 0) currentPatch.velToCutoff = value;
    else if (strcmp(param, "noiseToCutoff") == 0) currentPatch.noiseToCutoff = value;
    else if (strcmp(param, "sustainPedal") == 0) {
        // Performance state only; do not store pedal state in the patch.
    }
    
    // Send to Uno R4 over MIDI.
    sendControlToUno(param, value);
}

// Send mapped controls as MIDI CC, with special handling for non-CC params.
void sendControlToUno(const char* param, float value) {
    int ccNum = getParamCC(param);
    if (ccNum >= 0) {
        int ccVal = paramValueToCC(param, value);
        MIDI_UART.sendControlChange(ccNum, ccVal, 1);
        WEB_LOGF("-> Uno: %s = %.3f (CC%d=%d)\n", param, value, ccNum, ccVal);
    } else {
        // Handle special parameters via SysEx or additional CC assignments
        sendSpecialCommand(param, value);
    }
}

bool savePatchFromCurrent(int index, const char* name, bool overwrite, String& error) {
    if (index < 0 || index >= 8) {
        error = "Factory presets are read-only; choose a user slot.";
        return false;
    }
    if (!name || !name[0]) {
        error = "Enter a patch name before saving.";
        return false;
    }
    const String filename = "/patch_" + String(index) + ".json";
    if (LittleFS.exists(filename) && !overwrite) {
        error = "That user slot is occupied. Confirm overwrite and try again.";
        return false;
    }

    SynthPatch candidate = currentPatch;
    normalizePatch(candidate);
    strncpy(candidate.name, name, sizeof(candidate.name) - 1);
    candidate.name[sizeof(candidate.name) - 1] = '\0';
    if (!savePatchToFile(index, candidate)) {
        error = "Could not write the user patch. Existing patches were kept.";
        return false;
    }
    currentPatch = candidate;
    config.currentPatch = (uint8_t)index;
    saveConfig();
    broadcastPatchList();
    sendPatchDataToClient(index);
    Serial.printf("Saved patch %d: %s\n", index, currentPatch.name);
    return true;
}

void saveCurrentAsCommit() {
    // Primary commit write using your canonical serializer
    bool ok = savePatchToFile(-1, currentPatch);
    if (!ok) {
        Serial.println("[saveCurrentAsCommit] ERROR: savePatchToFile(-1, ...) failed");
        return;
    }

    // Optional: also write a backup file
    const char* bak = "/commit_patch.bak";
    if (LittleFS.exists(bak)) {
        if (!LittleFS.remove(bak)) {
            Serial.println("[saveCurrentAsCommit] WARN: couldn't remove old backup");
        }
    }
    // Serialize the same JSON layout used by savePatchToFile()
    {
        File b = LittleFS.open(bak, "w");
        if (b) {
            JsonDocument doc;
            doc["version"] = 1;
            doc["name"] = currentPatch.name;

            JsonObject env    = doc["envelope"].to<JsonObject>();
            JsonObject filter = doc["filter"].to<JsonObject>();
            JsonObject tempo  = doc["tempo"].to<JsonObject>();
            JsonObject mix    = doc["mix"].to<JsonObject>();
            JsonObject delay  = doc["delay"].to<JsonObject>();
            JsonObject lfo    = doc["lfo"].to<JsonObject>();
            JsonObject perf   = doc["performance"].to<JsonObject>();
            JsonObject lfo2   = doc["lfo2"].to<JsonObject>();
            JsonObject chorus = doc["chorus"].to<JsonObject>();
            JsonObject fx     = doc["fx"].to<JsonObject>();
            JsonObject arp    = doc["arp"].to<JsonObject>();
            JsonObject voice  = doc["voice"].to<JsonObject>();

            env["attack"] = currentPatch.a; env["decay"] = currentPatch.d;
            env["sustain"] = currentPatch.s; env["release"] = currentPatch.r;
            doc["morph"]   = currentPatch.morph;
            filter["cutoff"] = currentPatch.cutoff; filter["resonance"] = currentPatch.resonance;
            filter["enabled"] = currentPatch.filterOn;
            doc["glide"] = currentPatch.glide; doc["detune"] = currentPatch.detune; doc["unison"] = currentPatch.unison;

            tempo["source"] = currentPatch.tempoSrc; tempo["bpm"] = currentPatch.bpmInt;
            tempo["lfoSync"] = currentPatch.lfoSync; tempo["delaySync"] = currentPatch.dlySync;

            mix["masterGain"] = currentPatch.masterGain; mix["noiseAmount"] = currentPatch.noiseAmt;

            delay["time"] = currentPatch.dlyTime; delay["feedback"] = currentPatch.dlyFb; delay["mix"] = currentPatch.dlyMix;

            lfo["amount"] = currentPatch.lfoAmtHz; lfo["rate"] = currentPatch.lfoRateHz;
            lfo["toMorph"] = currentPatch.lfoToMorph; lfo["toAmp"] = currentPatch.lfoToAmp; lfo["toDetune"] = currentPatch.lfoToDetune;

            perf["velToCutoff"] = currentPatch.velToCutoff; perf["noiseToCutoff"] = currentPatch.noiseToCutoff;

            lfo2["rate"] = currentPatch.lfo2RateHz; lfo2["amount"] = currentPatch.lfo2AmtSemi; lfo2["wave"] = currentPatch.lfo2Wave;

            chorus["mix"] = currentPatch.chMix; chorus["depth"] = currentPatch.chDepth;

            fx["bitcrushMix"] = currentPatch.bitcrushMix; fx["bitcrushBits"] = currentPatch.bitcrushBits; fx["bitcrushRateDiv"] = currentPatch.bitcrushRateDiv;
            fx["tremDepth"] = currentPatch.tremDepth; fx["tremRateHz"] = currentPatch.tremRateHz;
            fx["driveAmount"] = currentPatch.driveAmount; fx["foldAmount"] = currentPatch.foldAmount;

            arp["mode"] = currentPatch.arpMode; arp["division"] = currentPatch.arpDiv; arp["gate"] = currentPatch.arpGate;

            voice["stealMode"] = currentPatch.stealMode; voice["velCurve"]  = currentPatch.velCurve;

            size_t w = serializeJson(doc, b);
            b.flush(); b.close();
            if (w == 0) {
                Serial.println("[saveCurrentAsCommit] WARN: backup serialize wrote 0 bytes");
            }
        } else {
            Serial.println("[saveCurrentAsCommit] WARN: couldn't open backup for write");
        }
    }

    // WS notify UI
    {
        JsonDocument doc;
        doc["type"] = "commitSaved";
        doc["name"] = currentPatch.name;
        String msg;
        serializeJson(doc, msg);
        websocket.broadcastTXT(msg);
    }

    // Visual confirmation
    Serial.println("[saveCurrentAsCommit] Current patch committed");
    ledBlink(1, 120);
}

void loadCommitPatch() {
    SynthPatch commitPatch;
    bool ok = loadPatchFromFile(-1, commitPatch);

    if (!ok) {
        Serial.println("[loadCommitPatch] commit_patch.json missing or corrupt; trying backup");
        // Try backup file directly
        const char* bak = "/commit_patch.bak";
        if (LittleFS.exists(bak)) {
            File f = LittleFS.open(bak, "r");
            if (f) {
                String content = f.readString();
                f.close();
                JsonDocument doc;
                DeserializationError err = deserializeJson(doc, content);
                if (!err) {
                    // Reconstruct SynthPatch from backup JSON (same layout as save)
                    const char* nameFromFile = doc["name"];
                    if (nameFromFile) {
                        strncpy(commitPatch.name, nameFromFile, 16);
                        commitPatch.name[16] = '\0';
                    } else {
                        strncpy(commitPatch.name, "Commit", 16);
                        commitPatch.name[16] = '\0';
                    }

                    commitPatch.a = doc["envelope"]["attack"] | 0.08f;
                    commitPatch.d = doc["envelope"]["decay"] | 0.7f;
                    commitPatch.s = doc["envelope"]["sustain"] | 0.85f;
                    commitPatch.r = doc["envelope"]["release"] | 0.8f;

                    commitPatch.morph = doc["morph"] | 0.35f;

                    commitPatch.cutoff   = doc["filter"]["cutoff"] | 1400.f;
                    commitPatch.resonance= doc["filter"]["resonance"] | 0.8f;
                    commitPatch.filterOn = doc["filter"]["enabled"] | true;

                    commitPatch.glide = doc["glide"] | 0.06f;
                    commitPatch.detune = doc["detune"] | 12.f;
                    commitPatch.unison = doc["unison"] | 1;

                    commitPatch.tempoSrc = doc["tempo"]["source"] | 0;
                    commitPatch.bpmInt   = doc["tempo"]["bpm"] | 120.0f;
                    commitPatch.lfoSync  = doc["tempo"]["lfoSync"] | false;
                    commitPatch.dlySync  = doc["tempo"]["delaySync"] | true;

                    commitPatch.masterGain = doc["mix"]["masterGain"] | 0.7f;
                    commitPatch.noiseAmt   = doc["mix"]["noiseAmount"] | 0.0f;

                    commitPatch.dlyTime = doc["delay"]["time"] | 0.14f;
                    commitPatch.dlyFb   = doc["delay"]["feedback"] | 0.35f;
                    commitPatch.dlyMix  = doc["delay"]["mix"] | 0.12f;

                    commitPatch.lfoAmtHz     = doc["lfo"]["amount"] | 350.f;
                    commitPatch.lfoRateHz    = doc["lfo"]["rate"] | 0.6f;
                    commitPatch.lfoToMorph   = doc["lfo"]["toMorph"] | 0.25f;
                    commitPatch.lfoToAmp     = doc["lfo"]["toAmp"] | 0.10f;
                    commitPatch.lfoToDetune  = doc["lfo"]["toDetune"] | 0.0f;

                    commitPatch.velToCutoff   = doc["performance"]["velToCutoff"] | 0.0f;
                    commitPatch.noiseToCutoff = doc["performance"]["noiseToCutoff"] | 0.0f;

                    commitPatch.lfo2RateHz = doc["lfo2"]["rate"] | 5.0f;
                    commitPatch.lfo2AmtSemi= doc["lfo2"]["amount"] | 0.2f;
                    commitPatch.lfo2Wave   = doc["lfo2"]["wave"] | 0;

                    commitPatch.chMix   = doc["chorus"]["mix"] | 0.0f;
                    commitPatch.chDepth = doc["chorus"]["depth"] | 5.0f;

                    commitPatch.bitcrushMix = doc["fx"]["bitcrushMix"] | 0.0f;
                    commitPatch.bitcrushBits = doc["fx"]["bitcrushBits"] | 16.0f;
                    commitPatch.bitcrushRateDiv = doc["fx"]["bitcrushRateDiv"] | 1.0f;
                    commitPatch.tremDepth = doc["fx"]["tremDepth"] | 0.0f;
                    commitPatch.tremRateHz = doc["fx"]["tremRateHz"] | 4.0f;
                    commitPatch.driveAmount = doc["fx"]["driveAmount"] | 0.0f;
                    commitPatch.foldAmount = doc["fx"]["foldAmount"] | 0.0f;

                    commitPatch.arpMode = doc["arp"]["mode"] | 0;
                    commitPatch.arpDiv  = doc["arp"]["division"] | 2;
                    commitPatch.arpGate = doc["arp"]["gate"] | 60;

                    commitPatch.stealMode = doc["voice"]["stealMode"] | 0;
                    commitPatch.velCurve  = doc["voice"]["velCurve"] | 0;

                    ok = true;
                } else {
                    Serial.printf("[loadCommitPatch] Backup parse error: %s\n", err.c_str());
                    ok = false;
                }
            }
        }
    }

    if (!ok) {
        Serial.println("[loadCommitPatch] ERROR: No valid commit patch found");
        return;
    }

    // Apply to engine & UI
    normalizePatch(commitPatch);
    currentPatch = commitPatch;
    sendPatchToSynth(currentPatch);
    sendPatchDataToClient(config.currentPatch);

    // WS notify UI that we loaded commit
    {
        JsonDocument doc;
        doc["type"] = "commitLoaded";
        doc["name"] = currentPatch.name;
        String msg;
        serializeJson(doc, msg);
        websocket.broadcastTXT(msg);
    }

    Serial.println("[loadCommitPatch] Committed patch loaded");
    ledBlink(2, 80);   // quick double-blink
}


void setupWebSocket() {
    websocket.begin();
    websocketStarted = true;
    websocket.onEvent([](uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
        switch (type) {
            case WStype_CONNECTED:
                WEB_LOGF("[setupWebSocket] WS[%u] Connected from %s\n", num, websocket.remoteIP(num).toString().c_str());
                releaseWsClientNotes(num);
                // Send patch list immediately on connection
                broadcastPatchList();
                // Also send current patch data
                sendPatchDataToClient(config.currentPatch);
                broadcastUnoStatus();
                break;
                
            case WStype_DISCONNECTED:
                WEB_LOGF("[setupWebSocket] WS[%u] Disconnected\n", num);
                releaseWsClientNotes(num);
                break;
                
            case WStype_TEXT: {
                JsonDocument doc;
                DeserializationError error = deserializeJson(doc, payload, length);
                
                if (error) {
                    Serial.printf("[WebSocket] JSON parse error: %s\n", error.c_str());
                    return;
                }
                
                const char* msgType = doc["type"] | doc["t"] | ""; // Support both 'type' and 't'
                
                if (strcmp(msgType, "randomize") == 0) {
                    randomizeCurrentPatch();
                } else if (strcmp(msgType, "control") == 0) {
                    const char* param = doc["param"] | "";
                    JsonVariant controlValue = doc["value"];
                    float value = controlValue.is<bool>()
                        ? (controlValue.as<bool>() ? 1.0f : 0.0f)
                        : (controlValue | 0.0f);
                    handleControlMessage(param, value);
                    
                } else if (strcmp(msgType, "noteOn") == 0) {
                    int noteValue = doc["note"] | -1;
                    int velocityValue = doc["velocity"] | 100;
                    if (noteValue >= 0 && noteValue <= 127) {
                        uint8_t note = (uint8_t)noteValue;
                        uint8_t velocity = (uint8_t)constrain(velocityValue, 1, 127);
                        if (trackWsNote(num, note, true)) MIDI_UART.sendNoteOn(note, velocity, 1);
                        WEB_LOGF("[WebSocket] Note On: %d vel %d\n", note, velocity);
                    }
                    
                } else if (strcmp(msgType, "noteOff") == 0) {
                    int noteValue = doc["note"] | -1;
                    if (noteValue >= 0 && noteValue <= 127) {
                        uint8_t note = (uint8_t)noteValue;
                        if (trackWsNote(num, note, false)) MIDI_UART.sendNoteOff(note, 0, 1);
                        WEB_LOGF("[WebSocket] Note Off: %d\n", note);
                    }
                } else if (strcmp(msgType, "loadPatch") == 0) {
                    int index = doc["index"];
                    WEB_LOGF("[WebSocket] Loading patch %d\n", index);
                    loadAndSendPatch(index);
                    
                } else if (strcmp(msgType, "savePatch") == 0) {
                    int index = doc["index"] | -1;
                    const char* name = doc["name"] | "";
                    bool overwrite = doc["overwrite"] | false;
                    WEB_LOGF("[WebSocket] Saving patch %d as '%s'\n", index, name);
                    String error;
                    bool saved = savePatchFromCurrent(index, name, overwrite, error);
                    JsonDocument result;
                    result["type"] = "patchSaveResult";
                    result["success"] = saved;
                    result["index"] = index;
                    if (!saved) result["error"] = error;
                    String response;
                    serializeJson(result, response);
                    websocket.sendTXT(num, response);
                    
                } else if (strcmp(msgType, "getPatchList") == 0) {
                    WEB_LOGLN("[WebSocket] Patch list requested");
                    broadcastPatchList();

                } else if (strcmp(msgType, "syncUno") == 0) {
                    startUnoSync(true);
                    
                } else if (strcmp(msgType, "panic") == 0) {
                    MIDI_UART.sendControlChange(120, 127, 1);  // All Sound Off
                    clearAllWsNotes();
                    Serial.println("[WebSocket] Panic sent to Uno R4");
                    
                } else if (strcmp(msgType, "commitPatch") == 0) {
                    saveCurrentAsCommit();
                    
                } else if (strcmp(msgType, "loadCommitPatch") == 0) {
                    loadCommitPatch();
                    
                } else if (strcmp(msgType, "visualization") == 0 || strcmp(msgType, "viz") == 0) {
                    int mode = doc["mode"] | doc["v"];
                    if (mode >= 0 && mode <= 2) {
                        MIDI_UART.sendProgramChange(mode + 10, 1);
                        WEB_LOGF("[WebSocket] Visualization mode %d sent to Uno R4\n", mode);
                    }
                    
                } else {
                    WEB_LOGF("[WebSocket] Unknown message type: %s\n", msgType);
                }
                break;
            }
            default:
                break;
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
    config.currentPatch = doc["currentPatch"] | 100;
    config.masterVolume = doc["masterVolume"] | 0.7f;
    config.autoSave = doc["autoSave"] | true;
    config.baudRate = doc["baudRate"] | 115200;
    
    Serial.println("Config loaded");
    return true;
}

bool savePatchToFile(int slot, const SynthPatch& patch) {
    // --- resolve filename ---
    String filename;
    if (slot == -1) {
        filename = "/commit_patch.json";
    } else {
        if (slot < 0 || slot >= 8) return false;
        filename = "/patch_" + String(slot) + ".json";
    }

    // --- filesystem usage (RP2040: use FSInfo via LittleFS.info) ---
    FSInfo fs_info;
    size_t totalBytes = 0, usedBytes = 0;
    if (LittleFS.info(fs_info)) {
        totalBytes = fs_info.totalBytes;
        usedBytes  = fs_info.usedBytes;
    }
    Serial.printf("[savePatchToFile] FS usage: %u / %u\n", (unsigned)usedBytes, (unsigned)totalBytes);

    // Stage the replacement first. LittleFS.rename atomically replaces the old file.
    const String stagedFilename = filename + ".tmp";
    File file = LittleFS.open(stagedFilename, "w");
    if (!file) {
        Serial.printf("[savePatchToFile] OPEN FAILED for %s\n", stagedFilename.c_str());
        return false;
    }

    // Build JSON
    JsonDocument doc;   // ArduinoJson v7 heap doc
    doc["version"] = 1;
    doc["name"] = patch.name;

    JsonObject env    = doc["envelope"].to<JsonObject>();
    JsonObject filter = doc["filter"].to<JsonObject>();
    JsonObject tempo  = doc["tempo"].to<JsonObject>();
    JsonObject mix    = doc["mix"].to<JsonObject>();
    JsonObject delay  = doc["delay"].to<JsonObject>();
    JsonObject lfo    = doc["lfo"].to<JsonObject>();
    JsonObject perf   = doc["performance"].to<JsonObject>();
    JsonObject lfo2   = doc["lfo2"].to<JsonObject>();
    JsonObject chorus = doc["chorus"].to<JsonObject>();
    JsonObject fx     = doc["fx"].to<JsonObject>();
    JsonObject arp    = doc["arp"].to<JsonObject>();
    JsonObject voice  = doc["voice"].to<JsonObject>();

    if (env.isNull() || filter.isNull() || tempo.isNull() || mix.isNull() ||
        delay.isNull() || lfo.isNull() || perf.isNull() || lfo2.isNull() ||
        chorus.isNull() || fx.isNull() || arp.isNull() || voice.isNull()) {
        Serial.println("[savePatchToFile] OOM creating one or more sub-objects");
        file.close();
        LittleFS.remove(stagedFilename);
        return false;
    }

    env["attack"] = patch.a; env["decay"] = patch.d; env["sustain"] = patch.s; env["release"] = patch.r;
    doc["morph"]  = patch.morph;
    filter["cutoff"] = patch.cutoff; filter["resonance"] = patch.resonance; filter["enabled"] = patch.filterOn;
    doc["glide"] = patch.glide; doc["detune"] = patch.detune; doc["unison"] = patch.unison;
    tempo["source"] = patch.tempoSrc; tempo["bpm"] = patch.bpmInt; tempo["lfoSync"] = patch.lfoSync; tempo["delaySync"] = patch.dlySync;
    mix["masterGain"] = patch.masterGain; mix["noiseAmount"] = patch.noiseAmt;
    delay["time"] = patch.dlyTime; delay["feedback"] = patch.dlyFb; delay["mix"] = patch.dlyMix;
    lfo["amount"] = patch.lfoAmtHz; lfo["rate"] = patch.lfoRateHz;
    lfo["toMorph"] = patch.lfoToMorph; lfo["toAmp"] = patch.lfoToAmp; lfo["toDetune"] = patch.lfoToDetune;
    perf["velToCutoff"] = patch.velToCutoff; perf["noiseToCutoff"] = patch.noiseToCutoff;
    lfo2["rate"] = patch.lfo2RateHz; lfo2["amount"] = patch.lfo2AmtSemi; lfo2["wave"] = patch.lfo2Wave;
    chorus["mix"] = patch.chMix; chorus["depth"] = patch.chDepth;
    fx["bitcrushMix"] = patch.bitcrushMix; fx["bitcrushBits"] = patch.bitcrushBits; fx["bitcrushRateDiv"] = patch.bitcrushRateDiv;
    fx["tremDepth"] = patch.tremDepth; fx["tremRateHz"] = patch.tremRateHz;
    fx["driveAmount"] = patch.driveAmount; fx["foldAmount"] = patch.foldAmount;
    arp["mode"] = patch.arpMode; arp["division"] = patch.arpDiv; arp["gate"] = patch.arpGate;
    voice["stealMode"] = patch.stealMode; voice["velCurve"]  = patch.velCurve;

    // Serialize + verify
    size_t written = serializeJson(doc, file);
    file.flush();
    file.close();

    if (written == 0) {
        Serial.printf("[savePatchToFile] serializeJson wrote 0 bytes for %s\n", filename.c_str());
        LittleFS.remove(stagedFilename);
        return false;
    }

    if (!LittleFS.exists(stagedFilename)) {
        Serial.printf("[savePatchToFile] ERROR: staged file missing for %s\n", filename.c_str());
        return false;
    }
    File verify = LittleFS.open(stagedFilename, "r");
    if (!verify) {
        Serial.printf("[savePatchToFile] ERROR: could not reopen staged %s\n", filename.c_str());
        LittleFS.remove(stagedFilename);
        return false;
    }
    size_t sz = verify.size();
    JsonDocument check;
    const bool valid = sz == written && !deserializeJson(check, verify);
    verify.close();
    if (!valid) {
        LittleFS.remove(stagedFilename);
        return false;
    }
    if (!LittleFS.rename(stagedFilename, filename)) {
        LittleFS.remove(stagedFilename);
        return false;
    }
    Serial.printf("[savePatchToFile] Saved %s (%u bytes)\n", filename.c_str(), (unsigned)sz);
    return true;
}

void debugFileSystem() {
    Serial.println("[debugFileSystem] === File System Debug ===");

    // RP2040 LittleFS capacity/usage
    FSInfo fs_info;
    if (LittleFS.info(fs_info)) {
        size_t total = fs_info.totalBytes;
        size_t used  = fs_info.usedBytes;
        float pct = (total > 0) ? (100.0f * (float)used / (float)total) : 0.0f;
        Serial.printf("[debugFileSystem] Total: %u bytes, Used: %u bytes (%.1f%%)\n",
                      (unsigned)total, (unsigned)used, pct);
    } else {
        Serial.println("[debugFileSystem] LittleFS.info failed");
    }

    // Check config file
    if (LittleFS.exists("/config.json")) {
        File file = LittleFS.open("/config.json", "r");
        if (file) {
            Serial.printf("[debugFileSystem] config.json size: %u bytes\n", (unsigned)file.size());
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
                Serial.printf("[debugFileSystem] %s exists, size: %u bytes\n",
                              filename.c_str(), (unsigned)fileSize);
                file.close();

                // Validate parse & show name
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
        // Try to parse and print its name, too
        SynthPatch commit;
        if (loadPatchFromFile(-1, commit)) {
            Serial.printf("[debugFileSystem] Commit patch loads OK, name: '%s'\n", commit.name);
        } else {
            Serial.println("[debugFileSystem] Commit patch FAILED to load");
        }
    } else {
        Serial.println("[debugFileSystem] commit_patch.json does not exist");
    }

    Serial.println("[debugFileSystem] === Debug Complete ===\n");
}

bool loadPatchFromFile(int slot, SynthPatch& patch) {
    String filename = (slot == -1) ? "/commit_patch.json" : "/patch_" + String(slot) + ".json";
    if (slot != -1 && (slot < 0 || slot >= 8)) return false;

    WEB_LOGF("[loadPatchFromFile] Loading patch from: %s\n", filename.c_str());

    File file = LittleFS.open(filename, "r");
    if (!file) {
        WEB_LOGF("[loadPatchFromFile] File does not exist: %s\n", filename.c_str());
        return false;
    }

    String content = file.readString();
    file.close();
    WEB_LOGF("[loadPatchFromFile] File content length: %d bytes\n", content.length());

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, content);
    if (error) {
        Serial.printf("[loadPatchFromFile] JSON parse error: %s\n", error.c_str());
        return false;
    }

    // --- now safely read fields as you already do ---
    const char* nameFromFile = doc["name"];
    if (nameFromFile) {
        strncpy(patch.name, nameFromFile, 16);
        patch.name[16] = '\0';
        WEB_LOGF("[loadPatchFromFile] Loaded name: '%s'\n", patch.name);
    } else {
        String defaultName = "P" + String(slot);
        strncpy(patch.name, defaultName.c_str(), 16);
        patch.name[16] = '\0';
        WEB_LOGF("[loadPatchFromFile] No name in file, using default: '%s'\n", patch.name);
    }

    // Envelope
    patch.a = doc["envelope"]["attack"] | 0.08f;
    patch.d = doc["envelope"]["decay"] | 0.7f;
    patch.s = doc["envelope"]["sustain"] | 0.85f;
    patch.r = doc["envelope"]["release"] | 0.8f;

    // Oscillator
    patch.morph = doc["morph"] | 0.35f;

    // Filter
    patch.cutoff = doc["filter"]["cutoff"] | 1400.f;
    patch.resonance = doc["filter"]["resonance"] | 0.8f;
    patch.filterOn = doc["filter"]["enabled"] | true;

    // Motion
    patch.glide = doc["glide"] | 0.06f;
    patch.detune = doc["detune"] | 12.f;
    patch.unison = doc["unison"] | 1;

    // Tempo / Sync
    patch.tempoSrc = doc["tempo"]["source"] | 0;
    patch.bpmInt   = doc["tempo"]["bpm"] | 120.0f;
    patch.lfoSync  = doc["tempo"]["lfoSync"] | false;
    patch.dlySync  = doc["tempo"]["delaySync"] | true;

    // Mixer
    patch.masterGain = doc["mix"]["masterGain"] | 0.7f;
    patch.noiseAmt   = doc["mix"]["noiseAmount"] | 0.0f;

    // Delay
    patch.dlyTime = doc["delay"]["time"] | 0.14f;
    patch.dlyFb   = doc["delay"]["feedback"] | 0.35f;
    patch.dlyMix  = doc["delay"]["mix"] | 0.12f;

    // LFO1
    patch.lfoAmtHz     = doc["lfo"]["amount"] | 350.f;
    patch.lfoRateHz    = doc["lfo"]["rate"] | 0.6f;
    patch.lfoToMorph   = doc["lfo"]["toMorph"] | 0.25f;
    patch.lfoToAmp     = doc["lfo"]["toAmp"] | 0.10f;
    patch.lfoToDetune  = doc["lfo"]["toDetune"] | 0.0f;

    // Mod routing (Performance)
    patch.velToCutoff   = doc["performance"]["velToCutoff"] | 0.0f;
    patch.noiseToCutoff = doc["performance"]["noiseToCutoff"] | 0.0f;

    // LFO2 / Vibrato
    patch.lfo2RateHz = doc["lfo2"]["rate"] | 5.0f;
    patch.lfo2AmtSemi = doc["lfo2"]["amount"] | 0.2f;
    patch.lfo2Wave   = doc["lfo2"]["wave"] | 0;

    // Chorus
    patch.chMix   = doc["chorus"]["mix"] | 0.0f;
    patch.chDepth = doc["chorus"]["depth"] | 5.0f;

    // Lightweight FX
    patch.bitcrushMix = doc["fx"]["bitcrushMix"] | 0.0f;
    patch.bitcrushBits = doc["fx"]["bitcrushBits"] | 16.0f;
    patch.bitcrushRateDiv = doc["fx"]["bitcrushRateDiv"] | 1.0f;
    patch.tremDepth = doc["fx"]["tremDepth"] | 0.0f;
    patch.tremRateHz = doc["fx"]["tremRateHz"] | 4.0f;
    patch.driveAmount = doc["fx"]["driveAmount"] | 0.0f;
    patch.foldAmount = doc["fx"]["foldAmount"] | 0.0f;

    // Arp
    patch.arpMode = doc["arp"]["mode"] | 0;
    patch.arpDiv  = doc["arp"]["division"] | 2;
    patch.arpGate = doc["arp"]["gate"] | 60;

    // Voice Management
    patch.stealMode = doc["voice"]["stealMode"] | 0;
    patch.velCurve  = doc["voice"]["velCurve"] | 0;

    normalizePatch(patch);  // Legacy files stay unchanged until an explicit save.
    WEB_LOGLN("[loadPatchFromFile] Patch loaded successfully");
    return true;
}

void sendPatchToSynth(const SynthPatch& patch) {
    Serial.println("[sendPatchToSynth] Sending complete patch to Uno R4...");

    struct { const char* param; float value; } ccParams[] = {
        {"attack", patch.a}, {"decay", patch.d}, {"sustain", patch.s}, {"release", patch.r},
        {"cutoff", patch.cutoff}, {"resonance", patch.resonance}, {"morph", patch.morph},
        {"filterOn", patch.filterOn ? 1.0f : 0.0f}, {"glide", patch.glide}, {"detune", patch.detune},
        {"masterGain", patch.masterGain}, {"noiseAmt", patch.noiseAmt},
        {"delayTime", patch.dlyTime}, {"delayFeedback", patch.dlyFb}, {"delayMix", patch.dlyMix},
        {"lfoAmtHz", patch.lfoAmtHz}, {"lfoRateHz", patch.lfoRateHz}, {"lfoSync", patch.lfoSync ? 1.0f : 0.0f},
        {"velToCutoff", patch.velToCutoff}, {"noiseToCutoff", patch.noiseToCutoff},
        {"lfoToDetune", patch.lfoToDetune}, {"unison", (float)patch.unison},
        {"chMix", patch.chMix}, {"arpMode", (float)patch.arpMode},
        {"bitcrushMix", patch.bitcrushMix}, {"bitcrushBits", patch.bitcrushBits},
        {"bitcrushRateDiv", patch.bitcrushRateDiv}, {"tremDepth", patch.tremDepth},
        {"tremRateHz", patch.tremRateHz}, {"driveAmount", patch.driveAmount},
        {"foldAmount", patch.foldAmount},
        {"arpDiv", (float)patch.arpDiv}, {"arpGate", (float)patch.arpGate},
        {"lfo2RateHz", patch.lfo2RateHz}, {"lfo2AmtSemi", patch.lfo2AmtSemi},
        {"velCurve", (float)patch.velCurve},
        {"stealMode", (float)patch.stealMode}
    };

    for (auto &p : ccParams) {
        int ccNum = getParamCC(p.param);
        if (ccNum >= 0) {
            int ccVal = paramValueToCC(p.param, p.value);
            MIDI_UART.sendControlChange(ccNum, ccVal, 1);
            delay(2);
        }
    }

    // Send specials that aren't straight CCs.
    delay(10);
    sendControlToUno("bpm", patch.bpmInt);
    sendControlToUno("tempoSrc", patch.tempoSrc);
    sendControlToUno("dlySync", patch.dlySync);
    sendControlToUno("lfoToMorph", patch.lfoToMorph);
    sendControlToUno("lfoToAmp", patch.lfoToAmp);
    sendControlToUno("chDepth", patch.chDepth);
    sendControlToUno("lfo2Wave", patch.lfo2Wave);

    const uint8_t complete[] = {0xF0, 0x7D, 0x00, 0x12, 0xF7};
    MIDI_UART.sendSysEx(sizeof(complete), complete, true);

    Serial.println("[sendPatchToSynth] Complete patch sent to Uno R4");
}

void broadcastPatchList() {
    WEB_LOGLN("[broadcastPatchList] Building patch list...");
    
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
            WEB_LOGF("[broadcastPatchList] User patch %d: %s (exists)\n", i, patch.name);
        } else {
            String defaultName = "P" + String(i);
            patchObj["name"] = defaultName;
            patchObj["exists"] = false;
            WEB_LOGF("[broadcastPatchList] User patch %d: %s (empty)\n", i, defaultName.c_str());
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
        WEB_LOGF("[broadcastPatchList] Factory preset %d: %s\n", i + 100, FACTORY_PRESETS[i].name);
    }

    String response;
    size_t len = serializeJson(doc, response);
    (void)len;
    WEB_LOGF("[broadcastPatchList] JSON size: %d bytes\n", len);
    WEB_LOGF("[broadcastPatchList] Broadcasting to %d clients\n", websocket.connectedClients());
    
    websocket.broadcastTXT(response);
    
    // Also send current patch index
    JsonDocument currentDoc;
    currentDoc["type"] = "currentPatch";
    currentDoc["index"] = config.currentPatch;
    String currentResponse;
    serializeJson(currentDoc, currentResponse);
    websocket.broadcastTXT(currentResponse);
    
    WEB_LOGLN("[broadcastPatchList] Patch list broadcast complete");
}
