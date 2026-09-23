/*  QuarkWave – Uno R4 WiFi Synth (UART controlled version)
   - Poly: 4 voices, unison 1..3 (per voice)
   - Osc: sine/tri/saw/square with morph
   - Filter: 2-pole TPT SVF (cutoff+resonance)
   - Portamento (glide)
   - UART: receives commands from QuarkWave_UI.cpp
   - LED matrix: status, meters, VU, scope, waterfall

   Extras:
   - Pitch bend (±2 semitones; DIN/RTP/BLE)
   - White-noise layer (CC93)
   - Simple delay/echo (CC12 time, CC13 feedback, CC14 mix)
   - MIDI Clock (24 PPQN) → LFO sync; CC3 toggles sync
   - Panic (UART + MIDI CC120/123)
*/

// ---------- User config (must come first) ----------
#define SAMPLE_RATE 22050

#define USE_CHORUS 1
#define NUM_VOICES 4
#define MAX_UNISON 3
#define PER_VOICE_FILTER 0  // 0 = global SVF, 1 = per-voice SVF
#define USE_RTP_MIDI 1
#define BLE_DEVICE_NAME "QuarkWave"
#define USE_BLE_MIDI 0
#define USE_WIFI_STACK 1  // <- still needed for RTP-MIDI
#define DEBUG_LOG 0

#if !USE_WIFI_STACK && USE_RTP_MIDI
#undef USE_RTP_MIDI
#define USE_RTP_MIDI 0
#pragma message("RTP-MIDI disabled because USE_WIFI_STACK=0")
#endif

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>
#include <ArduinoGraphics.h>
#include <Arduino_LED_Matrix.h>
#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#include <MIDI.h>
#include "secrets.h"

#if DEBUG_LOG
#define LOG_PRINT(x) Serial.print(x)
#define LOG_PRINTLN(x) Serial.println(x)
#else
#define LOG_PRINT(x) do {} while (0)
#define LOG_PRINTLN(x) do {} while (0)
#endif

#if USE_WIFI_STACK
#include <WiFiS3.h>
#include <WiFiUdp.h>
#endif

// --- Forward declarations so Arduino's auto-prototypes know these types ---
struct Voice;

// --- Forward decls for helpers used before their definitions ---
inline float clampf(float x, float a, float b);
inline float fastLerp(float a, float b, float t);
inline float maxf(float a, float b);
inline float warp01(float x, float bend);
static uint16_t crc16_zeroed_crc(const void* ptr, size_t len);
inline float randomNoiseSlow();
static void waterfallInit();
static inline void waterfallProcessIfReady();

#if USE_RTP_MIDI
#include <AppleMIDI.h>
#endif
#if USE_BLE_MIDI
#include <ArduinoBLE.h>
#include <Control_Surface.h>
#include <MIDI_Interfaces/MIDI_Pipes.hpp>
#include <MIDI_Interfaces/BluetoothMIDI_Interface.hpp>
#endif

// Filter (TPT SVF)
inline float zapDenorm(float x) {
  return (fabsf(x) < 1e-20f) ? 0.f : x;
}
struct SVF {
  float g, R;
  float ic1eq = 0, ic2eq = 0;
  float cutoff = 2000.0f, Q = 0.9f;
  bool enabled = true;

  inline void update(float fs) {
    float fc = clampf(cutoff, 20.0f, fs * 0.45f);
    float q = clampf(Q, 0.5f, 3.0f);
    g = tanf(PI * fc / fs);
    R = 1.0f / q;
  }
  inline void setCutoff(float fc) {
    cutoff = fc;
    update(SAMPLE_RATE);
  }
  inline float process(float x) {
    if (!enabled) return x;
    float v1 = (x - ic2eq) / (1.0f + R * g + g * g);
    float v2 = g * v1;
    float lp = ic2eq + g * (v1 + ic1eq);
    ic1eq += 2.0f * v2 + R * v1;
    ic2eq += 2.0f * lp;
    ic1eq = zapDenorm(ic1eq);
    ic2eq = zapDenorm(ic2eq);
    return lp;
  }
} svf;

struct ADSR {
  float a, d, s, r, env;
  bool gate;
};
struct Voice {
  bool active;
  uint8_t note;
  float phase[MAX_UNISON], inc[MAX_UNISON];
  float incTarget[MAX_UNISON];
  uint8_t unison;
  float vel;
  ADSR eg;
#if PER_VOICE_FILTER
  SVF svf;              // per-voice state variable filter
  float cfY = 1200.0f;  // current smoothed cutoff (Hz)
  float cfT = 1200.0f;  // target cutoff (Hz)
#endif
};
static Voice V[NUM_VOICES];
static const float SVF_CUTOFF_TAU_MS = 18.0f;


// ---------- Milestones / diagnostics ----------
volatile uint8_t lastMilestone = 0;
static uint32_t matrixHoldUntil = 0;
static bool uartEverConnected = false;
static uint32_t rtpRxPipHoldUntil = 0;
inline void touchRtpRx() {
  rtpRxPipHoldUntil = millis() + 300;
}

#if USE_BLE_MIDI
volatile bool bleOK = false;
#endif

static uint32_t wifiPipHoldUntil = 0, rtpPipHoldUntil = 0;
static uint32_t uartPipHoldUntil = 0;
static uint32_t uartRxPipHoldUntil = 0;

static bool ledStalled = false;

// ---- Waterfall (12-bin Goertzel) ----
#define WF_MODE 0  // 0 = lightweight output-energy waterfall.
                   // 1 = capture decimated audio for spectral waterfall.

static const uint8_t WF_COLS = 12;
static const uint8_t WF_DECIM = 5;
static const uint16_t WF_FS = SAMPLE_RATE / WF_DECIM;
static const uint16_t WF_N = 128;
static const float WF_ALPHA = 0.08f;
static const float WF_GAIN = 1.7f;

// Goertzel state
static float wf_bins[WF_COLS] = { 0 };
static float wf_thresh[WF_COLS] = { 0.02f };
static float wf_s1[WF_COLS] = { 0 }, wf_s2[WF_COLS] = { 0 };
static float wf_coef[WF_COLS] = { 0 };

// Frame assembler (decimated)
static uint8_t wf_decim_ctr = 0;
static int8_t wf_frame[WF_N];
static uint16_t wf_i = 0;
static volatile bool wf_ready = false;

// Log-ish spaced bin centers (Hz), all < Nyquist (~2205 Hz)
static const float WF_FC[WF_COLS] = { 110, 150, 200, 270, 360, 480, 640, 850, 1100, 1400, 1750, 2000 };

// ---------- Globals (audio / synth) ----------
static const uint16_t WT_SIZE = 256;
static int16_t WT_SINE[WT_SIZE], WT_SAW[WT_SIZE];

static float masterGain = 0.7f;
static float glideTime = 0.03f;
static uint8_t unisonCount = 1;
static float unisonDetuneCents = 8;

static float detuneRatio = 1.0f;
static float glideAlpha = 0.0f;

// UART-side dedupe/debounce for CCs
static uint8_t g_lastCcVal[128] = { 0 };
static uint32_t g_lastCcMs[128] = { 0 };

static bool g_cfgDirty = false;
static uint32_t g_cfgTouchedAt = 0;
static uint32_t g_cfgNextWriteAt = 0;

// ---------- Timing / rate tunables ----------
static const uint16_t CC_MIN_GAP_MS = 10;
static const uint16_t TEXT_MIN_GAP_MS = 7;
static const uint16_t UART_SINGLE_CC_MIN_GAP_MS = 8;
static const uint16_t UART_BATCH_MIN_GAP_MS = 3;
static const uint8_t UART_MAX_ITEMS = 20;
static const uint32_t UART_MAX_US = 5000;


// --- Pitch bend ---
static float pitchBendRatio = 1.0f;
static uint8_t pitchBendRange = 2;
inline void setPitchBend(int16_t v) {
  float semi = ((float)v - 8192.0f) * pitchBendRange / 8192.0f;
  pitchBendRatio = powf(2.0f, semi / 12.0f);
}

// Output conditioning
struct DCBlock {
  float y1 = 0, x1 = 0;
} dc;
inline float dcBlock(float x) {
  const float R = 0.995f;
  float y = x - dc.x1 + R * dc.y1;
  dc.x1 = x;
  dc.y1 = y;
  return y;
}
inline float softClip(float x) {
  const float a = 0.5f;
  return x * (1.0f - a * x * x);
}

// Output character FX
static float crushAmt = 0.0f;
static float tremRateHz = 5.0f, tremDepth = 0.0f, tremGain = 1.0f, tremPhase = 0.0f;
static float driveAmt = 0.0f, foldAmt = 0.0f;
static float crushHold = 0.0f;
static uint8_t crushCtr = 0;

inline float foldSample(float x) {
  for (uint8_t i = 0; i < 3; ++i) {
    if (x > 1.0f) x = 2.0f - x;
    else if (x < -1.0f) x = -2.0f - x;
    else break;
  }
  return clampf(x, -1.0f, 1.0f);
}

inline float applyOutputFX(float y) {
  if (driveAmt > 0.001f) {
    const float g = 1.0f + driveAmt * 7.0f;
    const float x = y * g;
    const float driven = x / (1.0f + fabsf(x));
    y = fastLerp(y, driven, driveAmt);
  }

  if (foldAmt > 0.001f) {
    const float folded = foldSample(y * (1.0f + foldAmt * 5.0f));
    y = fastLerp(y, folded, foldAmt);
  }

  if (crushAmt > 0.001f) {
    const uint8_t decim = 1 + (uint8_t)roundf(crushAmt * 15.0f);
    if (crushCtr == 0) {
      const uint8_t bits = 12 - (uint8_t)roundf(crushAmt * 8.0f);
      const int levels = 1 << bits;
      const float scale = (float)((levels / 2) - 1);
      crushHold = roundf(clampf(y, -1.0f, 1.0f) * scale) / scale;
    }
    crushCtr = (uint8_t)((crushCtr + 1) % decim);
    y = crushHold;
  } else {
    crushCtr = 0;
    crushHold = y;
  }

  if (tremDepth > 0.001f) y *= tremGain;

  return y;
}

// Control params
static float morph = 0.0f;
static float cutoff = 2000.0f, resonance = 0.9f;
static float noiseAmt = 0.0f;  // CC93

// Delay
static const uint16_t DLY_SIZE = 2048;
static int8_t dlyBuf[DLY_SIZE];
static uint16_t dlyW = 0;
static float dlyTime = 0.25f, dlyFb = 0.25f, dlyMix = 0.0f;

// MIDI Clock → LFO sync
static uint32_t _clkLast = 0;
static uint8_t _clkCnt = 0;
static float bpmExt = 120.0f;
static float bpmInt = 120.0f;
static uint8_t tempoSrc = 1;
static bool lfoSync = true;
static float lfoPhase = 0.0f;
static float lfoRateHz = 3.0f;
static float lfoAmtHz = 800.0f;

// Mini mod-matrix
static float lfoToMorph = 0.0f;
static float lfoToAmp = 0.0f;
static bool dlySync = false;
enum LfoWave : uint8_t { LFO_SINE = 0,
                         LFO_TRI = 1,
                         LFO_SQR = 2 };
static float lfo2Phase = 0.0f;
static float lfo2RateHz = 5.0f;
static float lfo2AmtSemi = 0.2f;
static float lfo2Ratio = 1.0f;
static LfoWave lfo2Wave = LFO_SINE;

// --- Extra mini mod-matrix amounts (0..1 unless noted)
static float velToCutoff = 0.0f;
static float noiseToCutoff = 0.0f;
static float lfoToDetune = 0.0f;

// Velocity curve + voice steal modes
enum VelCurveMode : uint8_t { VEL_LINEAR = 0,
                              VEL_SOFT = 1,
                              VEL_HARD = 2,
                              VEL_EXPO = 3 };
static VelCurveMode velMode = VEL_SOFT;

enum VoiceSteal : uint8_t { STEAL_QUIETEST = 0,
                            STEAL_LAST = 1 };
static VoiceSteal stealMode = STEAL_QUIETEST;
static uint32_t g_noteStamp[NUM_VOICES] = { 0 };

// Timing
static uint32_t nextSample, SAMPLE_PERIOD = 1000000UL / SAMPLE_RATE;

// ---------- Control-rate smoothing ----------
#define CONTROL_RATE 1000u
static const float CTRL_DT = 1.0f / (float)CONTROL_RATE;
static uint32_t nextCtrlAt;

struct SmootherExp {
  float y = 0.0f;
  float target = 0.0f;
  float alpha = 1.0f;
  void setTauMs(float tau_ms) {
    float tau = tau_ms * 0.001f;
    alpha = (tau <= 0.00001f) ? 1.0f : (1.0f - expf(-CTRL_DT / tau));
  }
  inline void setValue(float v) {
    y = target = v;
  }
  inline void setTarget(float v) {
    target = v;
  }
  inline void step() {
    y += (target - y) * alpha;
  }
};

static SmootherExp smCutoff, smReso, smMorph, smGain, smGlide, smDetune;
static const float CUTOFF_EPS = 1.0f;
static const float RESO_EPS = 0.005f;
static const float MORPH_EPS = 0.002f;
static const float GAIN_EPS = 0.002f;
static const float GLIDE_EPS = 0.001f;
static const float DETUNE_EPS = 0.05f;

inline float warp01(float x, float bend) {
  if (bend == 0.0f) return x;
  if (bend > 0.0f) return powf(x, 1.0f + 3.0f * bend);
  return 1.0f - powf(1.0f - x, 1.0f + 3.0f * (-bend));
}

// ---------- LED Matrix ----------
ArduinoLEDMatrix matrix;
uint8_t ledBuf[8][12];

volatile uint8_t noteBlink = 0;
enum VizMode : uint8_t { VIZ_STATUS = 0,
                         VIZ_VU = 1,
                         VIZ_SCOPE = 2,
                         VIZ_WATER = 3 } vizMode = VIZ_STATUS;

static volatile float vizPeakAccum = 0.0f;
static int8_t vizScope[12] = { 0 };
static uint8_t vizScopeWrite = 0;
static uint8_t vizScopeDecim = 0;
static const uint8_t VIZ_SCOPE_DECIM = ((SAMPLE_RATE / 480u) > 0) ? (SAMPLE_RATE / 480u) : 1u;

// ---------- Networking / UART ----------
#if USE_WIFI_STACK
bool wifiOK = false, rtpOK = false;
#endif

// ---------- DIN MIDI ----------
SoftwareSerial midiSerial(2, 3); // RX on pin 2, TX on pin 3
MIDI_CREATE_INSTANCE(SoftwareSerial, midiSerial, DIN_MIDI);

// ---------- RTP MIDI ----------
#if USE_RTP_MIDI
#ifdef USING_NAMESPACE_APPLEMIDI
USING_NAMESPACE_APPLEMIDI
#endif
APPLEMIDI_CREATE_INSTANCE(WiFiUDP, rtpMIDI, BLE_DEVICE_NAME, 5004);
void setupRTP();
static inline bool rtpIsUp() {
  return rtpOK;
}
#endif

// ---------- BLE-MIDI ----------
#if USE_BLE_MIDI
cs::BluetoothMIDI_Interface BLE_MIDI;
#endif

// ---------- UART Communication ----------
static char uartBuf[512];
static StaticJsonDocument<512> uartDoc;

// =================== Utilities ===================
#if USE_BLE_MIDI
inline void updateBLEStatus() {
  BLEDevice c = BLE.central();
  bleOK = (c && c.connected());
}
#endif

inline float clampf(float x, float a, float b) {
  return x < a ? a : (x > b ? b : x);
}
inline float fastLerp(float a, float b, float t) {
  return a + (b - a) * t;
}
inline float maxf(float a, float b) {
  return a > b ? a : b;
}
inline float midiToFreq(uint8_t n) {
  return 440.0f * powf(2.0f, (int(n) - 69) / 12.0f);
}
inline float centsToRatio(float c) {
  return powf(2.0f, c / 1200.0f);
}
inline void updateDetuneRatio() {
  detuneRatio = centsToRatio(unisonDetuneCents);
}
inline void updateGlideAlpha() {
  glideTime = maxf(glideTime, 0.0f);
  glideAlpha = (glideTime > 0.001f) ? expf(-1.0f / (SAMPLE_RATE * glideTime)) : 0.0f;
}

// Wavetables
void initWavetables() {
  for (uint16_t i = 0; i < WT_SIZE; i++) {
    float p = (float)i / WT_SIZE;
    float s = sinf(2 * PI * p);
    float saw = 2.0f * p - 1.0f;
    WT_SINE[i] = (int16_t)roundf(s * 32767.0f);
    WT_SAW[i] = (int16_t)roundf(saw * 32767.0f);
  }
}
inline float wtReadSine(uint16_t idx) {
  return WT_SINE[idx] / 32767.0f;
}
inline float wtReadSaw(uint16_t idx) {
  return WT_SAW[idx] / 32767.0f;
}

// Osc morph
inline float oscRead(const float ph, const float morph01) {
  uint16_t idx = ((uint16_t)(ph * WT_SIZE)) & (WT_SIZE - 1);
  float sine = wtReadSine(idx);
  float saw = wtReadSaw(idx);

  float tri = 2.0f * fabsf(saw) - 1.0f;
  tri = (saw >= 0.0f) ? tri : -tri;

  float sqr = (sine >= 0.0f) ? 1.0f : -1.0f;

  float t = morph01;
  if (t < 1.0f / 3.0f) return fastLerp(sine, tri, (t * 3.0f));
  else if (t < 2.0f / 3.0f) return fastLerp(tri, saw, ((t - 1.0f / 3.0f) * 3.0f));
  else return fastLerp(saw, sqr, ((t - 2.0f / 3.0f) * 3.0f));
}

// ADSR
inline void adsrTick(struct ADSR& e, const float dt) {
  if (e.gate) {
    if (e.env < 1.0f) {
      e.env += dt / maxf(1e-5f, e.a);
      if (e.env > 1.0f) e.env = 1.0f;
    } else if (e.env > e.s) {
      e.env -= dt / maxf(1e-5f, e.d);
      if (e.env < e.s) e.env = e.s;
    }
  } else {
    e.env -= dt / maxf(1e-5f, e.r);
    if (e.env < 0) e.env = 0;
  }
}

// SVF
void svfUpdateCoeffs() {
  float fc = clampf(cutoff, 20.0f, SAMPLE_RATE * 0.45f);
  float Q = clampf(resonance, 0.5f, 3.0f);
  svf.g = tanf(PI * fc / SAMPLE_RATE);
  svf.R = 1.0f / Q;
}
inline float svfProcess(float x) {
  if (!svf.enabled) return x;
  float v1 = (x - svf.ic2eq) * 1.0f / (1.0f + svf.R * svf.g + svf.g * svf.g);
  float v2 = svf.g * v1;
  float lp = svf.ic2eq + svf.g * (v1 + svf.ic1eq);
  svf.ic1eq += 2.0f * v2 + svf.R * v1;
  svf.ic2eq += 2.0f * lp;
  svf.ic1eq = zapDenorm(svf.ic1eq);
  svf.ic2eq = zapDenorm(svf.ic2eq);
  return lp;
}
inline void setFilterEnabled(bool on) {
  svf.enabled = on;
  if (on) svf.ic1eq = svf.ic2eq = 0;
}

// Sustain
static bool sustainOn = false;
static bool heldNotes[128] = { false };
static bool sustainLatch[128] = { false };

enum ArpMode : uint8_t { ARP_OFF = 0,
                         ARP_UP = 1,
                         ARP_DOWN = 2,
                         ARP_UP_DOWN = 3,
                         ARP_RANDOM = 4 };
static ArpMode arpMode = ARP_OFF;
static uint8_t arpDiv = 2;
static uint8_t arpGate = 60;

// --- Chorus
#if USE_CHORUS
static const uint16_t CH_BUF = 256;
static int8_t chBuf[CH_BUF];
static uint16_t chW = 0;
static float chRate1 = 0.25f, chRate2 = 0.33f;
static float chDepth = 5.0f;
static float chMix = 0.25f;
static float chPh1 = 0.0f, chPh2 = 0.5f;
#else
static float chMix = 0.0f;
static float chDepth = 0.0f;
#endif

// Voice management
int8_t allocVoice() {
  for (uint8_t i = 0; i < NUM_VOICES; ++i)
    if (!V[i].active || (!V[i].eg.gate && V[i].eg.env <= 0.0005f))
      return i;

  if (stealMode == STEAL_LAST) {
    uint32_t best = 0;
    int8_t idx = 0;
    for (uint8_t i = 0; i < NUM_VOICES; ++i)
      if (g_noteStamp[i] >= best) {
        best = g_noteStamp[i];
        idx = i;
      }
    return idx;
  } else {
    float minEnv = 1e9f;
    int8_t idx = 0;
    for (uint8_t i = 0; i < NUM_VOICES; ++i) {
      float e = V[i].eg.env;
      if (e < minEnv) {
        minEnv = e;
        idx = i;
      }
    }
    return idx;
  }
}

inline float velCurve(uint8_t v) {
  float x = v / 127.0f;
  switch (velMode) {
    case VEL_LINEAR: return x;
    case VEL_SOFT: return powf(x, 0.6f);
    case VEL_HARD: return powf(x, 1.6f);
    case VEL_EXPO: return (expf(2.0f * x) - 1.0f) / (expf(2.0f) - 1.0f);
  }
  return x;
}

void synthNoteOn(uint8_t note, uint8_t vel) {
  heldNotes[note] = true;
  int8_t i = allocVoice();
  Voice& v = V[i];
  g_noteStamp[i] = millis();
  bool wasActive = v.active && v.eg.gate;
  v.active = true;
  v.note = note;
  v.vel = clampf(velCurve(vel), 0, 1);
  v.unison = unisonCount;

  float center = midiToFreq(note);
  for (uint8_t u = 0; u < v.unison; ++u) {
    float ff = (v.unison == 1) ? center : (u == 0 ? center * detuneRatio : center / detuneRatio);
    float newInc = ff / SAMPLE_RATE;
    if (wasActive && glideTime > 0.001f) {
      v.incTarget[u] = newInc;
    } else {
      v.phase[u] = 0.0f;
      v.inc[u] = newInc;
      v.incTarget[u] = newInc;
    }
  }
  v.eg.gate = true;
  noteBlink = 4;
}

void synthNoteOff(uint8_t note) {
  heldNotes[note] = false;
  if (sustainOn) {
    sustainLatch[note] = true;
    return;
  }
  for (auto& v : V)
    if (v.active && v.note == note) v.eg.gate = false;
}

// Sustain helper
void sustainSet(bool on) {
  sustainOn = on;
  if (!sustainOn) {
    for (uint8_t n = 0; n < 128; ++n) {
      if (sustainLatch[n] && !heldNotes[n]) synthNoteOff(n);
      sustainLatch[n] = false;
    }
  }
}

// CC map
void allNotesOff() {
  for (auto& v : V) { v.eg.gate = false; }
  memset(heldNotes, 0, sizeof(heldNotes));
  memset(sustainLatch, 0, sizeof(sustainLatch));
}

void handleCC(uint8_t cc, uint8_t val) {
  const float bendCut = 0.35f;
  const float bendReso = 0.20f;
  const float bendVol = -0.15f;
  const float t0 = val / 127.0f;

  LOG_PRINT(F("[handleCC] cc="));
  LOG_PRINT(cc);
  LOG_PRINT(F(" val="));
  LOG_PRINTLN(val);

  switch (cc) {
    case 1:
      {
        lfoAmtHz = 0.0f + 3000.0f * (val / 127.0f);
        break;
      }
    case 2:
      {
        lfoRateHz = 0.1f + 12.0f * (val / 127.0f);
        break;
      }
    case 3:
      {
        bool prev = lfoSync;
        lfoSync = (val >= 64);
        break;
      }
    case 5:
      {
        smGlide.setTarget(0.0f + 0.3f * t0);
        break;
      }
    case 7:
      {
        float t = warp01(t0, bendVol);
        float g = 0.2f + 0.8f * t;
        smGain.setTarget(g);
        masterGain = g;
        break;
      }
    case 12:
      {
        dlyTime = 0.02f + 0.166f * (val / 127.0f);
        break;
      }
    case 13:
      {
        dlyFb = 0.01f + 0.89f * (val / 127.0f);
        break;
      }
    case 14:
      {
        dlyMix = (val / 127.0f);
        break;
      }
    case 16:
      {
        velMode = (VelCurveMode)constrain((int)roundf((val / 127.0f) * 3), 0, 3);
        break;
      }
    case 17:
      {
        stealMode = (val >= 64) ? STEAL_LAST : STEAL_QUIETEST;
        break;
      }
    case 18:
      {
        if (val < 26) arpMode = ARP_OFF;
        else if (val < 51) arpMode = ARP_UP;
        else if (val < 77) arpMode = ARP_DOWN;
        else if (val < 102) arpMode = ARP_UP_DOWN;
        else arpMode = ARP_RANDOM;
        break;
      }
    case 19:
      {
        arpDiv = (uint8_t)roundf((val / 127.0f) * 7);
        break;
      }
    case 20:
      {
        arpGate = (uint8_t)constrain((int)roundf((val / 127.0f) * 95), 5, 95);
        break;
      }
    case 21:
      {
        chMix = (val / 127.0f);
        break;
      }
    case 23:
      {
        float t = t0;
        for (auto& v : V) v.eg.s = 0.10f + 0.85f * t;
        break;
      }
    case 24:
      {
        velToCutoff = (val / 127.0f);
        break;
      }
    case 25:
      {
        noiseToCutoff = (val / 127.0f);
        break;
      }
    case 26:
      {
        lfoToDetune = (val / 127.0f);
        break;
      }
    case 27:
      {
        lfo2RateHz = 0.05f + 19.95f * (val / 127.0f);
        break;
      }
    case 28:
      {
        lfo2AmtSemi = 0.0f + 2.0f * (val / 127.0f);
        break;
      }
    case 29:
      {
        crushAmt = t0;
        break;
      }
    case 30:
      {
        tremRateHz = 0.2f + 19.8f * t0;
        break;
      }
    case 31:
      {
        tremDepth = t0;
        break;
      }
    case 33:
      {
        driveAmt = t0;
        break;
      }
    case 34:
      {
        foldAmt = t0;
        break;
      }
    case 64:
      {
        sustainSet(val >= 64);
        break;
      }
    case 71:
      {
        float t = warp01(t0, bendReso);
        smReso.setTarget(0.5f + 2.5f * t);
        break;
      }
    case 72:
      {
        float t = t0;
        for (auto& v : V) v.eg.r = 0.02f + 1.5f * t;
        break;
      }
    case 73:
      {
        float t = t0;
        for (auto& v : V) v.eg.a = 0.002f + 0.5f * t;
        break;
      }
    case 74:
      {
        float t = warp01(t0, bendCut);
        smCutoff.setTarget(40.0f + 10000.0f * t);
        break;
      }
    case 75:
      {
        float t = t0;
        for (auto& v : V) v.eg.d = 0.01f + 1.0f * t;
        break;
      }
    case 76:
      {
        smMorph.setTarget(t0);
        break;
      }
    case 91:
      {
        setFilterEnabled(val >= 64);
        break;
      }
    case 93:
      {
        noiseAmt = (val / 127.0f);
        break;
      }
    case 94:
      {
        smDetune.setTarget(2.0f + 18.0f * t0);
        break;
      }
    case 95:
      {
        unisonCount = 1 + (uint8_t)roundf(t0 * 2.0f);
        break;
      }
    case 120:  // All Sound Off (Panic)
    case 123:  // All Notes Off
      allNotesOff();
      break;
    default: break;
  }
}

// ---------- Patches + tiny config in EEPROM ----------
constexpr uint16_t PATCH_VER = 2;
constexpr uint16_t CFG_VER = 1;

static uint16_t crc16_zeroed_crc(const void* ptr, size_t len) {
  const uint8_t* d = (const uint8_t*)ptr;
  uint16_t c = 0xFFFF;
  for (size_t i = 0; i < len; ++i) {
    c ^= d[i];
    for (int k = 0; k < 8; ++k) c = (c & 1) ? (c >> 1) ^ 0xA001 : (c >> 1);
  }
  return c;
}

static inline uint8_t clampU8(int v, int lo, int hi) {
  if (v < lo) return (uint8_t)lo;
  if (v > hi) return (uint8_t)hi;
  return (uint8_t)v;
}


// CRC
static inline uint16_t crc16_a001(const void* ptr, size_t len) {
  const uint8_t* d = (const uint8_t*)ptr;
  uint16_t c = 0xFFFF;
  for (size_t i = 0; i < len; ++i) {
    c ^= d[i];
    for (int k = 0; k < 8; ++k) c = (c & 1) ? (c >> 1) ^ 0xA001 : (c >> 1);
  }
  return c;
}


// UART State broadcast
static void sendStateToUART() {
  const float bpmUse = (tempoSrc == 1 ? bpmExt : bpmInt);

#if USE_CHORUS
  const float CH_MIX_OUT = chMix;
#else
  const float CH_MIX_OUT = 0.0f;
#endif

  Serial1.print(F("{\"t\":\"state\","));
  Serial1.print(F("\"a\":")); Serial1.print(V[0].eg.a, 4); Serial1.print(F(","));
  Serial1.print(F("\"d\":")); Serial1.print(V[0].eg.d, 4); Serial1.print(F(","));
  Serial1.print(F("\"s\":")); Serial1.print(V[0].eg.s, 4); Serial1.print(F(","));
  Serial1.print(F("\"r\":")); Serial1.print(V[0].eg.r, 4); Serial1.print(F(","));
  Serial1.print(F("\"morph\":")); Serial1.print(morph, 4); Serial1.print(F(","));
  Serial1.print(F("\"cutoff\":")); Serial1.print(cutoff, 2); Serial1.print(F(","));
  Serial1.print(F("\"res\":")); Serial1.print(resonance, 3); Serial1.print(F(","));
  Serial1.print(F("\"glide\":")); Serial1.print(glideTime, 4); Serial1.print(F(","));
  Serial1.print(F("\"detune\":")); Serial1.print(unisonDetuneCents, 2); Serial1.print(F(","));
  Serial1.print(F("\"unison\":")); Serial1.print(unisonCount); Serial1.print(F(","));
  Serial1.print(F("\"filterOn\":")); Serial1.print(svf.enabled ? 1 : 0); Serial1.print(F(","));
  Serial1.print(F("\"gain\":")); Serial1.print(masterGain, 3); Serial1.print(F(","));
  Serial1.print(F("\"noise\":")); Serial1.print(noiseAmt, 3); Serial1.print(F(","));
  Serial1.print(F("\"dlyT\":")); Serial1.print(dlyTime, 4); Serial1.print(F(","));
  Serial1.print(F("\"dlyFb\":")); Serial1.print(dlyFb, 3); Serial1.print(F(","));
  Serial1.print(F("\"dlyMix\":")); Serial1.print(dlyMix, 3); Serial1.print(F(","));
  Serial1.print(F("\"crush\":")); Serial1.print(crushAmt, 3); Serial1.print(F(","));
  Serial1.print(F("\"tremRate\":")); Serial1.print(tremRateHz, 3); Serial1.print(F(","));
  Serial1.print(F("\"tremDepth\":")); Serial1.print(tremDepth, 3); Serial1.print(F(","));
  Serial1.print(F("\"drive\":")); Serial1.print(driveAmt, 3); Serial1.print(F(","));
  Serial1.print(F("\"fold\":")); Serial1.print(foldAmt, 3); Serial1.print(F(","));
  Serial1.print(F("\"lfoSync\":")); Serial1.print(lfoSync ? 1 : 0); Serial1.print(F(","));
  Serial1.print(F("\"bpm\":")); Serial1.print(bpmUse, 2); Serial1.print(F(","));
  Serial1.print(F("\"tempoSrc\":")); Serial1.print(tempoSrc); Serial1.print(F(","));
  Serial1.print(F("\"bpmInt\":")); Serial1.print(bpmInt, 2); Serial1.print(F(","));
  Serial1.print(F("\"lfoMorph\":")); Serial1.print(lfoToMorph, 3); Serial1.print(F(","));
  Serial1.print(F("\"lfoAmp\":")); Serial1.print(lfoToAmp, 3); Serial1.print(F(","));
  Serial1.print(F("\"dlySync\":")); Serial1.print(dlySync ? 1 : 0); Serial1.print(F(","));
  Serial1.print(F("\"velToCut\":")); Serial1.print(velToCutoff, 3); Serial1.print(F(","));
  Serial1.print(F("\"noiseToCut\":")); Serial1.print(noiseToCutoff, 3); Serial1.print(F(","));
  Serial1.print(F("\"lfoToDet\":")); Serial1.print(lfoToDetune, 3); Serial1.print(F(","));
  Serial1.print(F("\"velCurve\":")); Serial1.print((unsigned)velMode); Serial1.print(F(","));
  Serial1.print(F("\"stealMode\":")); Serial1.print((unsigned)stealMode); Serial1.print(F(","));
  Serial1.print(F("\"arpMode\":")); Serial1.print((unsigned)arpMode); Serial1.print(F(","));
  Serial1.print(F("\"arpDiv\":")); Serial1.print((unsigned)arpDiv); Serial1.print(F(","));
  Serial1.print(F("\"arpGate\":")); Serial1.print((unsigned)arpGate); Serial1.print(F(","));
  Serial1.print(F("\"chorusMix\":")); Serial1.print(CH_MIX_OUT, 3); Serial1.print(F(","));
  Serial1.print(F("\"lfo2Rate\":")); Serial1.print(lfo2RateHz, 3); Serial1.print(F(","));
  Serial1.print(F("\"lfo2Amt\":")); Serial1.print(lfo2AmtSemi, 3); Serial1.print(F(","));
  Serial1.print(F("\"lfo2Wave\":")); Serial1.print((unsigned)lfo2Wave);
  Serial1.println(F("}"));
}

// ---------- Audio engine ----------
void audioInit() {
  analogWriteResolution(12);
  nextSample = micros();
  initWavetables();
  updateDetuneRatio();
  updateGlideAlpha();
  for (auto& v : V) {
    v.active = false;
    v.eg = { 0.005f, 0.08f, 0.7f, 0.25f, 0.0f, false };
    v.unison = 1;
    v.vel = 0.8f;
    for (uint8_t u = 0; u < MAX_UNISON; u++) {
      v.phase[u] = 0;
      v.inc[u] = 0;
      v.incTarget[u] = 0;
    }
  }
  svfUpdateCoeffs();

  smCutoff.setTauMs(30);
  smCutoff.setValue(cutoff);
  smReso.setTauMs(35);
  smReso.setValue(resonance);
  smMorph.setTauMs(25);
  smMorph.setValue(morph);
  smGain.setTauMs(20);
  smGain.setValue(masterGain);
  smGlide.setTauMs(60);
  smGlide.setValue(glideTime);
  smDetune.setTauMs(45);
  smDetune.setValue(unisonDetuneCents);

#if USE_CHORUS
  memset(chBuf, 0, sizeof(chBuf));
  chW = 0;
  chPh1 = 0.0f;
  chPh2 = 0.5f;
#endif

  nextCtrlAt = micros();
  waterfallInit();
}

inline void glideTick(Voice& v) {
  if (glideAlpha > 0.0f) {
    const float k = glideAlpha;
    for (uint8_t u = 0; u < v.unison; ++u)
      v.inc[u] = v.incTarget[u] + (v.inc[u] - v.incTarget[u]) * k;
  }
}

inline float randomNoiseSlow() {
  static uint32_t rng = 0x12345678;
  static uint8_t ctr = 0;
  static float cached = 0.0f;
  if (++ctr == 0) {
    rng = 1664525u * rng + 1013904223u;
    cached = ((int32_t)(rng >> 9) / 8388608.0f) - 1.0f;
  }
  return cached;
}

inline void audioTick() {
  uint32_t now = micros();
  if ((int32_t)(now - nextSample) < 0) return;
  nextSample += SAMPLE_PERIOD;
  const float dt = 1.0f / SAMPLE_RATE;

  const float bend = pitchBendRatio * lfo2Ratio;

#if PER_VOICE_FILTER
  float y = 0.0f;
  for (auto& v : V) {
    if (!v.active && v.eg.env <= 0) continue;

    glideTick(v);

    float sum = 0.0f;
    const float invU = 1.0f / (float)v.unison;
    for (uint8_t u = 0; u < v.unison; ++u) {
      const float s = oscRead(v.phase[u], morph);
      v.phase[u] += v.inc[u] * bend;
      if (v.phase[u] >= 1.0f) v.phase[u] -= 1.0f;
      sum += s;
    }
    sum *= invU;

    adsrTick(v.eg, dt);

    const float vo = v.svf.process(sum) * v.eg.env * v.vel;
    y += vo;

    if (!v.eg.gate && v.eg.env <= 0) v.active = false;
  }

  if (noiseAmt > 0.0001f) {
    static uint32_t rng = 0x12345678;
    rng = 1664525u * rng + 1013904223u;
    const float wn = ((int32_t)(rng >> 9) / 8388608.0f) - 1.0f;
    y += wn * noiseAmt * 0.35f;
  }

#else
  float mix = 0.0f;
  for (auto& v : V) {
    if (!v.active && v.eg.env <= 0) continue;

    glideTick(v);

    float sum = 0.0f;
    const float invU = 1.0f / (float)v.unison;
    for (uint8_t u = 0; u < v.unison; ++u) {
      const float s = oscRead(v.phase[u], morph);
      v.phase[u] += v.inc[u] * bend;
      if (v.phase[u] >= 1.0f) v.phase[u] -= 1.0f;
      sum += s;
    }
    sum *= invU;

    adsrTick(v.eg, dt);
    mix += sum * v.eg.env * v.vel;

    if (!v.eg.gate && v.eg.env <= 0) v.active = false;
  }

  if (noiseAmt > 0.0001f) {
    static uint32_t rng = 0x12345678;
    rng = 1664525u * rng + 1013904223u;
    const float wn = ((int32_t)(rng >> 9) / 8388608.0f) - 1.0f;
    mix += wn * noiseAmt * 0.35f;
  }

  float y = svfProcess(mix);
#endif

  y = dcBlock(y);

  // --- CHORUS (pre-delay)
#if USE_CHORUS
  chBuf[chW] = (int8_t)roundf(clampf(y, -1.0f, 1.0f) * 127.0f);
  auto chTap = [&](float& ph, float rate) -> float {
    ph += rate / SAMPLE_RATE;
    if (ph >= 1.0f) ph -= 1.0f;
    const float lf = sinf(2.0f * PI * ph);
    const float dSamples = (CH_BUF / 3) + lf * chDepth;
    int rd = (int)(chW - (int)dSamples);
    while (rd < 0) rd += CH_BUF;
    return chBuf[rd % CH_BUF] / 127.0f;
  };
  const float t1 = chTap(chPh1, chRate1), t2 = chTap(chPh2, chRate2);
  chW = (uint16_t)((chW + 1) % CH_BUF);
  const float chor = 0.5f * (t1 + t2);
  y = y * (1.0f - chMix) + chor * chMix;
#endif

  // --- DELAY
  if (dlyMix > 0.001f) {
    uint16_t dlySamp;
    if (dlySync) {
      const float bpmUse = (tempoSrc == 1 ? bpmExt : bpmInt);
      const float synced = maxf(0.01f, (60.0f / maxf(1.0f, bpmUse)) / 4.0f);  // 1/16
      dlySamp = (uint16_t)constrain((int)(synced * SAMPLE_RATE), 1, (int)DLY_SIZE - 1);
    } else {
      dlySamp = (uint16_t)constrain((int)(dlyTime * SAMPLE_RATE), 1, (int)DLY_SIZE - 1);
    }
    const uint16_t dlyR = (uint16_t)((dlyW + DLY_SIZE - dlySamp) % DLY_SIZE);
    const float tap = dlyBuf[dlyR] / 127.0f;
    const float fbv = clampf(y + tap * dlyFb, -1.0f, 1.0f);
    dlyBuf[dlyW] = (int8_t)roundf(fbv * 127.0f);
    dlyW = (uint16_t)((dlyW + 1) % DLY_SIZE);
    y = y * (1.0f - dlyMix) + tap * dlyMix;
  }

  // --- Waterfall feed
#if defined(WF_MODE) && (WF_MODE >= 1)
  if (vizMode == VIZ_WATER) {
    int32_t headroom = (int32_t)(nextSample - micros());
    if (headroom > 80) {
      if (++wf_decim_ctr >= WF_DECIM) {
        wf_decim_ctr = 0;
        static int32_t pe_prev_q15 = 0;
        int32_t y_q15 = (int32_t)(y * 32767.0f);
        const int32_t a_q15 = 31875;  // ~0.97
        int32_t x_q15 = y_q15 - (int32_t)(((int64_t)a_q15 * pe_prev_q15) >> 15);
        pe_prev_q15 = y_q15;

        int32_t s = x_q15 >> 8;
        if (s > 127) s = 127;
        else if (s < -128) s = -128;

        if (wf_i < WF_N) {
          wf_frame[wf_i++] = (int8_t)s;
          if (wf_i >= WF_N) wf_ready = true;
        }
      }
    }
  } else {
    wf_i = 0;
    wf_decim_ctr = 0;
    wf_ready = false;
  }
#endif

  // --- OUTPUT
  y = applyOutputFX(y);
  y *= masterGain;
  y = softClip(y);
  y = clampf(y, -1.0f, 1.0f);

  const uint16_t dac = (uint16_t)((y * 0.5f + 0.5f) * 4095.0f);
  analogWrite(A0, dac);

  const float ay = fabsf(y);
  if (ay > vizPeakAccum) vizPeakAccum = ay;

  if (++vizScopeDecim >= VIZ_SCOPE_DECIM) {
    vizScopeDecim = 0;
    vizScope[vizScopeWrite] = (int8_t)roundf(clampf(y, -1.0f, 1.0f) * 63.0f);
    vizScopeWrite = (uint8_t)((vizScopeWrite + 1) % 12);
  }
}

// ---------- LED visualizers ----------
static inline float takeVizPeak() {
  float p = vizPeakAccum;
  vizPeakAccum = 0.0f;
  return clampf(p, 0.0f, 1.0f);
}

static inline uint8_t activeVoiceCount() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < NUM_VOICES; ++i) {
    if (V[i].active || V[i].eg.env > 0.002f) ++n;
  }
  return n;
}

static inline void drawTopPips() {
#if USE_WIFI_STACK
  if (wifiOK) wifiPipHoldUntil = millis() + 1000;
  if (rtpOK) rtpPipHoldUntil = millis() + 1000;
  const bool showWiFi = wifiOK || (int32_t)(millis() - wifiPipHoldUntil) < 0;
  const bool showRTP = rtpOK || (int32_t)(millis() - rtpPipHoldUntil) < 0;
#else
  const bool showWiFi = false, showRTP = false;
#endif

  for (int c = 0; c < 3; ++c) ledBuf[0][c] = showWiFi ? 1 : 0;

#if USE_BLE_MIDI
  updateBLEStatus();
  static uint32_t blePipHoldUntil = 0;
  if (bleOK) blePipHoldUntil = millis() + 1000;
  const bool showBLE = bleOK || (int32_t)(millis() - blePipHoldUntil) < 0;
  ledBuf[0][3] = showBLE ? 1 : 0;
#else
  ledBuf[0][3] = 0;
#endif

  ledBuf[0][4] = showRTP ? 1 : 0;

  const bool showUART = (int32_t)(millis() - uartPipHoldUntil) < 0;
  const bool showUARTRX = (int32_t)(millis() - uartRxPipHoldUntil) < 0;
  ledBuf[0][5] = showUART ? 1 : 0;
  ledBuf[0][6] = showUARTRX ? 1 : 0;

  const bool showRTPRX = (int32_t)(millis() - rtpRxPipHoldUntil) < 0;
  ledBuf[0][8] = showRTPRX ? 1 : 0;
}

void ledsClear() {
  memset(ledBuf, 0, sizeof(ledBuf));
}
void ledsSet(int c, int r, bool on = true) {
  if (r >= 0 && r < 8 && c >= 0 && c < 12) ledBuf[r][c] = on;
}

void drawStatusOverlays() {
  ledBuf[0][11] = ((millis() / 250) & 1);
  for (int r = 0; r < 7; r++) ledBuf[r][10] = 0;
  int mrow = (int)constrain((int)lastMilestone, 0, 6);
  ledBuf[mrow][10] = 1;
  ledBuf[7][11] = ledStalled ? 1 : 0;
}

void ledsRender() {
  matrix.renderBitmap(ledBuf, 8, 12);
}

void ledsWarmupComet() {
  static uint8_t x = 0;
  static uint32_t lastStep = 0;
  const uint32_t stepMs = 120;
  const int row = 3;
  const int tail = 1;

  if ((int32_t)(millis() - matrixHoldUntil) < 0) return;

  uint32_t now = millis();
  if (now - lastStep >= stepMs) {
    lastStep = now;
    x = (uint8_t)((x + 1) % 12);
  }

  ledsClear();
  for (int i = 0; i <= tail; ++i) {
    int c = (int)x - i;
    if (c >= 0 && c <= 11) ledBuf[row][c] = 1;
  }
  drawTopPips();
  drawStatusOverlays();
  ledsRender();
}

void ledsStatusMeters() {
  static float outHold = 0.0f;
  const float outLevel = takeVizPeak();
  outHold = maxf(outHold * 0.82f, outLevel);

  ledsClear();
  if (noteBlink) {
    ledBuf[1][8] = 1;
    ledBuf[1][9] = 1;
    noteBlink--;
  }

  for (int i = 0; i < NUM_VOICES; ++i) {
    const int row = 2 + i;
    const float lvl = clampf(V[i].eg.env, 0.0f, 1.0f);
    const int bars = (int)roundf(lvl * 12.0f);
    for (int c = 0; c < bars && c < 12; ++c) ledBuf[row][c] = 1;
  }

  const int outBars = (int)roundf(outHold * 12.0f);
  for (int c = 0; c < outBars && c < 12; ++c) ledBuf[7][c] = 1;

  drawTopPips();
  drawStatusOverlays();
  ledsRender();
}

void ledsVU() {
  static float hold = 0.0f;
  const float level = takeVizPeak();
  hold = maxf(hold * 0.90f, level);

  ledsClear();

  const int bars = (int)roundf(level * 12.0f);
  for (int c = 0; c < bars && c < 12; ++c) {
    ledsSet(c, 4, true);
    if (bars > 7) ledsSet(c, 5, true);
  }

  const int peak = (int)roundf(hold * 11.0f);
  if (peak >= 0 && peak < 12) {
    ledsSet(peak, 2, true);
    ledsSet(peak, 3, true);
  }

  const uint8_t voices = activeVoiceCount();
  for (uint8_t c = 0; c < voices && c < 4; ++c) ledsSet(c, 7, true);

  drawTopPips();
  drawStatusOverlays();
  ledsRender();
}

void ledsScope() {
  ledsClear();

  const uint8_t write = vizScopeWrite;
  int prevRow = -1;
  for (uint8_t c = 0; c < 12; ++c) {
    const int8_t s = vizScope[(write + c) % 12];
    int row = 4 - (int)roundf((float)s * (3.0f / 63.0f));
    row = (int)constrain(row, 1, 7);

    if (prevRow >= 0) {
      const int a = min(prevRow, row);
      const int b = max(prevRow, row);
      for (int r = a; r <= b; ++r) ledsSet(c, r, true);
    } else {
      ledsSet(c, row, true);
    }
    prevRow = row;
  }

  for (int c = 0; c < 12; c += 3) ledsSet(c, 4, true);

  drawTopPips();
  drawStatusOverlays();
  ledsRender();
}

static void waterfallInit() {
  for (uint8_t k = 0; k < WF_COLS; ++k) {
    float w = 2.0f * PI * (WF_FC[k] / (float)WF_FS);
    wf_coef[k] = 2.0f * cosf(w);
  }
  memset(wf_s1, 0, sizeof(wf_s1));
  memset(wf_s2, 0, sizeof(wf_s2));
  memset(wf_thresh, 0, sizeof(wf_thresh));
  wf_decim_ctr = 0;
  wf_i = 0;
  wf_ready = false;
}

static inline void waterfallProcessIfReady() {
  if (!wf_ready) return;
  wf_ready = false;

  for (uint8_t c = 0; c < WF_COLS; ++c) { wf_s1[c] = wf_s2[c] = 0.0f; }

  for (uint16_t n = 0; n < WF_N; ++n) {
    float x = wf_frame[n];
    for (uint8_t c = 0; c < WF_COLS; ++c) {
      float s0 = x + wf_coef[c] * wf_s1[c] - wf_s2[c];
      wf_s2[c] = wf_s1[c];
      wf_s1[c] = s0;
    }
  }

  for (uint8_t c = 0; c < WF_COLS; ++c) {
    float p = wf_s1[c] * wf_s1[c] + wf_s2[c] * wf_s2[c] - wf_coef[c] * wf_s1[c] * wf_s2[c];
    p = p / (float)(WF_N * WF_N);
    float mag = clampf(sqrtf(maxf(0.0f, p)), 0.0f, 1.5f);

    wf_bins[c] = mag;

    float base = wf_thresh[c];
    base = (1.0f - WF_ALPHA) * base + WF_ALPHA * mag;
    wf_thresh[c] = base;
  }

  wf_i = 0;
}

void ledsWater() {
  const float level = takeVizPeak();

  for (int r = 7; r > 1; --r)
    for (int c = 0; c < 12; ++c)
      ledBuf[r][c] = ledBuf[r - 1][c];

  for (int c = 0; c < 12; ++c) ledBuf[1][c] = 0;

#if defined(WF_MODE) && (WF_MODE >= 1)
  for (uint8_t c = 0; c < WF_COLS; ++c) {
    float thr = WF_GAIN * maxf(0.02f, wf_thresh[c]);
    if (wf_bins[c] > thr) ledBuf[1][c] = 1;
  }
#else
  const int halfBars = (int)roundf(level * 6.0f);
  for (int i = 0; i < halfBars; ++i) {
    ledsSet(5 - i, 1, true);
    ledsSet(6 + i, 1, true);
  }

  const uint8_t voices = activeVoiceCount();
  for (uint8_t i = 0; i < voices && i < 4; ++i) ledsSet(i, 1, true);
#endif

  drawTopPips();
  drawStatusOverlays();
  ledsRender();
}

void ledsUpdate() {
  if (!uartEverConnected) {
    ledsWarmupComet();
    return;
  }
  if ((int32_t)(millis() - matrixHoldUntil) < 0) return;

  static uint32_t _lastLEDTick = 0;
  uint32_t now = millis();
  ledStalled = (_lastLEDTick != 0 && now - _lastLEDTick > 200);
  _lastLEDTick = now;

  switch (vizMode) {
    case VIZ_STATUS: ledsStatusMeters(); break;
    case VIZ_VU: ledsVU(); break;
    case VIZ_SCOPE: ledsScope(); break;
    case VIZ_WATER: ledsWater(); break;
  }
}

void matrixScrollOnce(const char* s, uint16_t ms = 1800, uint32_t color = 0xFFFFFF) {
  matrix.beginDraw();
  matrix.clear();
  matrix.textFont(Font_5x7);
  matrix.textScrollSpeed(50);
  matrix.beginText(0, 1, color);
  matrix.print(s);
  matrix.endText(SCROLL_LEFT);
  matrix.endDraw();
  matrixHoldUntil = millis() + ms;
}

// ---------- UART Command Processing ----------
void processUARTCommand(const char* cmd) {
  uartDoc.clear();
  DeserializationError jerr = deserializeJson(uartDoc, cmd);
  if (jerr) return;
  if (!uartDoc.is<JsonObject>()) return;

  uartRxPipHoldUntil = millis() + 400;

  const char* t = uartDoc["t"] | "";
  if (!t || !t[0]) return;

  // Heartbeat
  if (strcmp(t, "ping") == 0) return;

  LOG_PRINT(F("[processUARTCommand] [UART] cmd: "));
  LOG_PRINTLN(t);

  // Batched CCs
  if (strcmp(t, "ccs") == 0) {
    JsonArray items = uartDoc["items"].as<JsonArray>();
    if (items.isNull() || items.size() == 0) return;

    static uint32_t lastBatchAt = 0;
    const uint32_t nowMs = millis();
    if (nowMs - lastBatchAt < UART_BATCH_MIN_GAP_MS) return;
    lastBatchAt = nowMs;

    uint8_t handled = 0;
    const uint32_t tStart = micros();

    for (JsonObject it : items) {
      int id = it["id"] | -1;
      int v = it["v"] | -1;
      if (id < 0 || id > 127 || v < 0 || v > 127) continue;

      const uint32_t ms = millis();
      if (g_lastCcVal[id] == (uint8_t)v && (ms - g_lastCcMs[id] < CC_MIN_GAP_MS)) continue;
      g_lastCcVal[id] = (uint8_t)v;
      g_lastCcMs[id] = ms;

      handleCC((uint8_t)id, (uint8_t)v);

      if (++handled >= UART_MAX_ITEMS) break;
      if (micros() - tStart > UART_MAX_US) break;
    }
    return;
  }

  // Single CC
  if (strcmp(t, "cc") == 0) {
    static uint32_t lastCCAt = 0;
    const uint32_t nowMs = millis();
    if (nowMs - lastCCAt < UART_SINGLE_CC_MIN_GAP_MS) return;
    lastCCAt = nowMs;

    int id = uartDoc["id"] | -1;
    int v = uartDoc["v"] | -1;
    if (id < 0 || id > 127 || v < 0 || v > 127) return;

    if (g_lastCcVal[id] == (uint8_t)v && (nowMs - g_lastCcMs[id] < CC_MIN_GAP_MS)) return;
    g_lastCcVal[id] = (uint8_t)v;
    g_lastCcMs[id] = nowMs;

    handleCC((uint8_t)id, (uint8_t)v);
    return;
  }

  // Other commands
  if (strcmp(t, "noteon") == 0) {
    synthNoteOn((uint8_t)(uartDoc["n"] | 60), (uint8_t)(uartDoc["v"] | 100));
  } else if (strcmp(t, "noteoff") == 0) {
    synthNoteOff((uint8_t)(uartDoc["n"] | 60));
  } else if (strcmp(t, "unison") == 0) {
    int v = uartDoc["v"] | 1;
    unisonCount = (uint8_t)constrain(v, 1, 3);
  } else if (strcmp(t, "viz") == 0) {
    int v = uartDoc["v"] | 0;
    vizMode = (VizMode)constrain(v, 0, 3);
    noteBlink = 4;
  } else if (strcmp(t, "tempo") == 0) {
    float b = uartDoc["v"] | 120.0f;
    if (b >= 20.0f && b <= 300.0f) {
      bpmInt = b;
    }
  } else if (strcmp(t, "tempo_src") == 0) {
    tempoSrc = (uint8_t)(uartDoc["v"] | 0) ? 1 : 0;
  } else if (strcmp(t, "mod") == 0) {
    if (uartDoc.containsKey("morph")) lfoToMorph = clampf(uartDoc["morph"], 0.0f, 1.0f);
    if (uartDoc.containsKey("amp")) lfoToAmp = clampf(uartDoc["amp"], 0.0f, 1.0f);
  } else if (strcmp(t, "dlysync") == 0) {
    dlySync = ((int)(uartDoc["v"] | 0) != 0);
  } else if (strcmp(t, "rand") == 0) {
    smCutoff.setTarget(40.0f + 10000.0f * (random(0, 1000) / 1000.0f));
    smReso.setTarget(0.5f + 2.2f * (random(0, 1000) / 1000.0f));
    smMorph.setTarget(random(0, 1000) / 1000.0f);
    smGlide.setTarget(0.01f * (random(0, 300) / 100.0f));
    smDetune.setTarget(2.0f + 16.0f * (random(0, 1000) / 1000.0f));
    unisonCount = 1 + (random(0, 3));
    masterGain = 0.5f + 0.5f * (random(0, 1000) / 1000.0f);
    noiseAmt = (random(0, 1000) / 1000.0f) * 0.5f;
    dlyTime = 0.02f + 0.166f * (random(0, 1000) / 1000.0f);
    dlyFb = 0.01f + 0.89f * (random(0, 1000) / 1000.0f);
    dlyMix = (random(0, 1000) / 1000.0f) * 0.5f;
    crushAmt = (random(0, 1000) / 1000.0f) * 0.45f;
    tremRateHz = 0.2f + 12.0f * (random(0, 1000) / 1000.0f);
    tremDepth = (random(0, 1000) / 1000.0f) * 0.55f;
    driveAmt = (random(0, 1000) / 1000.0f) * 0.55f;
    foldAmt = (random(0, 1000) / 1000.0f) * 0.35f;
    lfoAmtHz = 0.0f + 2000.0f * (random(0, 1000) / 1000.0f);
    lfoRateHz = 0.2f + 10.0f * (random(0, 1000) / 1000.0f);
    velToCutoff = (random(0, 1000) / 1000.0f) * 0.8f;
    noiseToCutoff = (random(0, 1000) / 1000.0f) * 0.6f;
    lfoToDetune = (random(0, 1000) / 1000.0f) * 0.6f;
    sendStateToUART();
    return;
  } else if (strcmp(t, "set") == 0) {
    const char* key = uartDoc["key"] | "";

    if (!strcmp(key, "velMode")) {
      velMode = (VelCurveMode)(uint8_t)(uartDoc["v"] | 0);
    } else if (!strcmp(key, "steal")) {
      stealMode = (VoiceSteal)(uint8_t)(uartDoc["v"] | 0);
    } else if (!strcmp(key, "arp")) {
      if (uartDoc.containsKey("mode")) arpMode = (ArpMode)(uint8_t)(uartDoc["mode"] | 0);
      if (uartDoc.containsKey("div")) arpDiv = (uint8_t)(uartDoc["div"] | 2);
      if (uartDoc.containsKey("gate")) arpGate = (uint8_t)(uartDoc["gate"] | 60);
    } else if (!strcmp(key, "chorus")) {
      if (uartDoc.containsKey("mix")) chMix = clampf(uartDoc["mix"] | chMix, 0, 1);
      if (uartDoc.containsKey("depth")) chDepth = (float)(uartDoc["depth"] | chDepth);
    } else if (!strcmp(key, "lfo2")) {
      if (uartDoc.containsKey("rate")) {
        uint8_t raw = (uint8_t)(uartDoc["rate"] | 64);
        lfo2RateHz = 0.05f + (20.0f - 0.05f) * (raw / 127.0f);
      }
      if (uartDoc.containsKey("amt")) {
        uint8_t raw = (uint8_t)(uartDoc["amt"] | 10);
        lfo2AmtSemi = 2.0f * (raw / 127.0f);
      }
      if (uartDoc.containsKey("wave")) lfo2Wave = (LfoWave)(uint8_t)(uartDoc["wave"] | 0);
    } else if (!strcmp(key, "mod")) {
      if (uartDoc.containsKey("velCut")) velToCutoff = clampf(uartDoc["velCut"], 0, 1);
      if (uartDoc.containsKey("noiseCut")) noiseToCutoff = clampf(uartDoc["noiseCut"], 0, 1);
      if (uartDoc.containsKey("lfoDet")) lfoToDetune = clampf(uartDoc["lfoDet"], 0, 1);
    }

    sendStateToUART();
    return;
  } else if (strcmp(t, "lfo2") == 0) {
    const char* k = uartDoc["k"] | "";
    uint8_t raw = (uint8_t)(uartDoc["v"] | 0);
    if (!strcmp(k, "rate")) {
      lfo2RateHz = 0.05f + (20.0f - 0.05f) * (raw / 127.0f);
    } else if (!strcmp(k, "amt")) {
      lfo2AmtSemi = 2.0f * (raw / 127.0f);
    } else if (!strcmp(k, "wave")) {
      lfo2Wave = (LfoWave)raw;
    }
    sendStateToUART();
    return;
  } else if (strcmp(t, "getstate") == 0) {
    sendStateToUART();
  }
}

// ---------- Wi-Fi / BLE / MIDI init ----------
#if USE_WIFI_STACK
static void wifiKick(const char* reason) {
  Serial.print(F("[WiFi] begin ("));
  Serial.print(reason);
  Serial.print(F(") SSID="));
  Serial.println(WIFI_SSID);
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

void wifiMaintain() {
  static uint32_t nextTry = 0;
  if (millis() < nextTry) return;

  wl_status_t st = (wl_status_t)WiFi.status();
  if (st != WL_CONNECTED || WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
    if (wifiOK) Serial.println(F("[WiFi] lost, retrying..."));
    wifiOK = false;
    WiFi.disconnect();
    wifiKick("maint");
    nextTry = millis() + 3000;
    return;
  }

  if (!wifiOK) {
    wifiOK = true;
    Serial.print(F("[WiFi] connected: "));
    Serial.println(WiFi.localIP());
#if USE_RTP_MIDI
    if (!rtpOK) setupRTP();
#endif
    sendStateToUART();
  }
}

void wifiStart() {
  Serial.println(F("[WiFi] starting..."));
  wifiKick("boot");

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 20000) {
    ledsWarmupComet();
    if (((millis() - t0) % 1000) < 15) Serial.print('.');
    delay(10);
  }
  Serial.println();

  IPAddress ip(0, 0, 0, 0);
  uint32_t t1 = millis();
  while (WiFi.status() == WL_CONNECTED && ip == IPAddress(0, 0, 0, 0) && millis() - t1 < 5000) {
    ip = WiFi.localIP();
    ledsWarmupComet();
    delay(10);
  }

  wifiOK = (WiFi.status() == WL_CONNECTED && ip != IPAddress(0, 0, 0, 0));
  delay(10);
  if (wifiOK) {
    Serial.print(F("[WiFi] connected: "));
    Serial.println(ip);
    matrixScrollOnce(ip.toString().c_str(), 1800, 0xFFFFFF);
  } else {
    Serial.println(F("[WiFi] failed to get IP"));
    matrixScrollOnce("No WiFi", 1500, 0xFF0000);
  }
  matrix.endDraw();
}
#endif

// DIN handlers
void setupDIN() {
  DIN_MIDI.begin(MIDI_CHANNEL_OMNI);
  DIN_MIDI.setHandleNoteOn([](byte, byte n, byte v) {
    touchRtpRx();
    synthNoteOn(n, v);
  });
  DIN_MIDI.setHandleNoteOff([](byte, byte n, byte) {
    touchRtpRx();
    synthNoteOff(n);
  });
  DIN_MIDI.setHandleControlChange([](byte, byte cc, byte v) {
    touchRtpRx();
    handleCC(cc, v);
  });
  DIN_MIDI.setHandlePitchBend([](byte, int bend) {
    touchRtpRx();
    setPitchBend((int16_t)(bend + 8192));
  });
  DIN_MIDI.setHandleClock([]() {
    touchRtpRx();
    uint32_t now = millis();
    if (_clkCnt == 0) _clkLast = now;
    _clkCnt = (uint8_t)((_clkCnt + 1) % 24);
    if (_clkCnt == 0) {
      uint32_t dt = now - _clkLast;
      if (dt > 0) bpmExt = 60000.0f / (float)dt;
      _clkLast = now;
    }
  });
}

#if USE_RTP_MIDI
static bool rtpInitDone = false;
void setupRTP() {
  if (rtpInitDone) return;
  rtpInitDone = true;
  rtpMIDI.begin(MIDI_CHANNEL_OMNI);
  rtpMIDI.setHandleNoteOn([](byte, byte n, byte v) {
    touchRtpRx();
    synthNoteOn(n, v);
  });
  rtpMIDI.setHandleNoteOff([](byte, byte n, byte) {
    touchRtpRx();
    synthNoteOff(n);
  });
  rtpMIDI.setHandleControlChange([](byte, byte cc, byte v) {
    touchRtpRx();
    handleCC(cc, v);
  });
  rtpMIDI.setHandlePitchBend([](byte, int bend) {
    touchRtpRx();
    setPitchBend((int16_t)(bend + 8192));
  });
  rtpMIDI.setHandleClock([]() {
    touchRtpRx();
    uint32_t now = millis();
    if (_clkCnt == 0) _clkLast = now;
    _clkCnt = (uint8_t)((_clkCnt + 1) % 24);
    if (_clkCnt == 0) {
      uint32_t dt = now - _clkLast;
      if (dt > 0) bpmExt = 60000.0f / (float)dt;
      _clkLast = now;
    }
  });

  ApplertpMIDI.setHandleConnected([](const APPLEMIDI_NAMESPACE::ssrc_t&, const char*) {
    rtpOK = true;
    Serial.println(F("[RTP] Connected"));
    uartEverConnected = true;
    uartPipHoldUntil = millis() + 1500;
    noteBlink = 10;
    matrixScrollOnce("RTP Connected", 1200, 0xFFFFFF);
  });

  ApplertpMIDI.setHandleDisconnected([](const APPLEMIDI_NAMESPACE::ssrc_t&) {
    rtpOK = false;
    Serial.println(F("[RTP] Disconnected"));
  });
}
#endif

// ---- BLE-MIDI via MIDI Pipes ----
#if USE_BLE_MIDI
class BLESink : public cs::MIDI_Sink {
public:
  void sinkMIDIfromPipe(cs::ChannelMessage msg) override {
    using cs::MIDIMessageType;
    switch (msg.getMessageType()) {
      case MIDIMessageType::NoteOn:
        {
          uint8_t n = msg.getData1(), v = msg.getData2();
          if (v) synthNoteOn(n, v);
          else synthNoteOff(n);
          break;
        }
      case MIDIMessageType::NoteOff: synthNoteOff(msg.getData1()); break;
      case MIDIMessageType::ControlChange: handleCC(msg.getData1(), msg.getData2()); break;
      case MIDIMessageType::PitchBend: setPitchBend((int16_t)(msg.getData14bit())); break;
      default: break;
    }
  }
  void sinkMIDIfromPipe(cs::SysExMessage) override {}
  void sinkMIDIfromPipe(cs::SysCommonMessage) override {}
  void sinkMIDIfromPipe(cs::RealTimeMessage) override {}
};
static BLESink bleSink;
static cs::MIDI_PipeFactory<1> ble_pipes;
void setupBLE() {
  BLE_MIDI.begin();
  BLE_MIDI >> ble_pipes >> bleSink;
}
#endif

// ---------- Boot / control ----------
void bootAnim() {
  for (int step = 0; step < 6; ++step) {
    int L = 5 - step, R = 6 + step;
    if (L >= 0)
      for (int r = 0; r < 8; ++r) ledBuf[r][L] = 1;
    if (R < 12)
      for (int r = 0; r < 8; ++r) ledBuf[r][R] = 1;
    matrix.renderBitmap(ledBuf, 8, 12);
    memset(ledBuf, 0, sizeof(ledBuf));
    delay(60);
  }
  for (int r = 7; r >= 0; --r) {
    for (int rr = 7; rr >= r; --rr)
      for (int c = 0; c < 12; ++c) ledBuf[rr][c] = 1;
    matrix.renderBitmap(ledBuf, 8, 12);
    memset(ledBuf, 0, sizeof(ledBuf));
    delay(40);
  }
  for (int frame = 0; frame < 24; ++frame) {
    memset(ledBuf, 0, sizeof(ledBuf));
    for (int c = 0; c < 12; ++c) {
      int r = (int)(3.5f + 3.5f * sinf((frame * 0.4f) + c * 0.7f));
      ledBuf[r][c] = 1;
    }
    matrix.renderBitmap(ledBuf, 8, 12);
    delay(40);
  }
  matrix.beginDraw();
  matrix.textFont(Font_5x7);
  matrix.textScrollSpeed(60);
  matrix.beginText(0, 1, 0xFFFFFF);
  matrix.print("QuarkWave");
  matrix.endText(SCROLL_LEFT);
  matrix.endDraw();
  delay(1500);
}

// ---------- Control-rate tick ----------
inline float maxVoiceVelocity() {
  float m = 0.0f;
  for (auto& v : V)
    if (v.active) m = maxf(m, v.vel);
  return m;
}

inline void controlTick() {
  // --- LFO1 (modulation)
  float lfoRate = lfoRateHz;
  const float bpmUse = (tempoSrc == 1 ? bpmExt : bpmInt);
  if (lfoSync && bpmUse > 1.0f) lfoRate = bpmUse / 60.0f;

  lfoPhase += (lfoRate * CTRL_DT);
  if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
  const float lfo = sinf(2.0f * PI * lfoPhase);

  // --- LFO2 (vibrato)
  lfo2Phase += (lfo2RateHz * CTRL_DT);
  if (lfo2Phase >= 1.0f) lfo2Phase -= 1.0f;
  float l2;
  switch (lfo2Wave) {
    case LFO_TRI: l2 = 2.0f * fabsf(2.0f * (lfo2Phase - floorf(lfo2Phase + 0.5f))) - 1.0f; break;
    case LFO_SQR: l2 = (lfo2Phase < 0.5f) ? 1.0f : -1.0f; break;
    default: l2 = sinf(2.0f * PI * lfo2Phase); break;
  }
  lfo2Ratio = powf(2.0f, (lfo2AmtSemi * l2) / 12.0f);

  tremPhase += (tremRateHz * CTRL_DT);
  if (tremPhase >= 1.0f) tremPhase -= 1.0f;
  tremGain = 1.0f - tremDepth * (0.5f + 0.5f * sinf(2.0f * PI * tremPhase));

  // --- Step smoothers
  smCutoff.step();
  smReso.step();
  smMorph.step();
  smGain.step();
  smGlide.step();
  smDetune.step();

  // --- Cutoff base
  const float cutBase = smCutoff.y + lfo * lfoAmtHz;

  bool needSVF = false;

#if !PER_VOICE_FILTER
  const float velMod = velToCutoff * maxVoiceVelocity();
  const float noiseMod = noiseToCutoff * randomNoiseSlow();
  const float modCut = clampf(cutBase + velMod + noiseMod, 20.0f, SAMPLE_RATE * 0.45f);
  if (fabsf(modCut - cutoff) > CUTOFF_EPS) {
    cutoff = modCut;
    needSVF = true;
  }
#else
  const float modCut = clampf(cutBase, 20.0f, SAMPLE_RATE * 0.45f);
  if (fabsf(modCut - cutoff) > CUTOFF_EPS) {
    cutoff = modCut;
    needSVF = true;
  }

  const float noiseCtrl = noiseToCutoff * randomNoiseSlow();
  const float alpha = (SVF_CUTOFF_TAU_MS <= 0.01f) ? 1.0f
                                                   : (1.0f - expf(-CTRL_DT / (SVF_CUTOFF_TAU_MS * 0.001f)));

  for (uint8_t i = 0; i < NUM_VOICES; ++i) {
    Voice& v = V[i];

    const float velModV = velToCutoff * (v.active ? v.vel : 0.0f);
    const float tgt = clampf(cutBase + velModV + noiseCtrl, 20.0f, SAMPLE_RATE * 0.45f);
    v.cfT = tgt;

    v.cfY += (v.cfT - v.cfY) * alpha;

    if (v.active || v.eg.env > 0.001f) {
      if (fabsf(v.cfY - v.svf.cutoff) > 0.5f || fabsf(smReso.y - v.svf.Q) > 0.005f) {
        v.svf.cutoff = v.cfY;
        v.svf.Q = smReso.y;
        v.svf.update(SAMPLE_RATE);
      }
    }
  }
#endif

  if (fabsf(smReso.y - resonance) > RESO_EPS) {
    resonance = smReso.y;
    needSVF = true;
  }
  if (!PER_VOICE_FILTER && needSVF) svfUpdateCoeffs();

  // --- Mini mod-matrix
  morph = clampf(smMorph.y + lfo * lfoToMorph, 0.0f, 1.0f);
  masterGain = clampf(smGain.y * (1.0f + 0.8f * lfoToAmp * lfo), 0.0f, 2.0f);

  // --- Glide / Detune updates
  if (fabsf(smGlide.y - glideTime) > GLIDE_EPS) {
    glideTime = smGlide.y;
    updateGlideAlpha();
  }
  if (fabsf(smDetune.y - unisonDetuneCents) > DETUNE_EPS) {
    unisonDetuneCents = smDetune.y;
    updateDetuneRatio();
  }

  // ===================== Arpeggiator =====================
  if (arpMode != 0) {
    auto beatsPerStep = [](uint8_t div) -> float {
      switch (div) {
        default:
        case 0: return 1.0f;
        case 1: return 0.5f;
        case 2: return 0.25f;
        case 3: return 0.125f;
        case 4: return 0.0625f;
        case 5: return 1.0f / 6.0f;
        case 6: return 1.0f / 12.0f;
        case 7: return 1.0f / 8.0f;
      }
    };

    static float stepTimer = 0.0f;
    static float gateTimer = 0.0f;
    static int8_t dir = +1;
    static int idx = -1;
    static int lastNote = -1;

    uint8_t pool[128];
    int nPool = 0;
    for (int n = 0; n < 128; ++n)
      if (heldNotes[n]) pool[nPool++] = (uint8_t)n;

    if (nPool == 0) {
      if (lastNote >= 0) {
        synthNoteOff((uint8_t)lastNote);
        lastNote = -1;
      }
      stepTimer = gateTimer = 0.0f;
      idx = -1;
      dir = +1;
      return;
    }

    const float spb = (bpmUse > 1.0f) ? (60.0f / bpmUse) : (60.0f / 120.0f);
    const float stepDur = beatsPerStep((uint8_t)min<uint8_t>(arpDiv, 7)) * spb;
    const float gateDur = clampf((arpGate / 100.0f) * stepDur, 0.01f, stepDur);

    stepTimer += CTRL_DT;
    gateTimer += CTRL_DT;

    if (lastNote >= 0 && gateTimer >= gateDur) {
      synthNoteOff((uint8_t)lastNote);
      lastNote = -1;
    }

    if (stepTimer >= stepDur) {
      stepTimer -= stepDur;
      gateTimer = 0.0f;

      if (idx < 0) {
        idx = 0;
        dir = +1;
      }
      switch (arpMode) {
        case 1: idx = (idx + 1) % nPool; break;
        case 2: idx = (idx - 1 + nPool) % nPool; break;
        case 3:
          {
            idx += dir;
            if (idx >= nPool) {
              dir = -1;
              idx = max(0, nPool - 2);
            } else if (idx < 0) {
              dir = +1;
              idx = (nPool > 1 ? 1 : 0);
            }
          }
          break;
        case 4: idx = (int)random(nPool); break;
        default: idx = (idx + 1) % nPool; break;
      }

      const int nextNote = pool[idx];
      if (lastNote >= 0) synthNoteOff((uint8_t)lastNote);
      synthNoteOn((uint8_t)nextNote, 100);
      lastNote = nextNote;
    }
  }
}

void setup() {
  vizMode = VIZ_STATUS;
  Serial.begin(115200);   // USB for debug
  Serial1.begin(115200);  // Hardware UART for UI
  delay(200);

  // Intro
  Serial.print(F("QuarkWave\n"));
  Serial.print(F("Web Synthesis Sound Module\n"));
  Serial.print(F("(C) 2025 Iain Bennett\n"));
  Serial.print(F("---------------------------------\n"));

  // Build flags
  Serial.print(F("[BOOT] Build flags  WIFI="));
  Serial.print(USE_WIFI_STACK);
  Serial.print(F("  RTP="));
  Serial.print(USE_RTP_MIDI);
  Serial.print(F("  BLE="));
  Serial.println(USE_BLE_MIDI);

  pinMode(LED_BUILTIN, OUTPUT);
  matrix.begin();
  delay(1000);
  bootAnim();
  audioInit();

#if USE_WIFI_STACK
  wifiStart();
  if (wifiOK) {
    sendStateToUART();
  }
#endif

#if USE_BLE_MIDI
  setupBLE();
#endif

  setupDIN();

#if USE_RTP_MIDI
  setupRTP();
#endif

  Serial.println(F("[BOOT] Ready for UART commands on Serial1"));
  uartEverConnected = true;
}

// ===================== Main loop =====================
void loop() {
  const uint32_t now = millis();
  static uint32_t tLED = 0;

  // ----- AUDIO FIRST -----
  audioTick();

  // ----- DIN MIDI -----
  DIN_MIDI.read();

  // ----- RTP-MIDI (if enabled) -----
#if USE_RTP_MIDI
#if USE_WIFI_STACK
  if (wifiOK) rtpMIDI.read();
#else
  rtpMIDI.read();
#endif
  {
    bool up = rtpIsUp();
    static bool _rtpWasUp_local = false;
    if (up != _rtpWasUp_local) {
      _rtpWasUp_local = up;
      rtpOK = up;
      Serial.println(up ? F("[RTP] Connected") : F("[RTP] Disconnected"));
    }
  }
#endif

  // ----- CONTROL-RATE (1 kHz) -----
  {
    uint32_t nowUs = micros();
    while ((int32_t)(nowUs - nextCtrlAt) >= 0) {
      controlTick();
      nextCtrlAt += (1000000u / CONTROL_RATE);
      waterfallProcessIfReady();
    }
  }

  // ----- BLE MIDI (if any) -----
#if USE_BLE_MIDI
  BLE_MIDI.update();
#endif

  // ----- WIFI MAINTENANCE -----
#if USE_WIFI_STACK
  wifiMaintain();
#endif

  // ----- UART COMMAND PROCESSING -----
  if (Serial1.available()) {
    static size_t bufPos = 0;
    
    while (Serial1.available() && bufPos < sizeof(uartBuf) - 1) {
      char c = Serial1.read();
      uartPipHoldUntil = millis() + 300;
      
      if (c == '\n' || c == '\r') {
        if (bufPos > 0) {
          uartBuf[bufPos] = '\0';
          processUARTCommand(uartBuf);
          bufPos = 0;
        }
      } else {
        uartBuf[bufPos++] = c;
      }
    }
    
    // Prevent buffer overflow
    if (bufPos >= sizeof(uartBuf) - 1) {
      bufPos = 0;
      Serial.println(F("[UART] Buffer overflow, command discarded")); // Debug to USB
    }
  }

  // ----- LEDs at ~30 FPS -----
  if (now - tLED > 33) {
    tLED = now;
    ledsUpdate();
  }
}
