# Connect to QuarkWave over network MIDI

**For:** musicians using a Mac or Windows computer to play the Uno through the Pico. **Status:** source-reviewed; a live RTP invitation, note, and disconnect-release path passed a two-board smoke check on 2026-09-23. The platform procedures below have not been walked through end to end on both operating systems. The owner has confirmed that Logic Pro can use the separate experimental Uno USB-audio input; a Logic-to-Pico RTP performance has not yet been confirmed.

## What connects to what

| Path | Carries | Connection |
| --- | --- | --- |
| Mac or Windows MIDI app → Pico W | Notes, controls, optional MIDI Clock and transport | RTP-MIDI session named **QuarkWave** on UDP port **5004** |
| Pico W → Uno R4 WiFi | Forwarded MIDI | Existing wired MIDI UART |
| Uno R4 WiFi → computer | Mono sound, in the optional USB-audio experiment | USB audio input named **QuarkWave USB Audio**, 22,050 Hz, 16-bit |

The Pico and computer must be on the same reachable network for **RTP-MIDI**. The **Pico**, rather than the Uno, hosts that wireless session. The optional [Uno USB audio/MIDI experiment](../experiments/usb-audio/README.md) instead lets a DAW send MIDI to the Uno and receive its audio through **one Uno USB cable**, without a network MIDI session. The earlier audio-only experiment was confirmed in Logic; the new combined USB MIDI/audio image is source-built but still needs a live enumeration and simultaneous-play check. The Pico accepts **one RTP-MIDI peer at a time**. It advertises `QuarkWave._apple-midi` through mDNS, but you can connect to the Pico's IP address if discovery fails. The browser at `http://quarkwave.local/` shows **RTP-MIDI: Controller connected** only for a network peer; **Pico: Connected** and **Uno: Connected…** are separate indicators. [Pico session and port](../QuarkWave_UI_MIDI/QuarkWave_UI_MIDI.ino), [Pico discovery](../QuarkWave_UI_MIDI/QuarkWave_UI_MIDI.ino), [panel status](../QuarkWave_UI_MIDI/QuarkWave_UI.h).

| Optional direct USB path | Carries | Connection |
| --- | --- | --- |
| Computer → Uno R4 WiFi | Class-compliant USB MIDI 1.0 messages | **QuarkWave USB MIDI** port on the tested Mac, in the composite experiment |
| Uno R4 WiFi → computer | Mono sound at 22,050 Hz, 16-bit | **QuarkWave USB Audio** input on that same cable |

## On a Mac

1. Power the Pico and Uno. Open `http://quarkwave.local/` and check that the panel says the Uno is connected. If the name does not resolve, use the Pico's current IP address.
2. Open **Audio MIDI Setup** in Applications → Utilities. Choose **Window → Show MIDI Studio**, then open **Configure Network Driver** (called **Network** in some macOS versions).
3. Under **My Sessions**, add a session and select **RTP** as its type. Give the *Mac's* session a recognizable name, such as `Mac to QuarkWave`, and turn **Enabled** on. Select **RTP**, not **Network MIDI 2.0**.
4. In **Directory**, select the advertised **QuarkWave** peer and choose **Connect**. If it is missing, add a manual directory entry with the Pico's IP address (or `quarkwave.local` if it resolves) and port **5004**, then connect.
5. Check that QuarkWave appears under **Connected Peers** and that the browser says **RTP-MIDI: Controller connected**. Choose the Mac session as your music app's MIDI output, send one note, and release it. The Uno should respond using its current sound. [Apple's network MIDI instructions](https://support.apple.com/guide/audio-midi-setup/ams1012/mac).

The Mac session's **local port** is its own port; you do not need to set it to 5004. Port 5004 is the **Pico's destination**. If you want to route a hardware keyboard into the network session, use Audio MIDI Setup's **Live routings** or your music app's MIDI routing. Keep that routing off when testing a DAW's own MIDI track so the same note is not sent twice. [Apple's live-routing description](https://support.apple.com/guide/audio-midi-setup/ams1012/mac).

## On Windows

Windows needs an RTP-MIDI implementation that exposes a MIDI port to your music software. The documented route here uses [Tobias Erichsen's rtpMIDI](https://www.tobias-erichsen.de/software/rtpmidi.html). Its published instructions name Windows 7–10; **Windows 11 with this QuarkWave build has not been tested**. Microsoft has separately documented a Windows 11 MIDI Services issue that can hide dynamic ports, including rtpMIDI ports, on affected systems. [rtpMIDI tutorial](https://www.tobias-erichsen.de/software/rtpmidi/rtpmidi-tutorial.html), [Microsoft issue notes](https://devblogs.microsoft.com/windows-music-dev/windows-midi-services-rollout-known-issues-and-workarounds/).

1. Put the Windows computer and Pico on the same reachable network. Power both QuarkWave boards and check the browser's Uno status.
2. Install rtpMIDI from the developer's site and open its configuration app. Follow the installer's Bonjour prompt if it appears; Bonjour helps discover peers on the local network. [Developer installation guide](https://www.tobias-erichsen.de/software/rtpmidi/rtpmidi-tutorial.html).
3. Under **My Sessions**, select **+**, give the Windows session a name such as `Windows to QuarkWave`, and enable it.
4. In **Directory**, select **QuarkWave** and choose **Connect**. If discovery fails, add a manual directory entry using the Pico's current IP address and port **5004**, then connect. Check that QuarkWave appears under **Participants**. [Developer connection guide](https://www.tobias-erichsen.de/software/rtpmidi/rtpmidi-tutorial.html).
5. In your DAW or MIDI controller app, choose the rtpMIDI session you created as the **MIDI output**. Send and release a note. The Pico browser should change to **RTP-MIDI: Controller connected**, and the Uno should play its current sound.

If the session cannot connect, check that Windows treats your local network as **Private** and allows rtpMIDI through its firewall on that network. The Pico's RTP session uses UDP **5004/5005** for control and data. You do not need to expose these ports to the internet. If rtpMIDI's port is absent from the DAW on Windows 11, check the linked [Microsoft MIDI Services issue notes](https://devblogs.microsoft.com/windows-music-dev/windows-midi-services-rollout-known-issues-and-workarounds/) before changing QuarkWave firmware. The Windows path is **source-reviewed, hardware untested**.

## Use Logic Pro with QuarkWave

### Record the sound while playing from the Pico page

This is the shortest audio test and does not need an RTP session. It uses the [experimental USB-audio build](../experiments/usb-audio/README.md), which the owner has confirmed works with Logic Pro.

1. Connect the Uno's USB cable to the Mac. In **Audio MIDI Setup → Window → Show Audio Devices**, check for **QuarkWave USB Audio**. [Apple's audio-device guide](https://support.apple.com/guide/audio-midi-setup/ams1010/mac).
2. In **Logic Pro → Settings → Audio → Devices**, select **QuarkWave USB Audio** as the **Input Device** and your usual speakers or headphones as the **Output Device**. Logic can use separate input and output devices. [Apple's Logic device guidance](https://support.apple.com/en-gb/102171).
3. Create a **mono audio track** with **Input 1**. Turn on its **I** (Input Monitoring) button, then play a note from the Pico page. Confirm that the track meter moves and you hear the note. [Apple's monitoring guide](https://support.apple.com/guide/logicpro/turn-on-input-monitoring-for-audio-tracks-lgcpbfbefa96/mac).
4. Arm the track, press **R**, play, and press **Space** to stop. Play back the resulting audio region. [Apple's recording guide](https://support.apple.com/guide/logicpro/record-sound-a-microphone-electric-instrument-lgcpb19e49e4/mac).

The Uno currently presents a fixed **22.05 kHz** input; Logic may resample it for a project at another rate. The owner's successful Logic test confirms the route worked on their Mac, but does not establish support across other Macs or settings. [USB-audio format](../experiments/usb-audio/UsbAudioCapture.h).

### Sequence the synth from Logic over RTP-MIDI

1. Complete [the Mac RTP connection](#on-a-mac). Leave the Uno USB cable connected if you also want Logic to hear or record its sound.
2. In Logic, set **QuarkWave USB Audio** as the audio input device and your speakers or headphones as the output. Create an **External MIDI** track with **Use External Instrument plug-in** enabled (or load **Utility → External Instrument** on an instrument channel strip). [Apple's External Instrument guide](https://support.apple.com/guide/logicpro/lgcp12e7acdc/mac).
3. In External Instrument, choose your **Mac RTP session** as **MIDI Destination**, **channel 1** for a first test, and **Input 1 (mono)** for **Audio Input**. Turn **Send Program Change** off initially: QuarkWave uses selected Program Change numbers for sound commands, not ordinary factory-preset selection. [Apple's plug-in parameters](https://support.apple.com/guide/logicpro/lgcp12e7acdc/mac), [QuarkWave command table](technical-guide.md#program-change-and-sysex-by-input).
4. Play a MIDI note in Logic. Check the browser's **RTP-MIDI: Controller connected** label, the Uno's note activity, and Logic's audio meter. Record a short MIDI region and play it back. The MIDI region can be edited and replayed; the Uno generates the sound in real time.
5. To capture a fixed audio take, record the Uno's Input 1 on a separate mono audio track, or use Logic's **real-time** project bounce with the external instrument return routed to the output. Do not expect an offline bounce of an external MIDI track to render the Uno. [Apple's bounce guidance](https://support.apple.com/guide/logicpro/lgcp785a41c3/mac).

Avoid monitoring **both** the External Instrument return and a separate record-enabled audio track at once; that can sound doubled. Choose one monitored path while recording the audio take. MIDI Clock and transport forwarding exist in the firmware, but Logic clock timing through the Pico and Uno still needs a dedicated hardware test. [External clock procedure](external-midi-guide.md#follow-midi-clock).

### Sequence and record through the Uno USB cable

This direct path needs the [combined USB audio/MIDI experiment](../experiments/usb-audio/README.md), not the normal Uno image or the earlier audio-only experiment. It does not require an RTP session or the Pico for note input.

1. In **Audio MIDI Setup**, check for **QuarkWave USB MIDI** in MIDI Studio and **QuarkWave USB Audio** in Audio Devices after the Uno is connected. Both names appeared on the tested Mac with the latest combined image. An older **UNO R4 WiFi** MIDI entry may remain in macOS from the earlier image.
2. In Logic, choose **QuarkWave USB Audio** as the input device and your speakers or headphones as output. Create an **External MIDI** track or External Instrument plug-in, set **MIDI Destination** to **QuarkWave USB MIDI**, channel 1, and **Audio Input** to Input 1 (mono).
3. Leave automatic Program Change sending off for the first test. Send and release a MIDI note. Check that the Uno matrix responds and that the Logic audio meter moves while the track is monitored. Record a short MIDI region, play it back, then record the returning audio in real time.

USB MIDI sound messages can put the Uno into **standalone sound** state, so a later Pico connection will preserve them until an explicit sync. MIDI Clock alone does not. When DIN, direct USB, and Pico RTP all send recent valid clock, the Uno prioritizes them in that order. [USB MIDI receiver](../experiments/usb-audio/UsbMidiInput.h), [Uno clock selection](../QuarkWave_MIDI/QuarkWave_MIDI.ino).

The combined image passed a Mac test that sent a USB MIDI note while recording nonzero USB audio and kept the Pico–Uno link connected. The Logic Pro steps above still need a user check with the combined image; the earlier audio-only image was confirmed in Logic. [Combined test record](hardware-test-log.md#composite-uno-usb-midi-and-audio--2026-09-23).

## If the connection or notes fail

| Symptom | Check |
| --- | --- |
| No **QuarkWave** peer appears | Open the Pico web page to verify its network connection; use a manual directory entry with the Pico IP and port 5004. Discovery and the actual MIDI connection are separate. |
| Peer connects, but there is no sound | Check **Uno: Connected…**, the current sound, Logic/DAW MIDI output, and channel. The Pico browser can send a test note independently. If a standalone sound is preserved, use **Sync Pico patch to Uno** only when you intend to replace it. |
| Browser says **No controller** | The RTP invitation has not completed. Verify the peer is under **Connected Peers** (Mac) or **Participants** (Windows), and that only one controller is connected. |
| A note remains on after a wireless dropout | The Pico releases its RTP-held notes when it detects session disconnection; a silent network failure may take longer. Use **Panic** on the Pico page for immediate release. [Disconnect behavior](../QuarkWave_UI_MIDI/QuarkWave_UI_MIDI.ino). |
| Logic MIDI moves, but its audio meter is still | Select **QuarkWave USB Audio** as input and **Input 1 (mono)** on the track or External Instrument. The USB-audio firmware is an optional experiment, so a normally flashed Uno will not appear as this audio device. |

For the complete MIDI message map and how the Pico keeps the Uno's standalone sound, see [External MIDI controllers](external-midi-guide.md) and the [Technical guide](technical-guide.md). Report platform-specific results in the [Hardware test log](hardware-test-log.md).
