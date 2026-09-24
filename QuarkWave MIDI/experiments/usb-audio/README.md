# Uno USB audio experiment

This isolated build sends the Uno R4 WiFi synth's mono output over USB as a
22,050 Hz, 16-bit audio input. The same samples still go to the A0 DAC. USB
serial remains available for diagnostics. The Pico continues to control the Uno
over the existing MIDI UART; the build script does not edit either board's
normal firmware.

The installed Arduino Renesas core 1.6.0 normally uses the Uno's ESP32-S3 USB
bridge (`NO_USB`). `build.py` copies that core into a temporary directory,
enables native RA4M1 USB with CDC and UAC2, and compiles a staged copy of the
current Uno sketch. It uses `Serial2` for the D0/D1 Pico UART because the
core's serial names shift when native USB is enabled.

## Build and upload

From this directory, run:

```sh
python3 build.py
python3 upload.py
```

The build requires `arduino-cli`, Arduino Renesas core 1.6.0, the libraries used
by the Uno sketch, and the locally ignored `QuarkWave_MIDI/secrets.h`. It writes
the image to `/private/tmp/quarkwave-usb-audio.bin` and does not copy secrets
into this directory. `upload.py` accepts an optional image path to upload a
different Uno binary.

The first upload can use the Uno's normal ESP USB bridge. Once this build runs,
macOS sees a native serial port and **QuarkWave USB Audio**. To upload again,
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

USB audio remains an experiment. More macOS recording app coverage, other
operating systems, and long sessions with demanding effect settings have not
been tested. The A0 line-output plan remains available.
