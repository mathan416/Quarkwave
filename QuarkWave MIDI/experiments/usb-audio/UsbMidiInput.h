#pragma once

#if !CFG_TUD_MIDI
#error "The USB audio/MIDI build must enable the TinyUSB MIDI device class"
#endif

// Included by the staged Uno sketch after its sound and MIDI helpers. TinyUSB
// supplies USB-MIDI 1.0 event packets; this input has its own note, pedal and
// clock identity so it cannot release a key owned by the Pico or DIN input.
static uint8_t usbMidiSysEx[16];
static uint8_t usbMidiSysExLength = 0;
static bool usbMidiWasMounted = false;

static void usbMidiRelease() {
  for (uint8_t note = 0; note < 128; ++note)
    inputNoteOff(NOTE_USB, note);
  inputSustain(NOTE_USB, false);
  usbClock = MidiClockState{};
  selectClock(micros());
  usbMidiSysExLength = 0;
}

static bool usbMidiAppendSysEx(const uint8_t* bytes, uint8_t count) {
  if (usbMidiSysExLength + count > sizeof(usbMidiSysEx)) {
    usbMidiSysExLength = 0;
    return false;
  }
  for (uint8_t i = 0; i < count; ++i)
    usbMidiSysEx[usbMidiSysExLength++] = bytes[i];
  return true;
}

static void usbMidiHandlePacket(const uint8_t packet[4]) {
  if ((packet[0] >> 4) != 0) return;  // One USB-MIDI cable is advertised.
  const uint8_t cin = packet[0] & 0x0f;
  const uint8_t status = packet[1];
  const uint8_t data1 = packet[2];
  const uint8_t data2 = packet[3];

  if (cin == 0x04 || cin == 0x05 || cin == 0x06 || cin == 0x07) {
    const uint8_t count = cin == 0x05 ? 1 : cin == 0x06 ? 2 : 3;
    if (status == 0xf0) usbMidiSysExLength = 0;
    if (usbMidiSysExLength == 0 && status != 0xf0) return;
    if (!usbMidiAppendSysEx(&packet[1], count)) return;
    if (cin != 0x04) {
      if (usbMidiSysEx[usbMidiSysExLength - 1] == 0xf7 &&
          applySoundSysEx(usbMidiSysEx, usbMidiSysExLength)) {
        markStandaloneActivity();
        touchRtpRx();
      }
      usbMidiSysExLength = 0;
    }
    return;
  }

  if (cin == 0x0f) {
    if (status == 0xf8) receiveClock(usbClock, CLOCK_USB);
    else if (status == 0xfa) receiveTransport(CLOCK_USB, 0);
    else if (status == 0xfb) receiveTransport(CLOCK_USB, 1);
    else if (status == 0xfc) receiveTransport(CLOCK_USB, 2);
    return;
  }

  // A channel message inside an unfinished SysEx is malformed. Ignore the
  // unfinished command, then process the channel message on its own.
  usbMidiSysExLength = 0;
  if (data1 >= 128) return;
  if (cin == 0x08 && (status & 0xf0) == 0x80 && data2 < 128) {
    markStandaloneActivity();
    touchRtpRx();
    inputNoteOff(NOTE_USB, data1);
  } else if (cin == 0x09 && (status & 0xf0) == 0x90 && data2 < 128) {
    markStandaloneActivity();
    touchRtpRx();
    if (data2) inputNoteOn(NOTE_USB, data1, data2);
    else inputNoteOff(NOTE_USB, data1);
  } else if (cin == 0x0b && (status & 0xf0) == 0xb0 && data2 < 128) {
    markStandaloneActivity();
    touchRtpRx();
    if (data1 == 64) inputSustain(NOTE_USB, data2 >= 64);
    else handleCC(data1, data2);
  } else if (cin == 0x0c && (status & 0xf0) == 0xc0) {
    if (applySoundProgram(data1)) {
      markStandaloneActivity();
      touchRtpRx();
    }
  } else if (cin == 0x0e && (status & 0xf0) == 0xe0 && data2 < 128) {
    markStandaloneActivity();
    touchRtpRx();
    setPitchBend((int16_t)(data1 | ((uint16_t)data2 << 7)));
  }
}

static void usbMidiService() {
  const bool mounted = tud_mounted() && !tud_suspended() && tud_midi_mounted();
  if (!mounted) {
    if (usbMidiWasMounted) usbMidiRelease();
    usbMidiWasMounted = false;
    return;
  }
  usbMidiWasMounted = true;
  uint8_t packet[4];
  for (uint8_t drained = 0; drained < 24 && tud_midi_packet_read(packet); ++drained)
    usbMidiHandlePacket(packet);
}
