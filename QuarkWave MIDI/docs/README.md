# QuarkWave MIDI documentation

**Audience:** development testers using an assembled instrument, and developers extending it.  
**Status:** source-reviewed, partial hardware check. The Pico RTP gateway has passed a live invitation, note, disconnect-release, standalone-state, and explicit-sync smoke check; clock timing remains untested. These pages describe firmware behavior inspected on **2026-09-23**. Browser, USB, targeted Uno timing, and owner-observed LED matrix checks have run on the two-board build. The owner has confirmed that Logic Pro works with the optional Uno USB-audio experiment; the proposed physical A0 audio circuit is still unbuilt and untested.

## Choose a guide

- [Quick start](quick-start.md) — first note, first factory sound, and first saved variation on an assembled instrument.
- [Musician guide](user-guide.md) — connect, play, shape sounds, use effects, and manage patches. Start here if you know synth controls but do not need firmware internals.
- [Patch book](patch-book.md) — eight built-in sounds, what shapes each one, and five new patch recipes to try.
- [Sound-design lessons](sound-design.md) — short listening exercises for oscillator, envelope, filter, movement, and effects.
- [Uno LED display](led-display-guide.md) — read the top-row connection lights, voice bars, output meter, and waveform view.
- [External MIDI controllers](external-midi-guide.md) — play the Uno alone, send clock, and use its supported sound commands.
- [Network MIDI setup](network-midi-setup.md) — connect a Mac or Windows RTP-MIDI session to the Pico and use Logic Pro with QuarkWave.
- [Technical guide](technical-guide.md) — board roles, signal flow, WebSocket and MIDI messages, patch files, and firmware behavior.
- [Diagram style](diagram-style.md) — visual conventions for maintaining the guide figures.
- [Connection guide](connection-guide.md) — photographs of the current USB-powered build, known board wiring, and proposed isolated DIN, line, and TDA1308 headphone connections.
- [Hardware test log](hardware-test-log.md) — repeatable checks and space for measured results.
- [Documentation gaps](documentation-gaps.md) — source discrepancies and tasks that need a product decision or hardware check before the guides can make stronger promises.

The **panel** images are source-rendered simulations of the current page, including the Start here page of its ten-page Help, using the built-in Warm Pad values and simulated connected status. The Save As dialog was opened without writing a patch. They document layout and labels, not a fresh hardware check or audible output. Help is embedded in the Pico page; the PDF editions are not uploaded to it. The Help pages cover everyday operation and a technical overview; the Markdown guides remain the source for complete schematics, MIDI value tables, test evidence, and update history. The [connection guide](connection-guide.md) contains owner-supplied photographs of the actual build; its line-output diagram is a proposed circuit, not an as-built photo.

## Firmware snapshot

The two sketches and embedded browser page are the source of truth for these pages:

| Component | Source | SHA-256 |
| --- | --- | --- |
| Uno R4 sound engine | [QuarkWave_MIDI.ino](../QuarkWave_MIDI/QuarkWave_MIDI.ino) | `16fd76343ddbb9858f84335a372225420e2f2d21d56f0c5bc64d26d71f1f9c79` |
| Pico W web controller | [QuarkWave_UI_MIDI.ino](../QuarkWave_UI_MIDI/QuarkWave_UI_MIDI.ino) | `d417836d7293f2a92310ca3f9d98572b8f6d8407242c1f2be2a5398b21a05646` |
| Embedded browser panel | [QuarkWave_UI.h](../QuarkWave_UI_MIDI/QuarkWave_UI.h) | `47e186a323af9138f230f242078ad84fb2771fafd0d51b05bafe4f9e0532e8cc` |

The connected controller identified over USB on 2026-09-23 is a **Pico W (RP2040, 2 MB flash)**. Its deployed build uses a 1 MB LittleFS region to match the existing patch storage. The Uno identified as **Arduino UNO R4 WiFi**. Both uploads verified; an earlier live panel check reported a connected Pico and Uno with the existing “Test Pluck” user patch. The current source shortens the normal Uno label to **Uno: Connected to Pico**; it has not yet been uploaded for a new live check. An automated browser pass exercised 50 control paths and 41 pointer keys, plus all 12 computer keys at three velocities. The Pico reported changes and the Uno handshake was initially connected, but the Uno does not acknowledge each sound command. A Pico return-message parser fix resolved the reproduced explicit-sync failure in repeated two-board tests. A temporary Uno diagnostic confirmed browser Note On, envelope movement, and a finite pre-DAC signal on one patch; audible output through the proposed physical A0 circuit and per-control sound response remain unverified. The latest Uno LED-status source also runs in the [USB-audio experiment](../experiments/usb-audio/README.md): the Pico link stayed connected during recordings, the owner confirmed the matrix responds to notes, and Logic Pro worked with the USB input. Each new status pixel has not been individually accepted. See the [hardware test log](hardware-test-log.md).

The older sketches under `../QuarkWave/` are outside this documentation baseline. The `secrets.h` files contain local Wi-Fi settings and are deliberately excluded.

## Reading the status labels

**Source-reviewed** means the described path exists in this snapshot. **Hardware untested** on a specific procedure means that procedure still needs a two-board test. The handshake and browser paths with recorded results are identified in the [hardware test log](hardware-test-log.md); physical A0 line-output levels remain unmeasured. A procedure that cannot be established confidently from source is listed in [Documentation gaps](documentation-gaps.md) instead of being presented as a working feature.

## When firmware changes

Recheck the three files above, update the protocol and control tables, walk through the musician tasks on the assembled unit, then change the snapshot date and hashes. Record unresolved changes in the gaps page.
