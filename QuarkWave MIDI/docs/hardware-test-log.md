# QuarkWave hardware test log

**Purpose:** record repeatable results for the assembled Pico W and Uno R4. The guides are **source-reviewed, hardware untested** until the relevant checks pass. Copy this page for each firmware and wiring revision; record actual observations rather than replacing expectations with a checkmark.

## Test record

| Field | Record |
| --- | --- |
| Date, tester, location | |
| Pico / Uno board and core versions | |
| Firmware revision or the three hashes in the [index](README.md#firmware-snapshot) | |
| Arduino libraries and relevant versions | |
| Wiring revision / [connection guide](connection-guide.md) evidence | |
| Power sources; measured Pico RX voltage during Uno TX | |
| Audio destination and listening level | |
| DIN controller, RTP-MIDI host, browser/device used | |

For each check, record **Pass / Fail / Not run**, the observed behavior, and a short note or evidence link. If a check fails, include the smallest repeatable steps and any MIDI, serial, or screen capture that helps reproduce it.

| Check | Expected from source | Result / observation |
| --- | --- | --- |
| Both boards start together | Uno replies when ready; Pico sends the selected boot patch on a clean Uno. Browser timing does not trigger sync. | |
| Uno starts late; Uno restarts | Pico detects the new ready state and syncs a clean Uno. | |
| Pico starts late; Pico restarts | Standalone Uno sound persists if an external input was used; otherwise the selected Pico patch syncs. | |
| Return MIDI wire disconnected | Uno connection label goes offline after missed replies; no false patch acknowledgment. | |
| Browser opens before / after sync | Pico and Uno status labels remain separate and consistent. | |
| Browser note and release | Pointer and computer keys send notes; release and browser disconnect end owned notes. | |
| Keyboard Velocity 1 / 100 / 127 | New notes use the selected velocity; held notes keep the old velocity. | |
| Perform / Shape / Explore / All controls | Controls keep values; held browser notes continue across view changes. | |
| Factory Load and user Save As | Factory sounds stay read-only; Save As chooses a slot and confirms occupied replacement. | |
| User Save, Load, restart | Saved file, selected slot, and recovered sound agree; failure does not change selection. | |
| Commit / Load committed sound | Snapshot can be saved and restored separately from user slots. | |
| Randomize | Returned panel values remain inside each control's range, including effects. | |
| Panic and visualizations | Panic silences notes; Status, VU, and Scope select the intended Uno LED view. | |
| DIN without Pico | Notes, bend, sustain, and CC work with Uno defaults; no Pico is required. | |
| RTP-MIDI without Pico | `QuarkWave` session accepts notes and sound controls. | |
| Standalone then attach Pico | Existing sound remains until explicit **Sync Pico patch to Uno**; warning appears. | |
| External Clock and transport | 40 / 120 / 240 BPM, Start / Continue / Stop, DIN priority, two-second failover, and held tempo behave as documented. | |
| External custom messages | Accepted sound Program Changes and SysEx change sound and mark standalone activity; Pico-only commands have no effect on DIN/RTP. | |
| Output and levels | Listen and measure across master volume, effects, and four-note/unison cases. Document safe connection and clipping behavior. | |

The [documentation gaps](documentation-gaps.md) page provides more detail for handshake, storage, panel, MIDI, and wiring checks. After a result passes, update the relevant guide with the tested board/wiring revision and date; keep untested claims labeled accordingly.

## Deployment smoke check — 2026-09-23

- USB identities: Raspberry Pi Pico W (RP2040, 2 MB flash; serial `E6614C311B886039`) and Arduino UNO R4 WiFi (serial `F412FA704F54`).
- Both sketches compiled and uploads verified. A full Pico flash backup was saved before upload; the new build uses the same 1 MB LittleFS region.
- Browser loaded the redesigned Perform/Shape/Explore/All controls panel at `http://quarkwave.local/`. It displayed **Pico: Connected** and **Uno: Connected · Pico patch** after startup, and selected the existing user patch **Test Pluck** in slot 3.
- Audio playback, control response, power-cycle recovery, standalone behavior, and MIDI Clock were **not tested** in this smoke check.

## Owner-supplied hardware record — 2026-09-23

- The [board overview and close-up](connection-guide.md#the-current-setup) show a Pico W, Uno R4 WiFi, bidirectional level-shifter module, and jumper wiring. The owner confirms Uno TX is shifted from 5 V to 3.3 V before Pico RX, with a common ground.
- **Power:** Each board is currently powered through its own USB port. The audio jack and `A0` output circuit have not been assembled.
- The [mono line-output schematic](connection-guide.md#proposed-mono-line-output-from-uno-a0) is a proposal for a powered speaker or mixer line input. Its DC level, audio level, and sound are **not tested**. Record build and measurement results above once assembled.
- The owner also identified an unconnected [TDA1308 headphone amplifier module](connection-guide.md#proposed-headphones-with-the-owners-tda1308-board). Its pad ground, output DC, gain, and headphone sound have **not** been measured.

## Automated browser and link check — 2026-09-23

- Live embedded page at `http://10.0.2.89/` matched the source HTML. Initial WebSocket state contained eight user slots, eight factory presets, user slot 3 “Test Pluck,” 45 patch fields, and **Uno connected · Pico patch**.
- Headless Chrome exercised 50 control paths in their visible views, 41 pointer keys, and all 12 computer note keys at velocities 1, 100, and 127. The browser sent the expected WebSocket messages and Pico `patchData` reflected control changes. A held browser note survived view switches. These observations do **not** prove that the Uno received each MIDI message or produced the intended audio; there is no per-control Uno acknowledgment and the audio output is not built.
- The first run found that checked browser controls were parsed as zero by the Pico. The Pico handler now converts JSON booleans to 1/0; its sketch compiled, was uploaded, and a repeat control pass passed 50/50. The Pico source hash is recorded in the [index](README.md#firmware-snapshot).
- Randomize returned all 45 patch fields. Factory and user loads selected the expected slots. Save As opened with eight destinations; factory save and occupied Save As without overwrite were rejected. Three visualization selections and Panic emitted their browser messages. Save/Save As success and Commit/Load Commit were **not run** to avoid altering the owner's stored patches. The existing user patch and live values were restored after the full sweep.
- An isolated explicit **Sync Pico patch to Uno** sometimes led to an Uno status timeout. A repeat sequence showed `pico → syncing → offline`; another earlier sequence later reported `syncFailed`. Restarting the Pico with the verified firmware led to repeated Wi-Fi connection attempts. A USB power cycle restored its web page, but the Uno still showed offline. A subsequent USB power cycle of the Uno restored **Uno connected · Pico patch**. The final browser snapshot showed user slot 3 “Test Pluck.” Treat patch sync and return MIDI reliability as **failed/unresolved** until diagnosed on hardware.

## Uno LED display photograph — 2026-09-23

The owner supplied a [live photograph](led-display-guide.md) of the Uno R4 WiFi matrix with several top-row pixels illuminated. The photograph confirms a powered, illuminated matrix, but one still frame cannot validate heartbeat timing, connection-indicator meaning, voice bars, VU/Scope response, or scrolling messages. Those behaviors are source-reviewed in the [LED display guide](led-display-guide.md) and remain candidates for a timed hardware check.

## Return-link parser fix — 2026-09-23

- **Reproduced:** one explicit sync caused two Uno reset messages, showing the Pico had retried the complete patch transfer. A later run showed Uno `Hello` reception every second while the Pico declared it offline.
- **Isolated:** a temporary Pico diagnostic build read valid raw Uno frames such as `F0 7D 00 11 02 F7`, but its generic MIDI receive library reported each as a two-byte SysEx. The Uno was sending status; the Pico discarded it before updating the connection state. The temporary diagnostic builds were removed after testing.
- **Fix deployed:** the Pico now directly validates the fixed six-byte Uno status and completion frames on its return UART. The Uno also drains up to 48 queued Pico UART bytes per loop so full patch transfers are serviced promptly. Both final sketches compiled and uploads verified. Their hashes are in the [index](README.md#firmware-snapshot).
- **Retest:** the first isolated sync completed with one Uno reset and completion marker, and the Pico returned to **Connected · Pico patch** in about 0.2 seconds. Five subsequent explicit syncs all completed and stayed connected; four reported completion in roughly 0.2–0.35 seconds, and one status confirmation arrived after about 2.1 seconds without an Uno reset retry. After the Uno release build was installed, clean-start automatic patch sync and a final explicit sync passed.
- **Formerly failing action sequence:** headless Chrome ran Randomize, factory Load, user Load, explicit Sync, visualization commands, Panic, Save As dialog, and rejected save checks on the final builds. After ten seconds, status remained **connected / Pico patch**, with no sync failure. The original user slot 3 “Test Pluck” and all 45 live patch fields were restored; saved user files were not overwritten.
- **Still unverified:** exact shifter voltage, forced loss of an acknowledgment, unplugged return wire, audible sound, and per-control Uno sound response.

## LED and audio timing diagnostic — 2026-09-23

- **Observed with the browser and Uno:** selecting Status, VU, or Scope reached the Uno (the `Viz:` label scrolled). The Scope baseline appeared. Browser key presses flashed the Pico UART receive indicator in top-row column 7. The owner did not see the brief Status note flash in column 10. The Pico WebSocket reported **Uno connected · Pico patch** and the current patch had arpeggiator Off.
- **Temporary Uno timing build:** a three-second browser Note On reached the Uno and triggered one voice; its envelope and pre-DAC visualization level increased. The same build counted about **216 audio samples per second** while the source target is **22,050 samples per second**. Timing instrumentation attributed about 0.88 seconds of each wall-clock second to `rtpMIDI.read()` with no RTP session. The underlying WiFiS3 UDP polling makes modem calls for both RTP ports. These are measurements of the temporary diagnostic build, not calibrated release-build audio output.
- **Isolation:** skipping RTP reads in the temporary build raised measured output to roughly **15,000–16,000 samples per second**. Initializing the DAC once and writing subsequent samples directly raised it to roughly **18,000–19,000**. A timer-driven audio prototype with RTP active starved the main loop and was rejected. The synchronous `matrixScrollOnce()` also pauses the main loop during a view label. These tests identify multiple timing constraints; none is a verified release fix.
- **Restoration:** the unchanged release Uno sketch was recompiled and uploaded after the experiments. The Pico WebSocket again reported `connected: true`, `syncFailed: false`, `mode: pico`, with user slot 3 still selected. The output circuit remains unbuilt, so audible behavior is untested.
- **Next engineering check:** redesign audio scheduling and network polling together, then measure actual DAC sample intervals, RTP note/clock delivery, voice envelopes, all three matrix modes, and Pico patch sync under load. Keep the matrix response issue open until the full two-board and audio test passes. [Uno loop and audio output](../QuarkWave_MIDI/QuarkWave_MIDI.ino), [matrix scroll](../QuarkWave_MIDI/QuarkWave_MIDI.ino), [LED display guide](led-display-guide.md).

## Uno matrix response fix — 2026-09-23

- **Code changes:** corrected the state-variable filter recurrence and cached its coefficients, made ADSR advance from attack to decay and sustain, made the note indicator a wall-clock 400 ms flash, replaced live scrolling labels with stationary two-letter labels, initialized the DAC once, and moved chorus tap modulation to the control rate. Idle WiFiS3 RTP polling now runs every 250 ms; a connected RTP session is polled every 50 ms. A 2 ms cap prevents a long audio catch-up burst after blocked work. [Uno source](../QuarkWave_MIDI/QuarkWave_MIDI.ino).
- **Hardware diagnostic:** with the Pico-selected `Test Pluck` patch and no RTP peer, one held browser note produced a finite filter state, a pre-DAC visualization level around 0.09–0.12, and an envelope that settled at sustain 0.147 before release. Status, VU, and Scope commands reached the Uno during the held note. The diagnostic counted approximately 22,050 audio updates per wall-clock second in that one-note test. A separate four-note test still measured about 17,300 updates/s against the 22,050 target; this remains an audio performance limitation.
- **Deployed build:** the clean Uno release sketch compiled and uploaded after the diagnostic. The Pico again reported `connected: true`, `syncFailed: false`, `mode: pico`, with user slot 3 selected. A release-build key and all-three-view smoke sequence finished with the same healthy Pico–Uno link. The internal signals and message paths were checked. The owner then observed a held key lighting a Status voice bar and flashing column 10, with VU and Scope both responding. The unbuilt audio output remains untested.
- **RTP caveat:** polling a connected RTP session every 50 ms reduces idle load but may coalesce high-rate clock pulses. RTP note, clock, transport, and four-voice audio timing still need a dedicated two-board test before claiming those paths are repaired.
