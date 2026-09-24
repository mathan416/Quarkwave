// Host-side packet checks: c++ -std=c++17 test_usb_midi.cpp -o /tmp/quarkwave-usb-midi-test && /tmp/quarkwave-usb-midi-test
#include <cassert>
#include <cstdint>
#include <cstring>

#define CFG_TUD_MIDI 1

enum ClockInput : uint8_t { CLOCK_NONE, CLOCK_DIN, CLOCK_USB, CLOCK_RTP };
enum NoteInput : uint8_t { NOTE_PICO = 1, NOTE_DIN = 2, NOTE_RTP = 4, NOTE_USB = 8 };
struct MidiClockState { bool valid = false; };
static MidiClockState usbClock;
static uint8_t owners[128]{};
static uint8_t sustainOwners = 0;
static uint32_t standaloneEvents = 0;
static uint32_t clockPulses = 0;
static uint8_t lastTransport = 255;
static uint8_t lastProgram = 255;
static uint8_t lastCC = 255;
static uint8_t lastSysExDepth = 255;
static int16_t lastBend = -1;
static bool mounted = false;

static uint32_t micros() { return 1000; }
static void selectClock(uint32_t) {}
static void markStandaloneActivity() { ++standaloneEvents; }
static void touchRtpRx() {}
static void inputNoteOn(uint8_t source, uint8_t note, uint8_t) { owners[note] |= source; }
static void inputNoteOff(uint8_t source, uint8_t note) { owners[note] &= (uint8_t)~source; }
static void inputSustain(uint8_t source, bool on) {
  if (on) sustainOwners |= source;
  else sustainOwners &= (uint8_t)~source;
}
static void handleCC(uint8_t cc, uint8_t) { lastCC = cc; }
static void setPitchBend(int16_t value) { lastBend = value; }
static bool applySoundProgram(uint8_t program) {
  if (program != 100 && program != 101) return false;
  lastProgram = program;
  return true;
}
static bool applySoundSysEx(const uint8_t* bytes, unsigned length) {
  if (length != 6 || bytes[0] != 0xf0 || bytes[1] != 0x7d ||
      bytes[2] != 0 || bytes[3] != 3 || bytes[5] != 0xf7) return false;
  lastSysExDepth = bytes[4];
  return true;
}
static void receiveClock(MidiClockState& clock, ClockInput input) {
  assert(&clock == &usbClock && input == CLOCK_USB);
  ++clockPulses;
  clock.valid = true;
}
static void receiveTransport(ClockInput input, uint8_t command) {
  assert(input == CLOCK_USB);
  lastTransport = command;
}
static bool tud_mounted() { return mounted; }
static bool tud_suspended() { return false; }
static bool tud_midi_mounted() { return mounted; }
static bool tud_midi_packet_read(uint8_t[4]) { return false; }

#include "UsbMidiInput.h"

static void send(uint8_t cin, uint8_t a, uint8_t b = 0, uint8_t c = 0) {
  const uint8_t packet[4] = {cin, a, b, c};
  usbMidiHandlePacket(packet);
}

int main() {
  mounted = true;
  usbMidiService();
  send(0x09, 0x90, 60, 100);
  assert(owners[60] == NOTE_USB && standaloneEvents == 1);
  owners[60] |= NOTE_PICO;
  send(0x08, 0x80, 60, 0);
  assert(owners[60] == NOTE_PICO);

  send(0x09, 0x90, 61, 90);
  send(0x0b, 0xb0, 64, 127);
  assert((sustainOwners & NOTE_USB) != 0);
  send(0x0b, 0xb0, 74, 45);
  assert(lastCC == 74);
  send(0x0e, 0xe0, 0, 64);
  assert(lastBend == 8192);

  const uint32_t beforeClock = standaloneEvents;
  send(0x0f, 0xf8);
  send(0x0f, 0xfa);
  send(0x0f, 0xfc);
  assert(clockPulses == 1 && lastTransport == 2);
  assert(standaloneEvents == beforeClock);

  send(0x0c, 0xc0, 10); // Pico-only visualization is ignored.
  assert(lastProgram == 255 && standaloneEvents == beforeClock);
  send(0x0c, 0xc0, 100);
  assert(lastProgram == 100 && standaloneEvents == beforeClock + 1);

  send(0x04, 0xf0, 0x7d, 0);
  send(0x07, 3, 12, 0xf7);
  assert(lastSysExDepth == 12);
  const uint32_t beforeBad = standaloneEvents;
  send(0x04, 0xf0, 0x7d, 0);
  send(0x06, 0x10, 0xf7); // Pico-only handshake is ignored.
  assert(standaloneEvents == beforeBad);

  send(0x09, 0x90, 62, 100);
  owners[62] |= NOTE_DIN;
  mounted = false;
  usbMidiService();
  assert(owners[61] == 0 && owners[62] == NOTE_DIN && owners[60] == NOTE_PICO);
  assert((sustainOwners & NOTE_USB) == 0 && !usbClock.valid);
}
