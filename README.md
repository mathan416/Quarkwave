# Quarkwave

Quarkwave is a two-board synthesizer project. The Arduino Uno R4 WiFi generates sound; a Raspberry Pi Pico W hosts its browser panel and manages patches. The current development focus is the MIDI version.

## Project layout

- [`QuarkWave MIDI/QuarkWave_MIDI/`](QuarkWave%20MIDI/QuarkWave_MIDI/) — Uno R4 MIDI sound engine.
- [`QuarkWave MIDI/QuarkWave_UI_MIDI/`](QuarkWave%20MIDI/QuarkWave_UI_MIDI/) — Pico W browser controller.
- [`QuarkWave MIDI/docs/README.md`](QuarkWave%20MIDI/docs/README.md) — musician and technical documentation, hardware notes, and verification status.
- [`QuarkWave MIDI/output/pdf/`](QuarkWave%20MIDI/output/pdf/) — manually generated PDF editions of the documentation.
- [`QuarkWave/`](QuarkWave/) — earlier non-MIDI sketches retained for reference.

The MIDI documentation describes the current source and distinguishes verified behavior from hardware work still to be tested.

## Local Wi-Fi configuration

Each sketch that uses Wi-Fi includes a local `secrets.h`. These files contain network credentials and are excluded from Git. On a fresh checkout, copy the `secrets.example.h` in the relevant sketch folder to `secrets.h`, then set your own SSID and password before compiling.

No license has been selected yet.
