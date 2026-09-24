#pragma GCC optimize ("O3")  // Time-critical Uno synthesis and control code; no fast-math.
/*  QuarkWave - Uno R4 WiFi Synth (MIDI controlled version)
   - Poly: 4 voices, unison 1..3 (per voice)
   - Osc: sine/tri/saw/square with morph
   - Filter: 2-pole TPT SVF (cutoff+resonance)
   - Portamento (glide)
   - MIDI: receives CC, Program Change, SysEx, and notes from QuarkWave_UI
   - LED matrix: status, VU, and scope

   Extras:
   - Pitch bend (±2 semitones; DIN/Pico gateway/BLE)
   - White-noise layer (CC93)
   - Simple delay/echo (CC12 time, CC13 feedback, CC14 mix)
   - MIDI Clock (24 PPQN) → LFO sync; CC3 toggles sync
   - Panic (UART + MIDI CC120/123)
*/

// ---------- User config (must come first) ----------
#define SAMPLE_RATE 22050
//#define SAMPLE_RATE 44100

#define USE_CHORUS 1
#define NUM_VOICES 4
#define MAX_UNISON 3
#define PER_VOICE_FILTER 0  // 0 = global SVF, 1 = per-voice SVF
#define USE_RTP_MIDI 0  // Network MIDI is received by the optional Pico gateway.
#define BLE_DEVICE_NAME "QuarkWave"
#define USE_BLE_MIDI 0
#define USE_WIFI_STACK 0  // Keep Wi-Fi polling out of the audio loop.
#ifndef DEBUG_MIDI_EVENTS
#define DEBUG_MIDI_EVENTS 0
#endif

#if !USE_WIFI_STACK && USE_RTP_MIDI
#undef USE_RTP_MIDI
#define USE_RTP_MIDI 0
#pragma message("RTP-MIDI disabled because USE_WIFI_STACK=0")
#endif

#include <Arduino.h>  // make Arduino typedefs available early
#include <stdint.h>   // (optional) for uint16_t, etc., if you prefer
#include <stddef.h>   // (optional) for size_t
#include <ArduinoGraphics.h>
#include <Arduino_LED_Matrix.h>
#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#include <MIDI.h>
#include "secrets.h"

#if USE_WIFI_STACK
#include <WiFiS3.h>
#include <WiFiUdp.h>
#endif

// --- Forward declarations so Arduino's auto-prototypes know these types ---
struct Voice;
struct ADSR;
enum ArpMode : uint8_t;
enum ClockInput : uint8_t;
struct MidiClockState;
static void setArpMode(ArpMode nextMode);

// --- Forward decls for helpers used before their definitions ---
inline float clampf(float x, float a, float b);
inline float fastLerp(float a, float b, float t);
inline float maxf(float a, float b);
inline float randomNoiseSlow();

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
  float g, R, a1, a2, a3;
  float ic1eq = 0, ic2eq = 0;
  float cutoff = 2000.0f, Q = 0.9f;
  bool enabled = true;

  inline void update(float fs) {
    float fc = clampf(cutoff, 20.0f, fs * 0.45f);
    float q = clampf(Q, 0.5f, 3.0f);
    g = tanf(PI * fc / fs);
    R = 1.0f / q;
    a1 = 1.0f / (1.0f + g * (g + R));
    a2 = g * a1;
    a3 = g * a2;
  }
  inline void setCutoff(float fc) {
    cutoff = fc;
    update(SAMPLE_RATE);
  }
  inline float process(float x) {
    if (!enabled) return x;
    const float v3 = x - ic2eq;
    const float v1 = a1 * ic1eq + a2 * v3;
    const float lp = ic2eq + a2 * ic1eq + a3 * v3;
    ic1eq = 2.0f * v1 - ic1eq;
    ic2eq = 2.0f * lp - ic2eq;
    ic1eq = zapDenorm(ic1eq);
    ic2eq = zapDenorm(ic2eq);
    return lp;
  }
} svf;

struct ADSR {
  float a, d, s, r, env;
  bool gate;
  bool decaying;
  float aStep = 0.0f, dStep = 0.0f, rStep = 0.0f;
};
struct Voice {
  bool active;
  uint8_t note;
  // Q0.32 oscillator phase and control-rate phase increment.
  uint32_t phase[MAX_UNISON], phaseStep[MAX_UNISON];
  float inc[MAX_UNISON], incTarget[MAX_UNISON];
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
static uint32_t matrixHoldUntil = 0;
static char matrixScrollText[24] = {0};
static int16_t matrixScrollX = 0, matrixScrollEndX = 0;
static uint32_t matrixScrollNextMs = 0;
static uint16_t matrixScrollHoldMs = 0;
static uint32_t matrixScrollColor = 0xFFFFFF;
static bool matrixScrollActive = false;
// A UART being initialized does not prove that a Pico is attached.
static bool picoLinkSeen = false;
static uint32_t lastPicoHelloMs = 0;
static bool standaloneTouched = false;
static bool picoPatchApplied = false;
static bool picoLinkActive() {
  return picoLinkSeen && (uint32_t)(millis() - lastPicoHelloMs) < 3500;
}
static void sendPicoLinkStatus(uint8_t command = 0x11);
static void markStandaloneActivity();
static uint32_t rtpRxPipHoldUntil = 0;
inline void touchRtpRx() {
  rtpRxPipHoldUntil = millis() + 300;
}

#if USE_BLE_MIDI
volatile bool bleOK = false;
#endif

static uint32_t uartRxPipHoldUntil = 0;
inline void touchUartRx() {
  uartRxPipHoldUntil = millis() + 200;
}

static bool ledStalled = false;

// Lightweight actual-output meters for LED visualizers.
static float vizLevel = 0.0f;
static float vizPeak = 0.0f;
static uint8_t vuHistory[11] = { 0 }; // Oldest at left, newest beside the peak column.
static const float VU_DISPLAY_GAIN = 2.5f;
static const float SCOPE_TARGET_PEAK = 0.90f;
static const float SCOPE_MAX_GAIN = 24.0f;
static const float SCOPE_SILENCE_LEVEL = 0.012f;
static int8_t scopeTrace[12] = { 0 };
static uint8_t scopeWrite = 0;
static uint16_t scopeDecim = 0;

// ---------- Globals (audio / synth) ----------
static const uint16_t WT_SIZE = 256;
static int16_t WT_SINE[WT_SIZE], WT_SAW[WT_SIZE];

static float masterGain = 0.7f;
static float glideTime = 0.03f;
static uint8_t unisonCount = 1;
static float unisonDetuneCents = 8;

static float detuneRatio = 1.0f;
static float lfoDetuneRatio = 1.0f;
static float glideAlpha = 0.0f;

// --- Pitch bend ---
static float pitchBendRatio = 1.0f;
static uint8_t pitchBendRange = 2;
inline void setPitchBend(int16_t v) {
  float semi = ((float)v - 8192.0f) * pitchBendRange / 8192.0f;
  pitchBendRatio = powf(2.0f, semi / 12.0f);
}

// Lightweight insert FX
static float bitcrushMix = 0.0f;
static float bitcrushBits = 16.0f;
static float bitcrushRateDiv = 1.0f;
// Derived settings are refreshed by controlTick, never inside audioTick.
static uint8_t bitcrushDiv = 1;
static float bitcrushLevels = 32768.0f;
static float bitcrushInverseLevels = 1.0f / 32768.0f;
static uint8_t bitcrushHoldCtr = 0;
static float bitcrushQuantizedHeld = 0.0f;
static float bitcrushHeld = 0.0f;
static float tremDepth = 0.0f;
static float tremRateHz = 4.0f;
static float tremPhase = 0.0f;
static float tremPhaseStep = 4.0f / SAMPLE_RATE;
static float driveAmount = 0.0f;
static float foldAmount = 0.0f;

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
inline float waveFold(float x) {
  x = clampf(x, -3.0f, 3.0f);
  while (x > 1.0f) x = 2.0f - x;
  while (x < -1.0f) x = -2.0f - x;
  return x;
}
inline float applyDriveFold(float x) {
  if (driveAmount > 0.001f) {
    const float pre = x * (1.0f + 8.0f * driveAmount);
    const float driven = pre / (1.0f + fabsf(pre));
    x = driveAmount >= 0.999f ? driven : fastLerp(x, driven, driveAmount);
  }
  if (foldAmount > 0.001f) {
    const float folded = waveFold(x * (1.0f + 5.0f * foldAmount));
    x = foldAmount >= 0.999f ? folded : fastLerp(x, folded, foldAmount);
  }
  return clampf(x, -1.0f, 1.0f);
}
inline int32_t audioRound(float x) {
  return (int32_t)(x >= 0.0f ? x + 0.5f : x - 0.5f);
}
inline float applyBitcrush(float x) {
  if (bitcrushMix <= 0.001f) return x;

  float crushed;
  if (bitcrushDiv > 1) {
    // The held input cannot change until the next capture. Quantize it once
    // per hold period instead of repeating float-to-int conversion per sample.
    if (bitcrushHoldCtr == 0) {
      bitcrushHeld = x;
      bitcrushQuantizedHeld = audioRound(clampf(x, -1.0f, 1.0f) * bitcrushLevels)
          * bitcrushInverseLevels;
    }
    crushed = bitcrushQuantizedHeld;
    if (++bitcrushHoldCtr >= bitcrushDiv) bitcrushHoldCtr = 0;
  } else {
    bitcrushHoldCtr = 0;
    bitcrushHeld = x;
    crushed = audioRound(clampf(x, -1.0f, 1.0f) * bitcrushLevels)
        * bitcrushInverseLevels;
  }
  return bitcrushMix >= 0.999f ? crushed : fastLerp(x, crushed, bitcrushMix);
}
inline float applyTremolo(float x) {
  if (tremDepth <= 0.001f) return x;

  tremPhase += tremPhaseStep;
  if (tremPhase >= 1.0f) tremPhase -= 1.0f;
  const float tri = (tremPhase < 0.5f) ? (tremPhase * 2.0f) : (2.0f - tremPhase * 2.0f);
  const float amp = 1.0f - tremDepth * tri;
  return x * amp;
}

// Control params
static float morph = 0.0f;
static float cutoff = 2000.0f, resonance = 0.9f;
static float noiseAmt = 0.0f;  // CC93

// Delay
// Keep the 8-bit delay line at 11,025 Hz regardless of the oscillator rate.
// This covers a synced 1/16 note at 40 BPM within the Uno R4 RAM budget.
// Oscillators and filters remain at SAMPLE_RATE.
static const uint16_t DLY_SAMPLE_RATE = 11025;
static_assert(SAMPLE_RATE % DLY_SAMPLE_RATE == 0, "Audio rate must divide the delay rate");
static const uint8_t DLY_RATE_DIV = SAMPLE_RATE / DLY_SAMPLE_RATE;
static const uint16_t DLY_SIZE = (DLY_SAMPLE_RATE * 3UL / 8UL) + 2;
static int8_t dlyBuf[DLY_SIZE];
static uint16_t dlyW = 0;
static uint8_t dlyRateCounter = 0;
// The selected tap changes at control rate; audioTick only reads the ring.
static uint16_t dlyTapSamples = 1;
static float dlyTime = 0.25f, dlyFb = 0.25f, dlyMix = 0.0f;

// Each input has its own clock history. DIN wins while it is active, then
// native USB MIDI (in the optional USB build), then Pico-forwarded RTP-MIDI.
enum ClockInput : uint8_t { CLOCK_NONE, CLOCK_DIN, CLOCK_USB, CLOCK_RTP };
struct MidiClockState {
  uint32_t lastPulseUs = 0;
  uint32_t beatStartUs = 0;
  uint32_t pulses = 0;
  uint32_t phaseTicks = 0;
  uint32_t transportUs = 0;
  bool transportRunning = true;
  float bpm = 120.0f;
  bool valid = false;
};
static MidiClockState dinClock, usbClock, rtpClock;
static ClockInput selectedClock = CLOCK_NONE;
static constexpr uint32_t CLOCK_TIMEOUT_US = 2000000UL;
static bool externalTransportRunning = true;
static float externalBeatPosition = 0.0f;
static float externalBeatOffset = 0.0f;
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
static const float VEL_TO_CUTOFF_HZ = 4000.0f;
static const float NOISE_TO_CUTOFF_HZ = 2000.0f;

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

// Fractional-microsecond scheduler: alternates integer intervals so the
// long-term rate is exactly SAMPLE_RATE instead of truncating to 45 us.
static uint32_t nextSample;
static uint32_t sampleRemainder = 0;
static const uint32_t SAMPLE_BASE_US = 1000000UL / SAMPLE_RATE;
static const uint32_t SAMPLE_REMAINDER_US = 1000000UL % SAMPLE_RATE;

static inline void scheduleNextSample() {
  nextSample += SAMPLE_BASE_US;
  sampleRemainder += SAMPLE_REMAINDER_US;
  if (sampleRemainder >= SAMPLE_RATE) {
    sampleRemainder -= SAMPLE_RATE;
    ++nextSample;
  }
}

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

// ---------- LED Matrix ----------
ArduinoLEDMatrix matrix;
uint8_t ledBuf[8][12];
volatile uint32_t ledCanary = 0xCAFEBABE;

static uint32_t noteBlinkUntil = 0;
static uint32_t voiceStealPipHoldUntil = 0;
static uint32_t audioSlipPipHoldUntil = 0;
enum VizMode : uint8_t { VIZ_STATUS = 0,
                         VIZ_VU = 1,
                         VIZ_SCOPE = 2 } vizMode = VIZ_STATUS;

// ---------- Networking ----------
#if USE_WIFI_STACK
bool wifiOK = false, rtpOK = false;
#endif

// ---------- DIN MIDI ----------
SoftwareSerial midiSerial(2, 3); // RX on pin 2, TX on pin 3
MIDI_CREATE_INSTANCE(SoftwareSerial, midiSerial, DIN_MIDI);
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, PICO_MIDI);

static void sendPicoLinkStatus(uint8_t command) {
  const uint8_t flags = (standaloneTouched ? 0x01 : 0) | (picoPatchApplied ? 0x02 : 0);
  const uint8_t message[] = {0xF0, 0x7D, 0x00, command, flags, 0xF7};
  PICO_MIDI.sendSysEx(sizeof(message), message, true);
}

static void markStandaloneActivity() {
  if (standaloneTouched) return;
  standaloneTouched = true;
  if (picoLinkActive()) sendPicoLinkStatus();
}

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
inline int8_t unisonPosition(uint8_t count, uint8_t index) {
  if (count <= 1) return 0;
  if (count == 2) return index == 0 ? -1 : 1;
  return (int8_t)index - 1;  // three voices: low, centre, high
}
inline void updateGlideAlpha() {
  glideTime = maxf(glideTime, 0.0f);
  glideAlpha = (glideTime > 0.001f) ? expf(-1.0f / (CONTROL_RATE * glideTime)) : 0.0f;
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
inline float lfoSine(float phase) {
  const float position = phase * WT_SIZE;
  const uint16_t index = (uint16_t)position & (WT_SIZE - 1);
  const float fraction = position - (uint16_t)position;
  return fastLerp(WT_SINE[index], WT_SINE[(index + 1) & (WT_SIZE - 1)], fraction) * (1.0f / 32767.0f);
}
// Polynomial error is negligible for the modulation range (at most +/- 2 semitones).
inline float smallExp(float x) {
  const float x2 = x * x;
  return 1.0f + x + x2 * (0.5f + x * (1.0f / 6.0f + x * (1.0f / 24.0f + x / 120.0f)));
}

// Morph is updated at the control rate; compute its segment and blend once.
static uint8_t morphSegment = 0;
static uint16_t morphBlendQ15 = 0;  // 0..32767 waveform blend
static float morphBlend = 0.0f;
inline int16_t oscReadQ15(uint8_t idx) {
  const int32_t saw = WT_SAW[idx];
  int32_t a, b;
  if (morphSegment == 1) {
    a = saw >= 0 ? 2 * saw - 32767 : 2 * saw + 32767;
    b = saw;
  } else if (morphSegment == 0) {
    a = WT_SINE[idx];
    b = saw >= 0 ? 2 * saw - 32767 : 2 * saw + 32767;
  } else {
    a = saw;
    b = WT_SINE[idx] >= 0 ? 32767 : -32767;
  }
  return (int16_t)(a + (((b - a) * morphBlendQ15) >> 15));
}

// ADSR
inline void updateAdsrSteps(ADSR& e) {
  e.aStep = 1.0f / (SAMPLE_RATE * maxf(1e-5f, e.a));
  e.dStep = 1.0f / (SAMPLE_RATE * maxf(1e-5f, e.d));
  e.rStep = 1.0f / (SAMPLE_RATE * maxf(1e-5f, e.r));
}
inline void adsrTick(struct ADSR& e) {
  if (e.gate) {
    if (!e.decaying) {
      e.env += e.aStep;
      if (e.env >= 1.0f) {
        e.env = 1.0f;
        e.decaying = true;
      }
    } else {
      const float sustain = clampf(e.s, 0.0f, 1.0f);
      if (e.env > sustain) {
        e.env -= e.dStep;
        if (e.env < sustain) e.env = sustain;
      } else if (e.env < sustain) {
        e.env = sustain;
      }
    }
  } else {
    e.env -= e.rStep;
    if (e.env < 0) e.env = 0;
  }
}

// SVF
void svfUpdateCoeffs() {
  float fc = clampf(cutoff, 20.0f, SAMPLE_RATE * 0.45f);
  float Q = clampf(resonance, 0.5f, 3.0f);
  svf.g = tanf(PI * fc / SAMPLE_RATE);
  svf.R = 1.0f / Q;
  svf.a1 = 1.0f / (1.0f + svf.g * (svf.g + svf.R));
  svf.a2 = svf.g * svf.a1;
  svf.a3 = svf.g * svf.a2;
}
inline float svfProcess(float x) {
  if (!svf.enabled) return x;
  const float v3 = x - svf.ic2eq;
  const float v1 = svf.a1 * svf.ic1eq + svf.a2 * v3;
  const float lp = svf.ic2eq + svf.a2 * svf.ic1eq + svf.a3 * v3;
  svf.ic1eq = 2.0f * v1 - svf.ic1eq;
  svf.ic2eq = 2.0f * lp - svf.ic2eq;
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
static uint8_t sustainOwners = 0;
static bool heldNotes[128] = { false };
static uint8_t heldVelocity[128] = { 0 };
static bool sustainLatch[128] = { false };
// The Pico UART carries both browser notes and forwarded RTP notes. Keep
// independent ownership so a wireless disconnect cannot release a DIN or
// browser note of the same pitch.
enum NoteInput : uint8_t { NOTE_PICO = 1, NOTE_DIN = 2, NOTE_RTP = 4, NOTE_USB = 8 };
static uint8_t noteOwners[128] = { 0 };

enum ArpMode : uint8_t { ARP_OFF = 0,
                         ARP_UP = 1,
                         ARP_DOWN = 2,
                         ARP_UP_DOWN = 3,
                         ARP_RANDOM = 4 };
static ArpMode arpMode = ARP_OFF;
static uint8_t arpDiv = 2;
static uint8_t arpGate = 60;
struct ArpState {
  float stepTimer = 0.0f;
  float gateTimer = 0.0f;
  int8_t direction = 1;
  int16_t index = -1;
  int16_t lastNote = -1;
  int32_t nextExternalStep = 0;
} arpState;

static float arpBeatsPerStep(uint8_t div) {
  switch (div) {
    default: case 0: return 1.0f;
    case 1: return 0.5f;
    case 2: return 0.25f;
    case 3: return 0.125f;
    case 4: return 0.0625f;
    case 5: return 1.0f / 6.0f;
    case 6: return 1.0f / 12.0f;
    case 7: return 1.0f / 8.0f;
  }
}

static void alignExternalArpToNextStep() {
  const float step = arpBeatsPerStep((uint8_t)min<uint8_t>(arpDiv, 7));
  arpState.nextExternalStep = (int32_t)floorf(externalBeatPosition / step + 0.0001f) + 1;
}

// --- Chorus
#if USE_CHORUS
static const uint16_t CH_BUF = 256;
static int8_t chBuf[CH_BUF];
static uint16_t chW = 0;
static float chRate1 = 0.25f, chRate2 = 0.33f;
static float chDepth = 5.0f;
static float chMix = 0.25f;
static float chPh1 = 0.0f, chPh2 = 0.5f;
static uint8_t chOffset1 = CH_BUF / 3, chOffset2 = CH_BUF / 3;
#else
static float chMix = 0.0f;
static float chDepth = 0.0f;
#endif

// Voice management
int8_t allocVoice() {
  for (uint8_t i = 0; i < NUM_VOICES; ++i)
    if (!V[i].active || (!V[i].eg.gate && V[i].eg.env <= 0.0005f))
      return i;

  voiceStealPipHoldUntil = millis() + 400;
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

static void triggerVoiceNote(uint8_t note, uint8_t vel) {
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
    const int8_t pos = unisonPosition(v.unison, u);
    float ff = center;
    if (pos < 0) ff /= detuneRatio;
    else if (pos > 0) ff *= detuneRatio;
    float newInc = ff / SAMPLE_RATE;
    if (wasActive && glideTime > 0.001f) {
      v.incTarget[u] = newInc;
    } else {
      v.phase[u] = 0;
      v.inc[u] = newInc;
      v.incTarget[u] = newInc;
    }
  }
  v.eg.gate = true;
  v.eg.decaying = false;
  noteBlinkUntil = millis() + 400;
}

static void releaseVoiceNote(uint8_t note) {
  for (auto& v : V)
    if (v.active && v.note == note) v.eg.gate = false;
}

static void silenceVoicesImmediately() {
  for (auto& v : V) {
    v.active = false;
    v.eg.gate = false;
    v.eg.decaying = false;
    v.eg.env = 0.0f;
  }
}

static void resetArpState(bool stopCurrentNote) {
  if (stopCurrentNote && arpState.lastNote >= 0)
    releaseVoiceNote((uint8_t)arpState.lastNote);
  arpState = ArpState{};
}

static void setArpMode(ArpMode nextMode) {
  if (nextMode == arpMode) return;

  if (arpMode != ARP_OFF) resetArpState(true);
  if (arpMode == ARP_OFF && nextMode != ARP_OFF) silenceVoicesImmediately();

  arpMode = nextMode;
  if (arpMode != ARP_OFF && tempoSrc == 1) alignExternalArpToNextStep();
  if (arpMode == ARP_OFF) {
    for (uint16_t note = 0; note < 128; ++note)
      if (heldNotes[note]) triggerVoiceNote((uint8_t)note, heldVelocity[note]);
  }
}

void synthNoteOn(uint8_t note, uint8_t vel) {
#if DEBUG_MIDI_EVENTS
  Serial.println(F("[synthNoteOn] Note triggered"));
#endif
  heldNotes[note] = true;
  heldVelocity[note] = vel;
  sustainLatch[note] = false;
  if (arpMode == ARP_OFF) triggerVoiceNote(note, vel);
}

void synthNoteOff(uint8_t note) {
#if DEBUG_MIDI_EVENTS
  Serial.println(F("[synthNoteOff] Note off"));
#endif
  heldNotes[note] = false;
  if (arpMode != ARP_OFF) return;
  if (sustainOn) {
    sustainLatch[note] = true;
    return;
  }
  releaseVoiceNote(note);
}

static void inputNoteOn(uint8_t input, uint8_t note, uint8_t velocity) {
  if (note >= 128 || velocity == 0) return;
  noteOwners[note] |= input;
  synthNoteOn(note, velocity);
}

static void inputNoteOff(uint8_t input, uint8_t note) {
  if (note >= 128 || !(noteOwners[note] & input)) return;
  noteOwners[note] &= (uint8_t)~input;
  if (noteOwners[note] == 0) synthNoteOff(note);
}

static bool clockRecent(const MidiClockState& clock, uint32_t nowUs) {
  return (clock.valid && (uint32_t)(nowUs - clock.lastPulseUs) <= CLOCK_TIMEOUT_US) ||
         (clock.transportUs != 0 &&
          (uint32_t)(nowUs - clock.transportUs) <= CLOCK_TIMEOUT_US);
}

static void selectClock(uint32_t nowUs) {
  ClockInput next = clockRecent(dinClock, nowUs) ? CLOCK_DIN
                  : clockRecent(usbClock, nowUs) ? CLOCK_USB
                  : clockRecent(rtpClock, nowUs) ? CLOCK_RTP : CLOCK_NONE;
  if (next == selectedClock) return;
  selectedClock = next;
  if (next == CLOCK_NONE) return; // Keep the last tempo and free-run at it.
  MidiClockState& clock = next == CLOCK_DIN ? dinClock
                          : next == CLOCK_USB ? usbClock : rtpClock;
  bpmExt = clock.bpm;
  externalTransportRunning = clock.transportRunning;
  if (!externalTransportRunning && arpState.lastNote >= 0) {
    releaseVoiceNote((uint8_t)arpState.lastNote);
    arpState.lastNote = -1;
  }
  externalBeatOffset = roundf(externalBeatPosition - clock.phaseTicks / 24.0f);
  externalBeatPosition = externalBeatOffset + clock.phaseTicks / 24.0f;
}

static void receiveClock(MidiClockState& clock, ClockInput input) {
  const uint32_t nowUs = micros();
  if (clock.lastPulseUs == 0 || (uint32_t)(nowUs - clock.lastPulseUs) > CLOCK_TIMEOUT_US) {
    clock.pulses = 0;
    clock.valid = false;
  }
  if (clock.pulses == 0) clock.beatStartUs = nowUs;
  ++clock.pulses;
  if (clock.pulses == 25) { // Twenty-four intervals make one quarter note.
    const uint32_t elapsed = nowUs - clock.beatStartUs;
    const float measured = elapsed ? 60000000.0f / elapsed : 0.0f;
    if (measured >= 39.5f && measured <= 240.5f) {
      clock.bpm = clampf(measured, 40.0f, 240.0f);
      clock.valid = true;
    } else {
      clock.valid = false;
    }
    clock.pulses = 1;
    clock.beatStartUs = nowUs;
  }
  clock.lastPulseUs = nowUs;
  selectClock(nowUs);
  const float sourceBeat = clock.phaseTicks / 24.0f;
  if (clock.transportRunning) ++clock.phaseTicks;
  if (selectedClock != input || !externalTransportRunning) return;
  if (clock.valid) bpmExt = clock.bpm;
  // Pulse position corrects the time-based phase between pulses. A step index
  // in the arpeggiator prevents a late pulse from retriggering a played step.
  externalBeatPosition = externalBeatOffset + sourceBeat;
}

static void receiveTransport(ClockInput input, uint8_t command) {
  MidiClockState& clock = input == CLOCK_DIN ? dinClock
                          : input == CLOCK_USB ? usbClock : rtpClock;
  const uint32_t nowUs = micros();
  if (command == 0) clock.phaseTicks = 0;
  clock.transportRunning = command != 2;
  clock.transportUs = command == 2 ? 0 : nowUs;
  selectClock(nowUs);
  if (selectedClock != CLOCK_NONE && selectedClock != input) return;
  if (selectedClock == CLOCK_NONE) selectedClock = input;
  if (command == 0) { // Start
    resetArpState(true);
    externalBeatPosition = 0.0f;
    externalBeatOffset = 0.0f;
    externalTransportRunning = true;
  } else if (command == 1) { // Continue
    externalTransportRunning = true;
  } else { // Stop
    externalTransportRunning = false;
    if (arpState.lastNote >= 0) releaseVoiceNote((uint8_t)arpState.lastNote);
    arpState.lastNote = -1;
  }
}

static bool applySoundProgram(uint8_t program) {
  switch (program) {
    case 100:
      if (tempoSrc != 1) { resetArpState(true); tempoSrc = 1; alignExternalArpToNextStep(); }
      return true;
    case 101:
      if (tempoSrc != 0) { resetArpState(true); tempoSrc = 0; }
      return true;
    case 102: dlySync = true; return true;
    case 103: dlySync = false; return true;
    case 110: lfo2Wave = LFO_SINE; return true;
    case 111: lfo2Wave = LFO_TRI; return true;
    case 112: lfo2Wave = LFO_SQR; return true;
    default: return false;
  }
}

static bool applySoundSysEx(const byte* sysex, unsigned size) {
  if (!sysex || size < 6 || sysex[0] != 0xF0 || sysex[1] != 0x7D ||
      sysex[2] != 0x00 || sysex[size - 1] != 0xF7) return false;
  switch (sysex[3]) {
    case 0x01: {
      if (size != 7 || sysex[4] > 127 || sysex[5] > 127) return false;
      const uint16_t bpm = sysex[4] | (sysex[5] << 7);
      if (bpm < 40 || bpm > 240) return false;
      bpmInt = (float)bpm;
      return true;
    }
    case 0x02:
      if (size != 7 || sysex[4] > 1 || sysex[5] > 127) return false;
      if (sysex[4] == 0) lfoToMorph = sysex[5] / 127.0f;
      else lfoToAmp = sysex[5] / 127.0f;
      return true;
    case 0x03:
      if (size != 6 || sysex[4] > 20) return false;
      chDepth = (float)sysex[4];
      return true;
    default: return false;
  }
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

static void inputSustain(uint8_t input, bool on) {
  if (on) sustainOwners |= input;
  else sustainOwners &= (uint8_t)~input;
  sustainSet(sustainOwners != 0);
}

// CC map
void allNotesOff() {
  for (auto& v : V) { v.eg.gate = false; }
  resetArpState(false);
  memset(heldNotes, 0, sizeof(heldNotes));
  memset(noteOwners, 0, sizeof(noteOwners));
  memset(heldVelocity, 0, sizeof(heldVelocity));
  memset(sustainLatch, 0, sizeof(sustainLatch));
}

void allSoundOff() {
  silenceVoicesImmediately();
  resetArpState(false);
  memset(heldNotes, 0, sizeof(heldNotes));
  memset(noteOwners, 0, sizeof(noteOwners));
  memset(heldVelocity, 0, sizeof(heldVelocity));
  memset(sustainLatch, 0, sizeof(sustainLatch));
  memset(dlyBuf, 0, sizeof(dlyBuf));
  dlyW = 0;
  dlyRateCounter = 0;
  sustainOn = false;
  sustainOwners = 0;
}

void handleCC(uint8_t cc, uint8_t val) {
  const float t0 = val / 127.0f;

#if DEBUG_MIDI_EVENTS
  Serial.print(F("[handleCC] cc="));
  Serial.print(cc);
  Serial.print(F(" val="));
  Serial.println(val);
#endif

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
        float g = 0.2f + 0.8f * t0;
        smGain.setTarget(g);
        masterGain = g;
        break;
      }
    case 12:
      {
        dlyTime = 0.02f + 0.146f * (val / 127.0f);
        break;
      }
    case 13:
      {
        dlyFb = 0.01f + 0.88f * (val / 127.0f);
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
        ArpMode nextMode;
        if (val < 26) nextMode = ARP_OFF;
        else if (val < 51) nextMode = ARP_UP;
        else if (val < 77) nextMode = ARP_DOWN;
        else if (val < 102) nextMode = ARP_UP_DOWN;
        else nextMode = ARP_RANDOM;
        setArpMode(nextMode);
        break;
      }
    case 19:
      {
        arpDiv = (uint8_t)roundf((val / 127.0f) * 7);
        if (tempoSrc == 1) alignExternalArpToNextStep();
        break;
      }
    case 20:
      {
        arpGate = (uint8_t)constrain((int)roundf(5.0f + 90.0f * (val / 127.0f)), 5, 95);
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
        for (auto& v : V) v.eg.s = 0.10f + 0.75f * t;
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
        lfo2RateHz = 0.1f + 20.0f * (val / 127.0f);
        break;
      }
    case 28:
      {
        lfo2AmtSemi = 0.0f + 2.0f * (val / 127.0f);
        break;
      }
    case 29:
      {
        bitcrushMix = t0;
        break;
      }
    case 30:
      {
        bitcrushBits = 4.0f + 12.0f * t0;
        break;
      }
    case 31:
      {
        bitcrushRateDiv = 1.0f + 15.0f * t0;
        break;
      }
    case 64:
      {
        sustainSet(val >= 64);
        break;
      }
    case 71:
      {
        smReso.setTarget(0.5f + 2.5f * t0);
        break;
      }
    case 72:
      {
        float t = t0;
        for (auto& v : V) { v.eg.r = 0.02f + 1.48f * t; updateAdsrSteps(v.eg); }
        break;
      }
    case 73:
      {
        float t = t0;
        for (auto& v : V) { v.eg.a = 0.002f + 0.498f * t; updateAdsrSteps(v.eg); }
        break;
      }
    case 74:
      {
        smCutoff.setTarget(40.0f + 9960.0f * t0);
        break;
      }
    case 75:
      {
        float t = t0;
        for (auto& v : V) { v.eg.d = 0.01f + 0.99f * t; updateAdsrSteps(v.eg); }
        break;
      }
    case 76:
      {
        smMorph.setTarget(t0);
        break;
      }
    case 77:
      {
        tremDepth = t0;
        break;
      }
    case 78:
      {
        tremRateHz = 0.1f + 12.0f * t0;
        break;
      }
    case 79:
      {
        driveAmount = t0;
        break;
      }
    case 80:
      {
        foldAmount = t0;
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
      allSoundOff();
      break;
    case 123:  // All Notes Off
      allNotesOff();
      break;
    default: break;
  }
}

// ---------- Audio engine ----------
void audioInit() {
  analogWriteResolution(12);
  // The core's analogWrite() reopens the DAC on every call. Configure A0
  // once, then write its 12-bit data register for each audio sample.
  analogWrite(A0, 2048);
  nextSample = micros();
  sampleRemainder = 0;
  initWavetables();
  updateDetuneRatio();
  updateGlideAlpha();
  for (auto& v : V) {
    v.active = false;
    v.eg = { 0.005f, 0.08f, 0.7f, 0.25f, 0.0f, false, false };
    updateAdsrSteps(v.eg);
    v.unison = 1;
    v.vel = 0.8f;
    for (uint8_t u = 0; u < MAX_UNISON; u++) {
      v.phase[u] = 0;
      v.phaseStep[u] = 0;
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
}

// Keep pitch bend, vibrato, detune, and glide out of the per-sample loop.
inline void updateVoicePhaseSteps(Voice& v) {
  const float bend = pitchBendRatio * lfo2Ratio;
  const float leftBend = bend / lfoDetuneRatio;
  const float rightBend = bend * lfoDetuneRatio;
  for (uint8_t u = 0; u < v.unison; ++u) {
    if (glideAlpha > 0.0f)
      v.inc[u] = v.incTarget[u] + (v.inc[u] - v.incTarget[u]) * glideAlpha;
    const int8_t pos = unisonPosition(v.unison, u);
    const float ratio = pos < 0 ? leftBend : (pos > 0 ? rightBend : bend);
    v.phaseStep[u] = (uint32_t)(v.inc[u] * ratio * 4294967296.0f);
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

inline bool audioTick() {
  uint32_t now = micros();
  if ((int32_t)(now - nextSample) < 0) return false;
  scheduleNextSample();
  static const float invUnison[] = {0.0f, 1.0f, 0.5f, 1.0f / 3.0f};

#if PER_VOICE_FILTER
  float y = 0.0f;
  for (auto& v : V) {
    if (!v.active && v.eg.env <= 0) continue;

    int32_t sampleSum = 0;
    const float invU = invUnison[v.unison];
    for (uint8_t u = 0; u < v.unison; ++u) {
      sampleSum += oscReadQ15((uint8_t)(v.phase[u] >> 24));
      v.phase[u] += v.phaseStep[u];
    }
    const float sum = sampleSum * (invU / 32767.0f);

    adsrTick(v.eg);

    const float vo = v.svf.process(sum) * v.eg.env * v.vel;
    y += vo;

    if (!v.eg.gate && v.eg.env <= 0) v.active = false;
  }

  float noiseEnv = 0.0f;
  for (auto& v : V) noiseEnv = maxf(noiseEnv, v.eg.env * v.vel);
  if (noiseAmt > 0.0001f && noiseEnv > 0.0001f) {
    static uint32_t rng = 0x12345678;
    rng = 1664525u * rng + 1013904223u;
    const float wn = ((int32_t)(rng >> 9) / 8388608.0f) - 1.0f;
    y += wn * noiseAmt * noiseEnv * 0.35f;
  }

#else
  float mix = 0.0f;
  for (auto& v : V) {
    if (!v.active && v.eg.env <= 0) continue;

    int32_t sampleSum = 0;
    const float invU = invUnison[v.unison];
    for (uint8_t u = 0; u < v.unison; ++u) {
      sampleSum += oscReadQ15((uint8_t)(v.phase[u] >> 24));
      v.phase[u] += v.phaseStep[u];
    }
    const float sum = sampleSum * (invU / 32767.0f);

    adsrTick(v.eg);
    mix += sum * v.eg.env * v.vel;

    if (!v.eg.gate && v.eg.env <= 0) v.active = false;
  }

  float noiseEnv = 0.0f;
  for (auto& v : V) noiseEnv = maxf(noiseEnv, v.eg.env * v.vel);
  if (noiseAmt > 0.0001f && noiseEnv > 0.0001f) {
    static uint32_t rng = 0x12345678;
    rng = 1664525u * rng + 1013904223u;
    const float wn = ((int32_t)(rng >> 9) / 8388608.0f) - 1.0f;
    mix += wn * noiseAmt * noiseEnv * 0.35f;
  }

  float y = svfProcess(mix);
#endif

  y = dcBlock(y);

  // --- CHORUS (pre-delay)
#if USE_CHORUS
  chBuf[chW] = (int8_t)audioRound(clampf(y, -1.0f, 1.0f) * 127.0f);
  const float t1 = chBuf[(chW + CH_BUF - chOffset1) & (CH_BUF - 1)] / 127.0f;
  const float t2 = chBuf[(chW + CH_BUF - chOffset2) & (CH_BUF - 1)] / 127.0f;
  chW = (chW + 1) & (CH_BUF - 1);
  const float chor = 0.5f * (t1 + t2);
  y = y * (1.0f - chMix) + chor * chMix;
#endif

  // --- LIGHTWEIGHT INSERT FX (pre-delay)
  y = applyDriveFold(y);
  y = applyBitcrush(y);
  y = applyTremolo(y);

  // --- DELAY
  if (dlyMix > 0.001f) {
    const uint16_t dlyR = dlyW >= dlyTapSamples
      ? dlyW - dlyTapSamples : dlyW + DLY_SIZE - dlyTapSamples;
    const float tap = dlyBuf[dlyR] / 127.0f;
    if (++dlyRateCounter >= DLY_RATE_DIV) {
      dlyRateCounter = 0;
      const float fbv = clampf(y + tap * dlyFb, -1.0f, 1.0f);
      dlyBuf[dlyW] = (int8_t)audioRound(fbv * 127.0f);
      if (++dlyW >= DLY_SIZE) dlyW = 0;
    }
    y = y * (1.0f - dlyMix) + tap * dlyMix;
  }

  // --- OUTPUT
  y *= masterGain;
  y = softClip(y);
  y = clampf(y, -1.0f, 1.0f);

  const float mag = fabsf(y);
  vizLevel = maxf(mag, vizLevel * 0.9985f);
  vizPeak = maxf(mag, vizPeak * 0.9997f);

  if (++scopeDecim >= (SAMPLE_RATE / 360)) {
    scopeDecim = 0;
    scopeTrace[scopeWrite] = (int8_t)audioRound(y * 127.0f);
    scopeWrite = (uint8_t)((scopeWrite + 1) % 12);
  }

  const uint16_t dac = (uint16_t)((y * 0.5f + 0.5f) * 4095.0f);
  R_DAC->DADR[0] = dac;
  return true;
}

// ---------- LED visualizers ----------
static inline void drawTopPips() {
  const uint32_t now = millis();
  const bool clockLocked = tempoSrc == 1 && selectedClock != CLOCK_NONE &&
      ((selectedClock == CLOCK_DIN && dinClock.valid &&
        (uint32_t)(micros() - dinClock.lastPulseUs) <= CLOCK_TIMEOUT_US) ||
       (selectedClock == CLOCK_USB && usbClock.valid &&
        (uint32_t)(micros() - usbClock.lastPulseUs) <= CLOCK_TIMEOUT_US) ||
       (selectedClock == CLOCK_RTP && rtpClock.valid &&
        (uint32_t)(micros() - rtpClock.lastPulseUs) <= CLOCK_TIMEOUT_US));
  ledBuf[0][0] = picoPatchApplied ? 1 : 0;
  ledBuf[0][1] = standaloneTouched ? 1 : 0;
  ledBuf[0][2] = clockLocked ? 1 : 0;
  ledBuf[0][3] = sustainOn ? 1 : 0;
  ledBuf[0][4] = arpMode != ARP_OFF ? 1 : 0;
  const bool showUART = picoLinkActive();
  const bool showUARTRX = (int32_t)(now - uartRxPipHoldUntil) < 0;
  ledBuf[0][5] = showUART ? 1 : 0;
  ledBuf[0][6] = showUARTRX ? 1 : 0;
  ledBuf[0][7] = (int32_t)(now - voiceStealPipHoldUntil) < 0;
  const bool showRTPRX = (int32_t)(now - rtpRxPipHoldUntil) < 0;
  ledBuf[0][8] = showRTPRX ? 1 : 0;
  ledBuf[0][9] = (int32_t)(now - noteBlinkUntil) < 0;
  ledBuf[0][10] = (int32_t)(now - audioSlipPipHoldUntil) < 0;
}

void ledsClear() {
  memset(ledBuf, 0, sizeof(ledBuf));
}
void ledsSet(int c, int r, bool on = true) {
  if (r >= 0 && r < 8 && c >= 0 && c < 12) ledBuf[r][c] = on;
}

void drawStatusOverlays() {
  ledBuf[0][11] = ((millis() / 250) & 1);
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
  ledsClear();
  for (int i = 0; i < NUM_VOICES; ++i) {
    const int row = 3 + i;
    const float lvl = clampf(V[i].eg.env, 0.0f, 1.0f);
    const int bars = (int)roundf(lvl * 11.0f);
    for (int c = 0; c < bars && c < 12; ++c) ledBuf[row][c] = 1;
  }

  drawTopPips();
  drawStatusOverlays();
  ledsRender();
}

void ledsVU() {
  // Eleven recent mono output levels make a moving bank of vertical bars.
  // The last column is a peak marker; the bottom-right timing pixel stays free.
  const float level = sqrtf(clampf(vizLevel * VU_DISPLAY_GAIN, 0.0f, 1.0f));
  const float peakLevel = sqrtf(clampf(vizPeak * VU_DISPLAY_GAIN, 0.0f, 1.0f));
  for (uint8_t c = 0; c < 10; ++c) vuHistory[c] = vuHistory[c + 1];
  vuHistory[10] = (uint8_t)roundf(level * 7.0f);
  ledsClear();
  for (uint8_t c = 0; c < 11; ++c)
    for (uint8_t height = 0; height < vuHistory[c]; ++height)
      ledBuf[7 - height][c] = 1;
  if (peakLevel > 0.03f) {
    const uint8_t peak = (uint8_t)roundf(peakLevel * 6.0f);
    ledBuf[7 - (peak > 0 ? peak : 1)][11] = 1;
  }
  drawTopPips();
  drawStatusOverlays();
  ledsRender();
}

void ledsScope() {
  ledsClear();
  drawTopPips();
  drawStatusOverlays();

  float tracePeak = 0.0f;
  for (uint8_t c = 0; c < 12; ++c) {
    const float a = fabsf(scopeTrace[c] / 127.0f);
    tracePeak = maxf(tracePeak, a);
  }

  static float displayGain = 1.0f;
  float targetGain = 1.0f;
  if (tracePeak >= SCOPE_SILENCE_LEVEL) {
    targetGain = clampf(SCOPE_TARGET_PEAK / tracePeak, 1.0f, SCOPE_MAX_GAIN);
  }
  displayGain += (targetGain - displayGain) * 0.35f;

  for (uint8_t c = 0; c < 12; ++c) {
    const uint8_t idx = (uint8_t)((scopeWrite + c) % 12);
    const float y = clampf((scopeTrace[idx] / 127.0f) * displayGain, -1.0f, 1.0f);
    int row = 4 - (int)roundf(y * 3.0f);
    row = (int)constrain(row, 1, 7);
    ledsSet(c, row, true);
  }

  ledsRender();
}

void ledsUpdate() {
  if (matrixScrollActive || (int32_t)(millis() - matrixHoldUntil) < 0) return;

  static uint32_t _lastLEDTick = 0;
  uint32_t now = millis();
  ledStalled = (now - _lastLEDTick > 200);
  _lastLEDTick = now;

  switch (vizMode) {
    case VIZ_STATUS: ledsStatusMeters(); break;
    case VIZ_VU: ledsVU(); break;
    case VIZ_SCOPE: ledsScope(); break;
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

void matrixStartScroll(const char* label, uint16_t holdMs, uint32_t color) {
  strncpy(matrixScrollText, label, sizeof(matrixScrollText) - 1);
  matrixScrollText[sizeof(matrixScrollText) - 1] = '\0';
  matrix.textFont(Font_5x7);
  matrixScrollX = 12;
  matrixScrollEndX = -(int16_t)(strlen(matrixScrollText) * matrix.textFontWidth());
  matrixScrollNextMs = millis();
  matrixScrollHoldMs = holdMs;
  matrixScrollColor = color;
  matrixScrollActive = true;
}

void matrixScrollTick() {
  if (!matrixScrollActive) return;
  const uint32_t now = millis();
  if ((int32_t)(now - matrixScrollNextMs) < 0) return;
  if (matrixScrollX < matrixScrollEndX) {
    matrixScrollActive = false;
    matrixHoldUntil = now + matrixScrollHoldMs;
    return;
  }
  matrix.beginDraw();
  matrix.clear();
  matrix.textFont(Font_5x7);
  matrix.stroke(matrixScrollColor);
  matrix.text(matrixScrollText, matrixScrollX, 1);
  matrix.endDraw();
  --matrixScrollX;
  matrixScrollNextMs += 50;
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
  static uint32_t nextCheck = 0;
  const uint32_t now = millis();
  if ((int32_t)(now - nextCheck) < 0) return;
  nextCheck = now + 500;

  wl_status_t st = (wl_status_t)WiFi.status();
  if (st != WL_CONNECTED || WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
    if ((int32_t)(now - nextTry) < 0) return;
    if (wifiOK) Serial.println(F("[WiFi] lost, retrying..."));
    wifiOK = false;
    WiFi.disconnect();
    wifiKick("maint");
    nextTry = now + 3000;
    return;
  }

  if (!wifiOK) {
    wifiOK = true;
    Serial.print(F("[WiFi] connected: "));
    Serial.println(WiFi.localIP());
#if USE_RTP_MIDI
    if (!rtpOK) setupRTP();
#endif
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

void resetToDefaults() {
    Serial.println(F("[MIDI] Resetting all parameters to defaults"));
    
    // === Envelope (ADSR) ===
    for (auto& v : V) {
        v.eg.a = 0.005f;   // Attack
        v.eg.d = 0.08f;    // Decay  
        v.eg.s = 0.7f;     // Sustain
        v.eg.r = 0.25f;    // Release
        updateAdsrSteps(v.eg);
    }
    
    // === Oscillator ===
    smMorph.setValue(0.0f);
    morph = 0.0f;
    
    // === Filter ===
    smCutoff.setValue(2000.0f);
    cutoff = 2000.0f;
    smReso.setValue(0.9f);
    resonance = 0.9f;
    svf.enabled = true;
    svf.ic1eq = svf.ic2eq = 0.0f;
    svfUpdateCoeffs();
    
    // === Motion ===
    smGlide.setValue(0.03f);
    glideTime = 0.03f;
    updateGlideAlpha();
    smDetune.setValue(8.0f);
    unisonDetuneCents = 8.0f;
    updateDetuneRatio();
    unisonCount = 1;
    
    // === Amplitude ===
    smGain.setValue(0.7f);
    masterGain = 0.7f;
    noiseAmt = 0.0f;
    
    // === Delay ===
    dlyTime = 0.12f;
    dlyFb = 0.25f;
    dlyMix = 0.0f;
    
    // === LFO1 ===
    lfoAmtHz = 800.0f;
    lfoRateHz = 3.0f;
    lfoSync = true;
    lfoToMorph = 0.0f;
    lfoToAmp = 0.0f;
    lfoToDetune = 0.0f;
    lfoDetuneRatio = 1.0f;
    
    // === LFO2 (Vibrato) ===
    lfo2RateHz = 5.0f;
    lfo2AmtSemi = 0.2f;
    lfo2Wave = LFO_SINE;
    
    // === Modulation Matrix ===
    velToCutoff = 0.0f;
    noiseToCutoff = 0.0f;
    
    // === Tempo ===
    bpmInt = 120.0f;
    tempoSrc = 1;  // External clock
    dlySync = false;
    
    // === Chorus ===
#if USE_CHORUS
    chMix = 0.25f;
    chDepth = 5.0f;
#else
    chMix = 0.0f;
    chDepth = 0.0f;
#endif
    
    // === Lightweight FX ===
    bitcrushMix = 0.0f;
    bitcrushBits = 16.0f;
    bitcrushRateDiv = 1.0f;
    bitcrushHoldCtr = 0;
    bitcrushHeld = 0.0f;
    tremDepth = 0.0f;
    tremRateHz = 4.0f;
    tremPhase = 0.0f;
    driveAmount = 0.0f;
    foldAmount = 0.0f;

    // === Arpeggiator ===
    arpMode = ARP_OFF;
    arpDiv = 2;
    arpGate = 60;
    
    // === Voice Management ===
    velMode = VEL_SOFT;
    stealMode = STEAL_QUIETEST;
    
    // === System ===
    allNotesOff();  // Stop all playing notes
    sustainOn = false;
    sustainOwners = 0;
    memset(heldNotes, 0, sizeof(heldNotes));
    memset(sustainLatch, 0, sizeof(sustainLatch));
    
    // === Reset pitch bend ===
    pitchBendRatio = 1.0f;
    
    // === Reset LFO phases ===
    lfoPhase = 0.0f;
    lfo2Phase = 0.0f;
    lfo2Ratio = 1.0f;
    
    // === Reset voice states ===
    for (auto& v : V) {
        v.active = false;
        v.eg.gate = false;
        v.eg.decaying = false;
        v.eg.env = 0.0f;
        v.unison = 1;
        v.vel = 0.8f;
        for (uint8_t u = 0; u < MAX_UNISON; u++) {
            v.phase[u] = 0;
            v.phaseStep[u] = 0;
            v.inc[u] = 0.0f;
            v.incTarget[u] = 0.0f;
        }
#if PER_VOICE_FILTER
        v.cfY = 1200.0f;
        v.cfT = 1200.0f;
        v.svf.cutoff = 1200.0f;
        v.svf.Q = 0.9f;
        v.svf.enabled = true;
        v.svf.ic1eq = 0.0f;
        v.svf.ic2eq = 0.0f;
        v.svf.update(SAMPLE_RATE);
#endif
    }
    
    // === Reset smoothers to current values ===
    smCutoff.setValue(cutoff);
    smReso.setValue(resonance);
    smMorph.setValue(morph);
    smGain.setValue(masterGain);
    smGlide.setValue(glideTime);
    smDetune.setValue(unisonDetuneCents);
    
    // === Reset effects buffers ===
#if USE_CHORUS
    memset(chBuf, 0, sizeof(chBuf));
    chW = 0;
    chPh1 = 0.0f;
    chPh2 = 0.5f;
#endif
    
    memset(dlyBuf, 0, sizeof(dlyBuf));
    dlyW = 0;
    dlyRateCounter = 0;
    
    // === Reset visualization ===
    vizMode = VIZ_STATUS;
    noteBlinkUntil = 0;
    matrixScrollActive = false;
    matrixHoldUntil = 0;
    
    Serial.println(F("[MIDI] Reset complete"));
}

static void setViz(uint8_t m, const char* label) {
  vizMode = (VizMode)(m > 2 ? 2 : m);
  if (vizMode == VIZ_VU) memset(vuHistory, 0, sizeof(vuHistory));
  // Advance the label in matrixScrollTick(), so MIDI and audio keep running.
  matrixStartScroll(label, 900, 0xFFFFFF);
  Serial.print(F("[Viz] "));
  Serial.println(label);
}

// DIN handlers
void setupPICO() {
  PICO_MIDI.begin(MIDI_CHANNEL_OMNI);
  PICO_MIDI.turnThruOff();  // Avoid echoing Pico requests onto the return wire.
  PICO_MIDI.setHandleProgramChange([](byte, byte program) {
    touchUartRx();
    if (program >= 10 && program <= 12) {
      static const char* names[] = {"Viz: Status", "Viz: VU", "Viz: Scope"};
      setViz(program - 10, names[program - 10]);
    } else if (program == 0) {
      picoPatchApplied = false;
      standaloneTouched = false;
      resetToDefaults();
    } else {
      applySoundProgram(program);
    }
  });
  PICO_MIDI.setHandleControlChange([](byte, byte cc, byte val) {
      touchUartRx();
      if (cc == 64) inputSustain(NOTE_PICO, val >= 64);
      else handleCC(cc, val);
  });
  PICO_MIDI.setHandleNoteOn([](byte, byte note, byte vel) {
      touchUartRx();
      if (vel) inputNoteOn(NOTE_PICO, note, vel);
      else inputNoteOff(NOTE_PICO, note);
  });
  PICO_MIDI.setHandleNoteOff([](byte, byte note, byte) {
      touchUartRx();
      inputNoteOff(NOTE_PICO, note);
  });
  PICO_MIDI.setHandlePitchBend([](byte, int bend) {
      touchUartRx();
      setPitchBend((int16_t)(bend + 8192));
  });
  PICO_MIDI.setHandleSystemExclusive([](byte* sysex, unsigned size) {
    touchUartRx();
    if (applySoundSysEx(sysex, size)) return;
    if (sysex && size == 5 && sysex[0] == 0xF0 && sysex[1] == 0x7D &&
        sysex[2] == 0x00 && sysex[3] == 0x14 && sysex[4] == 0xF7) {
      touchRtpRx();
      markStandaloneActivity();
      return;
    }
    // A Pico-forwarded wireless note carries its own input identity. The
    // final release uses the normal envelope, including sustain behavior.
    if (sysex && size == 8 && sysex[0] == 0xF0 && sysex[1] == 0x7D &&
        sysex[2] == 0x00 && sysex[3] == 0x15 && sysex[7] == 0xF7 &&
        sysex[4] <= 1 && sysex[5] < 128 && sysex[6] < 128) {
      touchRtpRx();
      markStandaloneActivity();
      if (sysex[4] == 0 && sysex[6] > 0) inputNoteOn(NOTE_RTP, sysex[5], sysex[6]);
      else inputNoteOff(NOTE_RTP, sysex[5]);
      return;
    }
    if (sysex && size == 6 && sysex[0] == 0xF0 && sysex[1] == 0x7D &&
        sysex[2] == 0x00 && sysex[3] == 0x16 && sysex[4] <= 1 &&
        sysex[5] == 0xF7) {
      touchRtpRx();
      markStandaloneActivity();
      inputSustain(NOTE_RTP, sysex[4] != 0);
      return;
    }
    if (!sysex || size != 5 || sysex[0] != 0xF0 || sysex[1] != 0x7D ||
        sysex[2] != 0x00 || sysex[4] != 0xF7) return;
    if (sysex[3] == 0x10) {
      picoLinkSeen = true;
      lastPicoHelloMs = millis();
      sendPicoLinkStatus();
    } else if (sysex[3] == 0x12) {
      picoPatchApplied = true;
      standaloneTouched = false;
      sendPicoLinkStatus(0x13);
    }
  });
  PICO_MIDI.setHandleClock([]() { receiveClock(rtpClock, CLOCK_RTP); });
  PICO_MIDI.setHandleStart([]() { receiveTransport(CLOCK_RTP, 0); });
  PICO_MIDI.setHandleContinue([]() { receiveTransport(CLOCK_RTP, 1); });
  PICO_MIDI.setHandleStop([]() { receiveTransport(CLOCK_RTP, 2); });
}

// DIN handlers
void setupDIN() {
  DIN_MIDI.begin(MIDI_CHANNEL_OMNI);
  DIN_MIDI.setHandleNoteOn([](byte, byte n, byte v) {
    markStandaloneActivity();
    touchRtpRx();
    if (v) inputNoteOn(NOTE_DIN, n, v);
    else inputNoteOff(NOTE_DIN, n);
  });
  DIN_MIDI.setHandleNoteOff([](byte, byte n, byte) {
    markStandaloneActivity();
    touchRtpRx();
    inputNoteOff(NOTE_DIN, n);
  });
  DIN_MIDI.setHandleControlChange([](byte, byte cc, byte v) {
    markStandaloneActivity();
    touchRtpRx();
    if (cc == 64) inputSustain(NOTE_DIN, v >= 64);
    else handleCC(cc, v);
  });
  DIN_MIDI.setHandlePitchBend([](byte, int bend) {
    markStandaloneActivity();
    touchRtpRx();
    setPitchBend((int16_t)(bend + 8192));
  });
  DIN_MIDI.setHandleProgramChange([](byte, byte program) {
    if (applySoundProgram(program)) markStandaloneActivity();
  });
  DIN_MIDI.setHandleSystemExclusive([](byte* data, unsigned size) {
    if (applySoundSysEx(data, size)) markStandaloneActivity();
  });
  DIN_MIDI.setHandleClock([]() { receiveClock(dinClock, CLOCK_DIN); });
  DIN_MIDI.setHandleStart([]() { receiveTransport(CLOCK_DIN, 0); });
  DIN_MIDI.setHandleContinue([]() { receiveTransport(CLOCK_DIN, 1); });
  DIN_MIDI.setHandleStop([]() { receiveTransport(CLOCK_DIN, 2); });
}

#if USE_RTP_MIDI
static bool rtpInitDone = false;
void setupRTP() {
  if (rtpInitDone) return;
  rtpInitDone = true;
  rtpMIDI.begin(MIDI_CHANNEL_OMNI);
  rtpMIDI.setHandleNoteOn([](byte, byte n, byte v) {
    markStandaloneActivity();
    touchRtpRx();
    synthNoteOn(n, v);
  });
  rtpMIDI.setHandleNoteOff([](byte, byte n, byte) {
    markStandaloneActivity();
    touchRtpRx();
    synthNoteOff(n);
  });
  rtpMIDI.setHandleControlChange([](byte, byte cc, byte v) {
    markStandaloneActivity();
    touchRtpRx();
    handleCC(cc, v);
  });
  rtpMIDI.setHandlePitchBend([](byte, int bend) {
    markStandaloneActivity();
    touchRtpRx();
    setPitchBend((int16_t)(bend + 8192));
  });
  rtpMIDI.setHandleProgramChange([](byte, byte program) {
    if (applySoundProgram(program)) markStandaloneActivity();
  });
  rtpMIDI.setHandleSystemExclusive([](byte* data, unsigned size) {
    if (applySoundSysEx(data, size)) markStandaloneActivity();
  });
  rtpMIDI.setHandleClock([]() { receiveClock(rtpClock, CLOCK_RTP); });
  rtpMIDI.setHandleStart([]() { receiveTransport(CLOCK_RTP, 0); });
  rtpMIDI.setHandleContinue([]() { receiveTransport(CLOCK_RTP, 1); });
  rtpMIDI.setHandleStop([]() { receiveTransport(CLOCK_RTP, 2); });

  ApplertpMIDI.setHandleConnected([](const APPLEMIDI_NAMESPACE::ssrc_t&, const char*) {
    rtpOK = true;
    Serial.println(F("[RTP] Connected"));
    matrixStartScroll("RTP Connected", 1200, 0xFFFFFF);
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
  selectClock(micros());
#if USE_CHORUS
  chPh1 += chRate1 * CTRL_DT;
  chPh2 += chRate2 * CTRL_DT;
  if (chPh1 >= 1.0f) chPh1 -= 1.0f;
  if (chPh2 >= 1.0f) chPh2 -= 1.0f;
  chOffset1 = (uint8_t)((CH_BUF / 3) + wtReadSine(((uint16_t)(chPh1 * WT_SIZE)) & (WT_SIZE - 1)) * chDepth);
  chOffset2 = (uint8_t)((CH_BUF / 3) + wtReadSine(((uint16_t)(chPh2 * WT_SIZE)) & (WT_SIZE - 1)) * chDepth);
#endif
  if (externalTransportRunning) externalBeatPosition += (bpmExt / 60.0f) * CTRL_DT;
  // --- LFO1 (modulation)
  float lfoRate = lfoRateHz;
  const uint8_t bits = (uint8_t)constrain((int)roundf(bitcrushBits), 4, 16);
  bitcrushDiv = (uint8_t)constrain((int)roundf(bitcrushRateDiv), 1, 16);
  bitcrushLevels = (float)((uint32_t)1 << (bits - 1));
  bitcrushInverseLevels = 1.0f / bitcrushLevels;
  tremPhaseStep = tremRateHz / SAMPLE_RATE;
  const float bpmUse = (tempoSrc == 1 ? bpmExt : bpmInt);
  const float delaySeconds = dlySync
    ? maxf(0.01f, 15.0f / maxf(1.0f, bpmUse)) : dlyTime;
  dlyTapSamples = (uint16_t)constrain((int)(delaySeconds * DLY_SAMPLE_RATE), 1, (int)DLY_SIZE - 1);
  if (lfoSync && bpmUse > 1.0f) lfoRate = bpmUse / 60.0f;

  lfoPhase += (lfoRate * CTRL_DT);
  if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
  const float lfo = lfoSine(lfoPhase);
  lfoDetuneRatio = lfoToDetune > 0.0f
    ? smallExp(0.00057762265f * unisonDetuneCents * lfoToDetune * lfo) : 1.0f;

  // --- LFO2 (vibrato)
  lfo2Phase += (lfo2RateHz * CTRL_DT);
  if (lfo2Phase >= 1.0f) lfo2Phase -= 1.0f;
  float l2;
  switch (lfo2Wave) {
    case LFO_TRI: l2 = lfo2Phase < 0.5f ? 4.0f * lfo2Phase - 1.0f : 3.0f - 4.0f * lfo2Phase; break;
    case LFO_SQR: l2 = (lfo2Phase < 0.5f) ? 1.0f : -1.0f; break;
    default: l2 = lfoSine(lfo2Phase); break;
  }
  lfo2Ratio = smallExp(0.057762265f * lfo2AmtSemi * l2);

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
  const float velMod = velToCutoff * maxVoiceVelocity() * VEL_TO_CUTOFF_HZ;
  const float noiseMod = noiseToCutoff * randomNoiseSlow() * NOISE_TO_CUTOFF_HZ;
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

  const float noiseCtrl = noiseToCutoff * randomNoiseSlow() * NOISE_TO_CUTOFF_HZ;
  const float alpha = (SVF_CUTOFF_TAU_MS <= 0.01f) ? 1.0f
                                                   : (1.0f - expf(-CTRL_DT / (SVF_CUTOFF_TAU_MS * 0.001f)));

  for (uint8_t i = 0; i < NUM_VOICES; ++i) {
    Voice& v = V[i];

    const float velModV = velToCutoff * (v.active ? v.vel : 0.0f) * VEL_TO_CUTOFF_HZ;
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
  if (morph < 1.0f / 3.0f) { morphSegment = 0; morphBlend = morph * 3.0f; }
  else if (morph < 2.0f / 3.0f) { morphSegment = 1; morphBlend = (morph - 1.0f / 3.0f) * 3.0f; }
  else { morphSegment = 2; morphBlend = (morph - 2.0f / 3.0f) * 3.0f; }
  morphBlendQ15 = (uint16_t)roundf(clampf(morphBlend, 0.0f, 1.0f) * 32767.0f);
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

  // Refresh phase increments at 1 kHz; the sample loop only advances integer phases.
  for (auto& v : V)
    if (v.active || v.eg.env > 0.0f) updateVoicePhaseSteps(v);

  // ===================== Arpeggiator =====================
  if (arpMode != 0) {
    uint8_t pool[128];
    int nPool = 0;
    for (int n = 0; n < 128; ++n)
      if (heldNotes[n]) pool[nPool++] = (uint8_t)n;

    if (nPool == 0) {
      resetArpState(true);
      alignExternalArpToNextStep();
      return;
    }

    if (tempoSrc == 1 && !externalTransportRunning) return;

    const float spb = (bpmUse > 1.0f) ? (60.0f / bpmUse) : (60.0f / 120.0f);
    const float stepDur = arpBeatsPerStep((uint8_t)min<uint8_t>(arpDiv, 7)) * spb;
    const float gateDur = clampf((arpGate / 100.0f) * stepDur, 0.01f, stepDur);

    if (tempoSrc == 0) arpState.stepTimer += CTRL_DT;
    arpState.gateTimer += CTRL_DT;
    if (tempoSrc == 0 && arpState.index < 0 && arpState.lastNote < 0)
      arpState.stepTimer = stepDur;

    if (arpState.lastNote >= 0 && arpState.gateTimer >= gateDur) {
      releaseVoiceNote((uint8_t)arpState.lastNote);
      arpState.lastNote = -1;
    }

    const int32_t externalStep = (int32_t)floorf(externalBeatPosition /
                                  arpBeatsPerStep((uint8_t)min<uint8_t>(arpDiv, 7)) + 0.0001f);
    const bool stepDue = tempoSrc == 1 ? externalStep >= arpState.nextExternalStep
                                        : arpState.stepTimer >= stepDur;
    if (stepDue) {
      if (tempoSrc == 1) arpState.nextExternalStep = externalStep + 1;
      else arpState.stepTimer -= stepDur;
      arpState.gateTimer = 0.0f;

      if (arpState.index >= nPool) arpState.index = nPool - 1;
      switch (arpMode) {
        case ARP_UP:
          arpState.index = arpState.index < 0 ? 0 : (arpState.index + 1) % nPool;
          break;
        case ARP_DOWN:
          arpState.index = arpState.index < 0 ? nPool - 1 : (arpState.index - 1 + nPool) % nPool;
          break;
        case 3:
          {
            if (arpState.index < 0) {
              arpState.index = 0;
              arpState.direction = 1;
              break;
            }
            arpState.index += arpState.direction;
            if (arpState.index >= nPool) {
              arpState.direction = -1;
              arpState.index = max(0, nPool - 2);
            } else if (arpState.index < 0) {
              arpState.direction = 1;
              arpState.index = (nPool > 1 ? 1 : 0);
            }
          }
          break;
        case ARP_RANDOM: arpState.index = (int)random(nPool); break;
        default: arpState.index = 0; break;
      }

      const int nextNote = pool[arpState.index];
      if (arpState.lastNote >= 0) releaseVoiceNote((uint8_t)arpState.lastNote);
      triggerVoiceNote((uint8_t)nextNote, heldVelocity[nextNote]);
      arpState.lastNote = nextNote;
    }
  }
}

void setup() {
  vizMode = VIZ_STATUS;
  Serial.begin(115200);   // USB for debug
  Serial1.begin(31250);  // Hardware UART for UI

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
  setupPICO();

#if USE_WIFI_STACK
  wifiStart();
#endif

#if USE_BLE_MIDI
  setupBLE();
#endif
  
  setupDIN();

#if USE_RTP_MIDI
  setupRTP();
#endif

  Serial.println(F("[BOOT] Ready for UART commands on Serial1"));
  // Wi-Fi startup may have taken seconds; do not synthesize its elapsed time.
  nextSample = micros();
  sampleRemainder = 0;
  nextCtrlAt = nextSample;
}

// ===================== Main loop =====================
void loop() {
  const uint32_t now = millis();
  static uint32_t tLED = 0;

  // ----- AUDIO FIRST -----
  // Keep a short catch-up window after MIDI/network work. If synthesis cannot
  // keep up with wall time, drop stale work instead of racing through seconds
  // of old audio after notes are released.
  const uint32_t audioNow = micros();
  if ((int32_t)(audioNow - nextSample) > 2000) {
    audioSlipPipHoldUntil = now + 700;
    nextSample = audioNow;
    sampleRemainder = 0;
  }
  for (uint8_t generated = 0; generated < 4 && audioTick(); ++generated) {}

  // ----- DIN & PICO MIDI -----
  // A full patch arrives as a burst of CC, Program Change and SysEx bytes.
  // read() may consume only one byte per call, so service the queued UART
  // bytes before other loop work can cause the hardware RX buffer to overrun.
  for (uint8_t drained = 0; drained < 48 && Serial1.available() > 0; ++drained)
    PICO_MIDI.read();
  DIN_MIDI.read();

  // ----- RTP-MIDI (if enabled) -----
#if USE_RTP_MIDI
#if USE_WIFI_STACK
  // WiFiS3 checks both UDP ports through synchronous modem commands. Polling
  // on every loop starves the synth even when no RTP peer is connected.
  static uint32_t nextRtpPoll = 0;
  if (wifiOK && (int32_t)(millis() - nextRtpPoll) >= 0) {
    nextRtpPoll = millis() + (rtpIsUp() ? 50u : 250u);
    rtpMIDI.read();
  }
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

  // ----- LEDs at ~30 FPS -----
  if (now - tLED > 33) {
    tLED = now;
    if (matrixScrollActive) matrixScrollTick();
    else ledsUpdate();
  }
}
