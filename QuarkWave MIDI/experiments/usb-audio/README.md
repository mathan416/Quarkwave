# Uno USB audio and MIDI experiment

This isolated build presents the Uno R4 WiFi as a composite USB device: a
22,050 Hz, 16-bit mono **audio input**, a class-compliant **USB MIDI 1.0 port**,
and a USB serial diagnostic port. A DAW can send MIDI notes over the same Uno
USB cable that returns the generated sound. The same samples still go to the
A0 DAC. The Pico continues to control the Uno over the existing MIDI UART;
the build script does not edit the installed Arduino core or either board's
normal firmware.

The installed Arduino Renesas core 1.6.0 normally uses the Uno's ESP32-S3 USB
bridge (`NO_USB`). `build.py` copies that core into a temporary directory,
enables native RA4M1 USB with CDC, UAC2, and USB MIDI, and compiles a staged
copy of the current Uno sketch. It uses `Serial2` for the D0/D1 Pico UART
because the core's serial names shift when native USB is enabled. The staged
USB configuration advertises separate 64-byte MIDI bulk IN/OUT endpoints;
the sound engine currently **receives** USB MIDI and does not echo or sequence
MIDI back to the host.

## Play and record through one cable

1. Build and upload this image, then reconnect the Uno USB cable. In macOS
   **Audio MIDI Setup**, look for **QuarkWave USB Audio** under Audio Devices
   and **QuarkWave USB MIDI** in MIDI Studio. The composite build gives the
   Uno a distinct USB product name and stable serial identity so macOS reads
   this name instead of its cached **UNO R4 WiFi** entry.
2. In Logic Pro, choose **QuarkWave USB Audio** as an input device. Keep your
   speakers or headphones as the output device. Create a mono audio track
   from Input 1 and enable Input Monitoring at a low listening level.
3. Create a MIDI or External MIDI track whose output is **QuarkWave USB MIDI**,
   channel 1 for a first check. Send Middle C, release it, and check the Uno
   matrix and Logic audio meter. Record MIDI and audio in real time; an offline
   bounce cannot render an external sound device.

USB MIDI input accepts notes on all channels, pitch bend, CC (including
sustain), QuarkWave sound Program Changes `100–103` and `110–112`, sound SysEx
`01–03`, and MIDI Clock with Start/Continue/Stop. USB clock is considered only
when external tempo is selected. Recent clock priority is **DIN → USB → Pico
RTP**; when it stops, the Uno holds the last valid tempo. USB MIDI notes and
pedal have separate ownership from Pico, DIN, and RTP notes. If USB data
disconnects while the Uno remains powered, its held notes and pedal release
without releasing another input's note of the same pitch. A DAW stopping a
track does not necessarily disconnect its USB port; send Note Off or Panic if
the port remains connected. Sound commands mark standalone activity, while
Clock and transport alone do not. See the [technical MIDI tables](../../docs/technical-guide.md#program-change-and-sysex-by-input).

## Build and upload

From this directory, run:

```sh
python3 build.py
python3 upload.py
```

The build requires `arduino-cli`, Arduino Renesas core 1.6.0, the libraries used
by the Uno sketch, and the locally ignored `QuarkWave_MIDI/secrets.h`. It writes
the composite image to `/private/tmp/quarkwave-usb-audio-midi.bin` and does
not copy secrets into this directory. The earlier audio-only image path
is left alone. `upload.py` accepts an optional image path to upload a different
Uno binary.

The first upload can use the Uno's normal ESP USB bridge. Once this build runs,
macOS should see a native serial port, **QuarkWave USB Audio**, and a MIDI
port displayed as **QuarkWave USB MIDI** on the tested Mac. To upload again,
`upload.py` requests the bootloader through the native port, waits for the ESP
bridge port, then uploads there. Arduino CLI may print `No device found` during
the first stage even though the reset succeeded. If neither port appears,
double-tap RESET after the board has finished starting and retry. The normal
Uno sketch can be uploaded through the same script by passing its compiled
`.bin` path.

## Driver fix

The first full synth build hung as soon as a Mac started recording: USB serial
diagnostics stopped, the Pico reported the Uno offline, and the LED matrix
stopped updating. The bundled Renesas TinyUSB RUSB2 driver waits indefinitely
for an isochronous IN buffer to become ready. Current upstream TinyUSB bounds
that wait and removes the post-transfer `INBUFM` wait because a host that stops
polling can leave the pipe full. The experiment applies those changes to its
temporary core copy; the installed Arduino core remains untouched. See the
[upstream driver](https://github.com/hathach/tinyusb/blob/master/src/portable/renesas/rusb2/dcd_rusb2.c).

The experiment also services USB at most twice per millisecond after audio,
MIDI, control, and matrix work. When a USB serial monitor is connected, it
reports packet, underrun, overrun, and service-time counters every 500 ms
without blocking the synth loop.

## Live test, 2026-09-23

- macOS recognized a mono 22,050 Hz input and a native USB serial port.
- The Pico reported the Uno connected before, during, and after recording. The
  Uno LED matrix responded to played notes, confirmed by the user.
- A Middle C and a four-note chord sent from the Pico produced nonzero USB
  recordings. The chord had no clipped samples.
- The four-note recording added no underruns or overruns while notes were held.
  A short initial underrun occurred when each recording stream opened.
- Five consecutive three-second recording cycles and one 20-second continuous
  recording completed without losing the Pico link. The long capture contained
  441,344 samples, about 20.02 seconds at 22,050 Hz.
- The USB service peak observed during the four-note capture was 80 µs. This
  is a diagnostic maximum, not a full CPU-load measurement.
- The owner subsequently confirmed that **Logic Pro worked with the Uno's USB
  audio input** on their Mac. The [Logic and RTP-MIDI setup guide](../../docs/network-midi-setup.md)
  distinguishes this audio route from MIDI sent to the Pico over the network.

## Combined USB MIDI and audio check — 2026-09-23

- The composite sketch and the normal Uno sketch compiled. Host-side USB MIDI
  packet tests passed for notes, shared-note ownership, pedal, bend, clock,
  transport, sound commands, and USB disconnect.
- The initial composite image exposed **UNO R4 WiFi** as both a MIDI destination
  and source because CoreMIDI kept the board's original name.
  CoreAudio exposed **QuarkWave USB Audio** as an input.
- A CoreMIDI test sent Middle C Note On (velocity 100) and Note Off while a
  CoreAudio test recorded 3.02 seconds from the same Uno USB cable. Both sends
  returned success. The recording contained 66,560 mono samples at 22,050 Hz:
  silence before the note, a 17,835-count sample peak during the note, and a
  decaying tail after release.
- The Pico page still served HTTP 200 afterward; its live WebSocket reported
  `connected: true`, `syncFailed: false`, and `mode: standalone` after the USB
  sound message. That is the intended standalone-activity state.
- A second capture held the USB MIDI note for 15 seconds during a 20.02-second
  recording (441,344 samples). One-second windows during the hold remained
  nonzero, the signal decayed after Note Off, and the Pico WebSocket still
  reported `connected: true` with no sync failure afterward.
- After the USB product name and experiment-specific serial suffix were added,
  CoreMIDI exposed **QuarkWave USB MIDI** as both destination and source. A
  MIDI note sent through that name produced another nonzero USB recording; the
  Pico still reported its Uno link connected.

The combined route has not yet been tried inside Logic Pro. Simultaneous Pico
and USB playing, external clock timing, USB suspension or
disconnect while notes are held, Windows, and
physical A0 output remain to be tested. The A0 line-output plan remains
available.

## Performance limit found in the regression suite

The optional build now reports `dac` (actual DAC register writes), `slip`
(audio scheduler resets), and `taskUs` (cumulative USB service time) with the
existing packet and ring counters. Use counter **differences after startup**:
absolute underrun counts include the period before the synth is ready.

A clean 7,000-packet interval made 154,337 DAC writes, equivalent to **22,048.1
writes/s**, with no new underruns, overruns, or slips. A sustained four-voice,
three-unison, all-effects interval made 319,824 writes in 15,000 packets,
equivalent to **21,321.6 writes/s**, with 10,959 new USB audio underruns and 240
slips. The host still receives 22,050 samples/s because the USB packetizer
inserts zeros when the synth ring is empty. The [hardware test log](../../docs/hardware-test-log.md#usb-midi-audio-regression-suite--2026-09-23)
records the test method and the remaining performance work. Ordinary play and
the worst-case setting should be judged separately; this experiment is not yet
real-time safe under every combination of effects and voices.
