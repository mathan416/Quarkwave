# QuarkWave hardware test log

**Purpose:** record repeatable results for the assembled Pico W and Uno R4. The guides are **source-reviewed, hardware untested** until the relevant checks pass. Copy this page for each firmware and wiring revision; record actual observations rather than replacing expectations with a checkmark. The dated records below are chronological: older entries describe earlier builds and are superseded by later entries, not by edits to their measurements.

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
| Perform / Shape / Explore / All controls / Help | Controls keep values; held browser notes continue across view changes. Help pages make no sound change. | |
| Factory Load and user Save As | Factory sounds stay read-only; Save As chooses a slot and confirms occupied replacement. | |
| User Save, Load, restart | Saved file, selected slot, and recovered sound agree; failure does not change selection. | |
| Commit / Load committed sound | Snapshot can be saved and restored separately from user slots. | |
| Randomize | Returned panel values remain inside each control's range, including effects. | |
| Panic and visualizations | Panic silences notes; Status, VU, and Scope select the intended Uno LED view. | |
| DIN without Pico | Notes, bend, sustain, and CC work with Uno defaults; no Pico is required. | |
| RTP-MIDI through Pico | The Pico's `QuarkWave` session accepts notes and sound controls, then forwards them to the Uno. Without the Pico, no RTP-MIDI session exists in this build. | |
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

## Nonblocking matrix scroll — 2026-09-23

- Restored full scrolling labels for Status, VU, Scope, and RTP connection. Each frame is drawn during the normal LED update slot; MIDI and audio continue during the animation.
- A temporary timing build showed **Viz: VU** moving from x=6 through x=-34 while a held browser note maintained a pre-DAC level around 0.09–0.11. It counted about 19,700 audio updates/s during the scroll, matching an instrumented build of the previous stationary-label firmware. The diagnostic itself changes the measured rate, so this comparison does not establish the release audio sample rate.
- The clean scrolling build compiled and was installed on the Uno. The unbuilt audio output remains untested. [Uno matrix code](../QuarkWave_MIDI/QuarkWave_MIDI.ino).

## Four-voice audio stress and optimization — 2026-09-23

A temporary Uno build counted successful DAC updates over a five-second window after boot and Pico patch sync. It triggered four held notes (48, 55, 60, 64) directly in the Uno engine at velocity 100. The effects stress used chorus mix, bitcrush mix, tremolo depth, drive, fold, and delay mix at 1.0, bitcrush at 4 bits with 16-sample hold, delay feedback 0.8, and tempo-synced delay. The current Pico boot patch supplied other settings. The same counter and test sequence were used for the before-and-after comparison; diagnostic builds are not calibrated DAC interval measurements.

| Build and load | Updates in 5 s | Effective update rate |
| --- | ---: | ---: |
| Previous firmware · four voices × three unison · all stress effects | 44,220 | 8,844/s |
| Optimized firmware · four voices × three unison · all stress effects | 65,656 | 13,131/s |
| Optimized firmware · four voices × three unison · effects off | 73,596 | 14,719/s |
| Optimized firmware · four voices × one oscillator · all stress effects | 95,761 | 19,152/s |

The full-load improvement is about 48%, but every measured load above remains below the 22,050-updates/s target. The largest remaining cost is twelve active oscillators; all-effects processing also consumes time. The firmware still requests 22,050 updates/s and drops overdue work after a long block. These figures do not claim an achieved 22.05 kHz output during the stress test.

The release change keeps the delay's 8-bit, 11,025 Hz ring so its maximum synced time and current sound resolution are retained. A fixed delay rate also prevents its RAM use from growing with a future oscillator-rate change. A separate 44.1 kHz compile now fits RAM, whereas the prior source failed to link; this is a compile-only check, and the measured CPU load does not support changing the deployed rate. The tap length, envelope increments, waveform morph segment, bitcrush settings, and tremolo step now avoid redundant per-sample calculations; ring wrap uses a comparison instead of division. Waveform blend and delay ring arithmetic were checked against the previous equations over 100,000 morph cases and ring endpoints. The clean 22.05 kHz Uno sketch compiled, was uploaded, and the Pico reported `connected: true`, `syncFailed: false`, `mode: pico`; a brief browser note test completed. The line-output circuit is unbuilt, so audible quality and DAC timing remain unverified. [Uno source](../QuarkWave_MIDI/QuarkWave_MIDI.ino).

## Four-voice performance refinement — 2026-09-23

The same five-second direct-voice stress test described above was repeated with four notes, three unison oscillators per note, and all listed effects stressed. The sketch's requested rate stayed **22,050 updates/s** and its control rate stayed **1 kHz**. The numbers below count completed DAC writes in temporary instrumented builds; they do not establish evenly spaced sample intervals or audible quality.

| Candidate | Updates in 5 s | Effective update rate | Decision |
| --- | ---: | ---: | --- |
| Previous release | 65,656 | 13,131/s | Comparison baseline |
| Integer oscillator phases, refreshed at control rate | 81,236 | 16,247/s | Retained |
| Integer waveform blend added | 91,971 | 18,394/s | Retained |
| Standard `O3` optimization added | 98,197 | 19,639/s | Retained |
| Bounded audio rounding and LFO math added | 99,883 | 19,977/s | Retained |
| RTP-MIDI compiled out for comparison | 107,318 | 21,464/s | Measurement only; RTP remains enabled |
| Control rate reduced to 500 Hz | 100,074 | 20,015/s | Rejected; negligible gain with lower timing resolution |

The retained changes move oscillator phases with 32-bit integer increments, calculate pitch bend, detune, vibrato, and glide increments at the existing 1 kHz control rate, blend waveform table values as integers, use bounded rounding for audio samples, and replace small-exponent LFO calculations with accurate polynomial/table approximations. A 100,000-case comparison found the integer waveform blend within **0.000116** of the previous normalized waveform; sine-table interpolation was within **0.000082** of `sin()` in a 100,000-phase comparison, and the small-exponent polynomial's maximum error over the two-semitone vibrato range was below **0.000000004**. Glide now receives phase-increment updates every millisecond; its audible behavior remains to be checked.

A filter tangent lookup, drive-curve lookup, and one-step wavefold simplification showed no useful rate gain and were discarded. Two attempts to remove WiFiS3 UDP modem queries raised the benchmark but failed a real RTP invitation, so neither is in the release. The standard RTP path accepted control and data invitations, a test RTP note marked the Uno as **standalone sound** on the Pico, and an explicit patch sync returned to **Pico patch**. The clean optimized Uno sketch compiled and was uploaded. A browser note plus Status, VU, and Scope commands completed with the Pico reporting `connected: true`, `syncFailed: false`, `mode: pico`.

**Open limit:** the demanding four-voice/all-effects case still produced about **19,977 updates/s**, below the **22,050/s** target. With RTP compiled out it reached about **21,464/s**, also below target. Connected RTP-MIDI and actual DAC interval timing have not been characterized, and the audio output circuit is still unbuilt. Do not describe 22.05 kHz as achieved under all loads. Further work needs cycle-level profiling and a real-time audio scheduling/network design that preserves RTP clock and MIDI latency. [Uno source](../QuarkWave_MIDI/QuarkWave_MIDI.ino).

## Pico RTP-MIDI gateway smoke check — 2026-09-23

Both current sketches compiled. The Uno upload completed; the Pico's serial upload entered its USB bootloader but did not expose a mounted drive to `arduino-cli`, so `picotool load -v -x` was used to verify and start the compiled Pico image while preserving the configured LittleFS partition. The Pico then served HTTP 200 from its recorded local IP. A local RTP-MIDI test sent a control invitation to UDP 5004, received AppleMIDI `OK`, sent the data invitation, and sent Note On/Off. A simultaneous browser WebSocket changed from `Uno: Connected · Pico patch` to `Uno: Connected · standalone sound`, and its new `rtpConnected` field changed from false to true. An explicit `syncUno` request then produced `syncing` followed by `Connected · Pico patch` with `syncFailed: false`. A second test sent Start, 25 Clock pulses at approximately 120 BPM, and Stop; the Uno remained `Connected · Pico patch`, as expected because clock alone does not mark standalone sound. Closing each RTP session returned `rtpConnected` to false. The last check reported `connected: true`, `syncFailed: false`, `rtpConnected: false`, `mode: pico`. This confirms the local network parser, gateway activity marker, return link, and explicit-sync acknowledgment; it does not establish audible note output or clock accuracy. The physical DIN connector is still unbuilt.

## Uno network-free performance and wireless disconnect — 2026-09-23

The prior five-second direct-voice stress was repeated on the Uno with both `USE_WIFI_STACK=0` and `USE_RTP_MIDI=0`. Four notes (48, 55, 60, 64) used three unison oscillators each. Chorus mix, bitcrush mix, tremolo depth, drive, fold, and delay mix were 1.0; bitcrush used 4 bits and a 16-sample hold; delay feedback was 0.8 with tempo sync. The current Pico boot patch supplied other settings. The temporary build counted **110,251 DAC writes in 5,000 ms**, or **22,050.2 writes/s**. The extra write is a window-edge count; this meets the requested *average update count* of 22,050/s for this diagnostic.

A second temporary build instrumented scheduling lateness. It counted **110,250 writes in 5,000 ms**, with **6,775** writes starting over 45 µs late, **2,381** over 100 µs late, a **1,734 µs** worst observed lateness, and **zero** audio catch-up resets. Instrumentation itself adds timing cost. A comparison with matrix redraws suppressed counted 110,248 writes, 5,032 over 45 µs late, 2,214 over 100 µs late, and a 1,723 µs worst lateness. The small improvement did not justify degrading the LED display; normal redraws remain enabled. These counters show throughput and scheduling behavior in diagnostic builds, not DAC waveform timing or audible quality. The line-output circuit is still unbuilt.

The Pico and Uno were updated with source-aware note and sustain handling. The Pico AppleMIDI session now accepts one wireless controller, tracks held notes and pedal state by MIDI channel, and sends source-tagged releases on disconnect. The Uno tracks browser, DIN, and wireless ownership separately. A live local RTP test received an invitation and forwarded a note; a controller `BY` disconnect changed `rtpConnected` from true to false. A temporary Uno diagnostic logged one Note On and one engine Note Off for a wireless note released by disconnect. With a browser note held on the same pitch, it logged two Note Ons and only one engine Note Off after the later browser release, confirming that disconnect preserved browser ownership. A second diagnostic logged wireless pedal ownership changing `4 → 0` on disconnect and shared browser/wireless ownership changing `5 → 1 → 0` as the wireless session and then the browser pedal released. The production Uno image was then restored. Both updated sketches compiled and were uploaded; the release-build link was checked after restoration. A silent Wi-Fi loss without `BY` depends on the AppleMIDI session timeout and has not been timed on hardware. [Pico gateway](../QuarkWave_UI_MIDI/QuarkWave_UI_MIDI.ino), [Uno note state and audio loop](../QuarkWave_MIDI/QuarkWave_MIDI.ino).

## Clearer Uno connection label — 2026-09-23

The browser's `mode: pico` status previously read **Uno: Connected · Pico patch**. The Pico/Uno protocol and mode value are unchanged; the panel now reads **Uno: Connected to Pico · sound loaded**. “Connected to Pico” describes the recent two-way MIDI handshake, and “sound loaded” describes the Uno's acknowledgment of the Pico's selected sound settings. The standalone, loading, awaiting-sound, offline, and sync-failure labels were adjusted to use the same language. The updated Pico sketch compiled and was uploaded. A live browser WebSocket reported `connected: true`, `mode: pico`, `syncFailed: false`, and the served HTML contained the new label. Fresh Perform, Shape, Explore, All controls, and staged Save As captures now show that wording. The captures do not establish audio output or save a patch. This is a historical label; the current source uses **Uno: Connected to Pico**. [Panel label](../QuarkWave_UI_MIDI/QuarkWave_UI.h), [connection meaning](technical-guide.md#browser-pico-interface).

## Multi-bar Uno VU display — 2026-09-23

The previous VU view drew one horizontal output-level bar and a peak pixel. The new view draws **11 vertical bars** of recent mono pre-DAC output levels, oldest left and newest right, plus a peak dot in column 12. The top-row indicators and bottom-right timing diagnostic remain available. This is a short loudness history, not a frequency spectrum or a stereo meter. The LED guide's three-view SVG was refreshed as an illustration, not a photograph. [VU drawing](../QuarkWave_MIDI/QuarkWave_MIDI.ino), [LED guide](led-display-guide.md).

The Uno sketch compiled. In the same temporary five-second four-voice, three-unison, all-effects stress described above, with VU selected, it completed **110,248 DAC writes in 5,000 ms** against the 110,250-write target. The normal firmware was restored and uploaded. A live browser `visualization` message selected mode `1`, the Uno USB log reported `[Viz] Viz: VU`, a browser test note was released, and the Pico still reported `connected: true`, `mode: pico`, `syncFailed: false`. This verifies the command and average update count; the moving bars have not yet been visually checked on the physical matrix, and the audio output circuit remains unbuilt.

## Uno top-row status indicators — 2026-09-23

The Uno top row now uses columns 1–5 for Pico sound loaded, external MIDI used, valid external clock, sustain, and arpeggiator enabled. Column 8 briefly shows a voice steal, column 11 briefly shows an audio scheduler reset after more than 2 ms of lateness, and the note-start flash in column 10 is now drawn in Status, VU, and Scope. The existing Pico link, Pico receive, external activity, heartbeat, and bottom-right display timing signals remain. The top-row SVG and LED guide describe the exact conditions. [Uno source](../QuarkWave_MIDI/QuarkWave_MIDI.ino), [LED guide](led-display-guide.md).

The Uno sketch compiled for **Arduino UNO R4 WiFi** (117,908 bytes flash, 16,300 bytes global RAM) and uploaded successfully over USB. The uploader reported the same serial port afterward. The local shell could not reach `quarkwave.local` or the previously recorded Pico IP, so the Pico handshake and the new physical LED patterns were not checked after this upload. Hardware acceptance still needs: sync a Pico patch; use an external MIDI controller; send a valid external clock; toggle sustain and arpeggiator; overlap five notes to see voice stealing; and exercise a demanding patch while watching for audio-slip flashes. A forced scheduler delay would test column 11, but that diagnostic should use a temporary build and the normal firmware should be restored afterward. Audible effects remain untested until the audio output is built.

## Simpler Uno connection label — 2026-09-23

The browser source now displays **Uno: Connected to Pico** for its normal acknowledged-patch state; the previous label added “sound loaded.” The standalone, loading, awaiting-sound, offline, and failure labels remain distinct. The connection state and Pico–Uno MIDI protocol are unchanged. All five panel guide images were rendered again from the current embedded page with source-defined Warm Pad settings and a simulated connected status. The page was not uploaded in this change, so these images are not a live post-upload test. [Panel status](../QuarkWave_UI_MIDI/QuarkWave_UI.h), [guide index](README.md).

## Logic Pro USB-audio owner check — 2026-09-23

After the corrected [USB-audio experiment](../experiments/usb-audio/README.md) was running, the owner reported that it **worked beautifully** in Logic Pro using the Uno as the USB audio input. This confirms that the Mac/Logic route worked for this setup. The report did not specify the Logic project rate, buffer size, whether MIDI came from the Pico browser or an RTP session, or the duration and effects used. Do not treat those details as verified. The [Network MIDI setup guide](network-midi-setup.md) gives a repeatable Logic audio and optional RTP procedure for follow-up testing.

## Ten-page onboard Help — 2026-09-23

The embedded panel now contains seven User Guide pages and three Technical details pages, all in the Pico's existing self-contained HTML. Browser checks on a source-rendered preview opened each page, verified the active navigation state and heading focus, held and released a computer-key note across page changes, and confirmed that a changed control value survived navigation. All ten topics fit desktop (1440 px), tablet (768 px), and phone (390 px) widths without document-level horizontal overflow. The final Pico sketch compiled with the expanded Help at 581,164 bytes of flash and 75,940 bytes of global RAM. Six guide images were rendered again with Warm Pad and simulated connection state. This Help update has **not** been uploaded to the Pico or accepted on physical hardware. The Markdown guides still hold the complete schematics, control-value tables, and test evidence; PDFs were not regenerated. [Embedded Help](../QuarkWave_UI_MIDI/QuarkWave_UI.h), [guide index](README.md).

## Composite Uno USB MIDI and audio — 2026-09-23

The [optional composite build](../experiments/usb-audio/README.md) adds TinyUSB MIDI 1.0 to the established USB audio and serial experiment. The normal Uno source and the staged composite source both compiled. The composite used 123,168 bytes of flash and 17,776 bytes of global RAM. Host-side packet tests passed for note ownership, sustain, CC, bend, Clock and transport, accepted sound commands, ignored Pico-only commands, and USB disconnect release. These host tests do not substitute for a live unplug or clock-timing test.

The image uploaded successfully to the Uno. macOS CoreMIDI listed **UNO R4 WiFi** as both a MIDI destination and source; CoreAudio listed **QuarkWave USB Audio** as an input. A CoreMIDI test sent Note On `90 3C 64` and Note Off `80 3C 00` to the Uno while a CoreAudio test recorded 66,560 mono samples over 3.02 seconds at 22,050 Hz from the same USB cable. Both MIDI sends returned success. The first second was silent, the note interval reached sample peak 17,835, and the recording decayed after release. This demonstrates simultaneous USB MIDI reception and USB audio return for one note, not a long-duration performance bound.

Afterward, the Pico still served HTTP 200. Its WebSocket reported `connected: true`, `syncFailed: false`, and `mode: standalone`, consistent with the USB note marking standalone sound activity while the Pico–Uno handshake remained healthy.
The test was repeated with a 15-second held USB MIDI note during a 20.02-second recording: 441,344 samples were captured, the signal remained nonzero throughout sampled windows of the hold, and it decayed after Note Off. The Pico again reported `connected: true` and `syncFailed: false`. The direct USB route has not yet been tested inside Logic Pro; the owner's earlier Logic check used the audio-only image. Heavy four-voice patches, USB disconnect release, external USB clock, Windows enumeration, and the physical A0 circuit remain open. [USB MIDI receiver](../experiments/usb-audio/UsbMidiInput.h), [Uno source](../QuarkWave_MIDI/QuarkWave_MIDI.ino), [documentation gaps](documentation-gaps.md).

## USB MIDI port name — 2026-09-23

The first composite image advertised **QuarkWave USB MIDI** as its MIDI interface string, but macOS CoreMIDI displayed the Uno's old product name, **UNO R4 WiFi**, even after its USB product string was changed and CoreMIDI was asked to rescan. The optional build now uses **QuarkWave USB MIDI** as the USB product string and a distinct, stable serial suffix for this firmware; the Uno's normal firmware and hardware unique ID source are unchanged. After upload, CoreMIDI listed **QuarkWave USB MIDI** as both MIDI destination and source. A Note On/Off pair sent through that destination produced 66,560 captured mono audio samples with a 17,835-count peak, and the Pico WebSocket still reported `connected: true` and `syncFailed: false`. The old Mac MIDI device entry may remain cached, but the new entry is the one to select in Logic. [Composite build](../experiments/usb-audio/build.py), [Logic setup](network-midi-setup.md#sequence-and-record-through-the-uno-usb-cable).

## USB MIDI/audio regression suite — 2026-09-23

The final Uno and Pico sketches compiled, the optional USB composite sketch compiled, and the host USB MIDI parser test passed. macOS still listed **QuarkWave USB MIDI** and **QuarkWave USB Audio**. The Pico served its page, and WebSocket status remained connected with `syncFailed: false` after the live tests. The browser's current source was tested in an isolated headless preview: **50 controls each sent a message**, all **41 desktop on-screen keys** sent Note On/Off pairs, and all **12 computer keys** sent Note On/Off at velocity **1, 100, and 127**. Switching views retained a held note and its keyboard, and key release still worked with the velocity slider focused. Load, Randomize, Commit, Load Commit, Panic, visualization selection, empty-slot Save As, ten Help pages, and 1440/768/390 px layouts passed. The preview used a fake WebSocket, so it proves browser messages and layout, not every Uno control's audible result. No user patch file was overwritten. [Browser page](../QuarkWave_UI_MIDI/QuarkWave_UI.h), [USB MIDI test](../experiments/usb-audio/test_usb_midi.cpp).

For the audio-rate checks, the optional build counted writes to the Uno DAC register, USB audio packets, ring underruns/overruns, and audio scheduler slips. Counters were compared over steady intervals after startup; their absolute boot totals were excluded. Every 1,000 USB audio packets represents about one second of host streaming. On the restored Pico patch, **7,000 packets** coincided with **154,337 DAC writes**: **22,048.1 writes/s**, 0 new underruns, 0 overruns, and 0 slips. This is 0.0084% below the 22,050/s target over the interval. It checks average writes, not spacing of individual DAC edges. [Audio scheduler](../QuarkWave_MIDI/QuarkWave_MIDI.ino), [USB packetizer and diagnostics](../experiments/usb-audio/UsbAudioCapture.h).

The demanding patch used four USB MIDI notes (48, 55, 60, 64), three-way unison, chorus, delay, bitcrush, tremolo, drive, and wavefold at high settings. A 20.02-second CoreAudio capture returned **441,344 samples** with nonzero audio during held notes and no full-scale clipped samples. During a 15,000-packet steady interval of that capture, the Uno made **319,824 DAC writes**, equivalent to **21,321.6 writes/s**. The ring reported **10,959 new underruns**, 0 overruns, and **240 scheduler slips**. The USB packetizer fills missing samples with zeros, so the host's nominal capture length masks this synth-rate failure. In a 12-second held-note section, 3.9% of recorded samples were exactly zero and the longest zero run was 39 samples (1.77 ms); a simpler held note in the same interval had 0.05% zero samples and no run longer than one sample. The captured signal should be listened to before assessing audible severity. USB servicing accounted for about 1.4% of elapsed time in a related five-second stress interval, so slower polling alone cannot recover the missing work. [Performance gap](documentation-gaps.md), [USB audio implementation](../experiments/usb-audio/UsbAudioCapture.h).

Short four-voice tiers showed the cause is cumulative: unison alone, chorus/delay, bitcrush alone, tremolo alone, and drive/fold alone each stayed near the 22.05 kHz target apart from brief note-on transients. Bitcrush plus drive/fold dropped to about **21.15 kHz** before optimization. The current sound engine reuses a bitcrush quantization result during a sample-hold period and skips interpolation at full effect mix; this preserves the steady mathematical output while reducing work. It improved the combined insert-effects tier from roughly **20.94 to 21.3 kHz**, but did not resolve it. A temporary `-O2` composite build was slower in the same tier and was replaced with the standard build. After every stress run, explicit Pico sync returned `connected: true`, `mode: pico`, and `syncFailed: false`; the owner's live Pico patch remains selected. The composite build has not yet had a hands-on Logic Pro, Windows, USB-disconnect, USB-clock, or physical A0 output check.

## Help view color and Pico deployment — 2026-09-23

The Help view now uses a rose accent and plum navigation/cards, distinct from the All controls mint palette. Its source-rendered connected-state screenshot was regenerated. The Pico W sketch compiled with the existing 1 MB LittleFS layout. Before upload, a full 2 MB flash backup was saved at `/private/tmp/quarkwave-pico-pre-help-theme.bin`; the UF2 program ended at `0x10092700`, below the filesystem boundary at `0x10100000`. Picotool uploaded and verified the image, then rebooted the Pico. The live page returned HTTP 200 and contained the new Help palette. WebSocket reported current user slot 3 and `connected: true`, `mode: pico`, `syncFailed: false` for the Uno. Visual appearance was reviewed in a 1440 px source-rendered capture; an owner check on the actual browser remains useful.

## Complete onboard guides and LED images — 2026-09-23

The Pico Help center was regenerated from nine full Markdown guides: Quick start, Musician guide, Patch book, Sound design lessons, Uno LED display, External MIDI, Mac/Windows/Logic, Technical guide, and Connection guide. The Uno LED page includes the owner-supplied board photograph plus the mode and numbered-top-row diagrams. The Connection guide also carries the current-wiring and proposed-audio figures. Help pages and SVG diagrams are gzip-compressed in program flash; JPEG thumbnails are served locally. No internet connection or PDF is needed to read them from the Pico. [Generator](../tools/build-onboard-help.cjs), [generated routes](../QuarkWave_UI_MIDI/QuarkWave_Help.h).

A local browser preview loaded all nine full pages, all 20 image references, and 68 internal guide links with no missing target. It checked cross-guide navigation, a held keyboard state through tab changes, and layouts at 1440, 768, and 390 px without document-level horizontal overflow. The three Uno LED images loaded with nonzero dimensions. The final Pico sketch compiled at **887,340 / 1,044,480 bytes (84%) program flash** and **75,940 / 262,144 bytes (28%) global RAM**. This preview does not establish live Pico response timing or audio behavior; record the post-upload check separately. The earlier ten-page Help result above describes the previous firmware snapshot.

**Live Pico check after upload:** The Pico firmware upload verified and restarted. At `http://quarkwave.local/`, the page and compressed `/help/start`, `/help/led`, and `/help/technical` routes returned HTTP 200. The live LED image routes returned HTTP 200 for the photograph and both SVG diagrams. A headless browser opened the Pico at its resolved local address, reported **Pico: Connected**, **Uno: Connected to Pico**, and the preserved **Test Pluck** patch, loaded all three LED images with nonzero dimensions, and opened the complete Technical guide. This confirms the browser rendering and board handshake after this Help deployment; audible output and per-pixel LED timing are outside this documentation check.

## Musician and technical guide rewrite — 2026-09-23

The nine onboard guides were rewritten around musician tasks and a narrative technical tour. Protocol tables and schematic distinctions remain in the technical and connection guides; test history stays here. The generated Help passed local browser checks for all nine pages, 39 internal links, 19 image references, desktop/tablet/phone widths, and held keyboard state through view changes. The final Pico build compiled at **874,628 / 1,044,480 bytes (83%) program flash** and **75,940 / 262,144 bytes (28%) global RAM**. After a full-flash backup and verified upload, the live Pico served all nine rewritten pages and the three Uno LED images. A browser showed **Pico: Connected**, **Uno: Connected to Pico**, and the preserved **Test Pluck** user patch; the LED and technical guides opened with their images. This was a documentation deployment, with no Uno firmware change or new audio acceptance test.

## Onboard Help serving check — 2026-09-23

The owner reported that opening the Connection guide appeared to crash the Pico. Before restarting it, USB serial continued printing the periodic mDNS health message, so the main loop was still running. HTTP requests to the main page, a small status endpoint, and multiple Help pages stalled or took 7–12 seconds; the slowdown was not limited to the Connection guide. A short ping sample lost one of five packets. This establishes an intermittent serving/network stall, not a confirmed firmware reboot or memory fault.

The Mac, Windows, and Logic guide was rewritten in the same task-focused style as the other Help pages. The browser now stops a Help fetch after 20 seconds so a stuck request can be retried, decodes Help images asynchronously, and retries a failed image once. A read-only health response now reports Pico uptime, free heap, and Wi-Fi RSSI. The updated Pico sketch compiled at 875,404 bytes flash and 75,940 bytes global RAM, then uploaded and verified.

After the upload, the Pico reported RSSI **−38 dBm** and **175,576 free heap bytes**. The Connection guide and its first images loaded in a headless browser; all eight illustrations were present. Five more concurrent asset-load cycles left the health endpoint responsive, with one truncated JPEG response among 45 guide/asset requests. The failure did not repeat as a full HTTP stall after reboot. The exact cause of the earlier stall remains undetermined; repeat the test after longer uptime and with the owner's normal browser session open. [Help loader](../QuarkWave_UI_MIDI/QuarkWave_UI.h), [asset generator](../tools/build-onboard-help.cjs), [Pico health endpoint](../QuarkWave_UI_MIDI/QuarkWave_UI_MIDI.ino).
