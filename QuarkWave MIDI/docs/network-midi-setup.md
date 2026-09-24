# Make music with a Mac or Windows computer

Your computer can play QuarkWave and record what the Uno makes. Choose how you want to send notes, then choose how you want to hear the sound. These are separate connections: a working MIDI link does not carry audio to your speakers.

| If you want to… | Send notes with… | Hear or record with… |
| --- | --- | --- |
| Play from the QuarkWave browser | The on-screen keyboard through the Pico | **QuarkWave USB Audio** on the Uno |
| Play from a Mac or Windows music app without a MIDI cable | An RTP-MIDI session over Wi-Fi to the Pico | **QuarkWave USB Audio** on the Uno |
| Use one Uno USB cable for both MIDI and audio | **QuarkWave USB MIDI** directly to the Uno | **QuarkWave USB Audio** on the same cable |

The USB route needs the combined USB audio/MIDI firmware on the Uno. The photographed breadboard does not yet have its proposed physical A0 line output, so USB audio is the available listening path for this build. Its input is mono, 16-bit, 22,050 Hz. Start with your speakers or headphones at a low level.

## Connect over Wi-Fi

The Pico hosts an RTP-MIDI session named **QuarkWave**. It accepts one computer or controller at a time. Put the computer and Pico on a network where they can reach each other. The Pico's destination port is **5004**; your computer's session has its own local port.

When the connection succeeds, the browser panel says **RTP-MIDI: Controller connected**. Its **Pico** and **Uno** labels describe the browser and wired board connections separately.

### On a Mac

1. Power both boards and open `http://quarkwave.local/`. Check that the panel says **Uno: Connected to Pico**.
2. Open **Audio MIDI Setup** from Applications → Utilities. Choose **Window → Show MIDI Studio**, then open **Configure Network Driver**. Some macOS versions call it **Network**.
3. Add a session under **My Sessions**. Choose **RTP**, give it a name such as `Mac to QuarkWave`, and enable it. Choose RTP rather than Network MIDI 2.0.
4. Select **QuarkWave** in the **Directory** and click **Connect**. If it does not appear, add the Pico's IP address with port **5004** manually.
5. Check that QuarkWave appears under **Connected Peers**. In your music app, choose the *Mac session* as its MIDI output and play a note.

You should see **RTP-MIDI: Controller connected** in the Pico panel and MIDI activity on the Uno. If the same keyboard is routed through two tracks or sessions, a note may sound twice. [Apple's Network MIDI instructions](https://support.apple.com/guide/audio-midi-setup/ams1012/mac) show the Mac session window.

### On Windows

Windows needs an RTP-MIDI app to make the network session appear as a MIDI port. [rtpMIDI by Tobias Erichsen](https://www.tobias-erichsen.de/software/rtpmidi.html) is one option.

1. Power both boards, connect the computer and Pico to a reachable network, and open the QuarkWave panel.
2. Install and open rtpMIDI. If the installer asks for Bonjour, install it so the Pico can appear in the Directory.
3. Add a session under **My Sessions**, name it something like `Windows to QuarkWave`, and enable it.
4. Select **QuarkWave** in the **Directory** and click **Connect**. If it is missing, add the Pico's IP address and port **5004** manually. Check that QuarkWave appears under **Participants**.
5. In your music app, choose this rtpMIDI session as the **MIDI output**. Play and release a note.

The Pico should show **RTP-MIDI: Controller connected** and the Uno should show note activity. If the invitation fails, allow rtpMIDI through the computer's local-network firewall. The RTP session uses UDP **5004/5005** on the Pico; it does not need an internet port forward. The [rtpMIDI tutorial](https://www.tobias-erichsen.de/software/rtpmidi/rtpmidi-tutorial.html) shows the Windows session controls. On Windows 11, a port that does not appear in a music app may also be affected by [Windows MIDI Services port visibility](https://devblogs.microsoft.com/windows-music-dev/windows-midi-services-rollout-known-issues-and-workarounds/).

## Use Logic Pro with QuarkWave

Logic can record a browser performance as audio, send editable MIDI notes to QuarkWave, or do both through the Uno USB cable. Set a comfortable monitoring level before you start.

### Record the sound while playing from the Pico page

Choose this when you want to play the browser keyboard and keep an audio take. You do not need an RTP-MIDI session.

1. Connect the Uno's USB cable to the Mac. In **Audio MIDI Setup → Window → Show Audio Devices**, find **QuarkWave USB Audio**.
2. In **Logic Pro → Settings → Audio → Devices**, choose QuarkWave USB Audio as the **Input Device** and your usual speakers or headphones as the **Output Device**.
3. Create a **mono audio track** using **Input 1**. Turn on **Input Monitoring (I)**. Play a note on the Pico page; you should hear it and see Logic's track meter move.
4. Arm the track, record a phrase, then play the region back.

Logic may convert the Uno's 22.05 kHz stream to the project's sample rate. See [Logic audio device settings](https://support.apple.com/en-gb/102171) and [input monitoring](https://support.apple.com/guide/logicpro/turn-on-input-monitoring-for-audio-tracks-lgcpbfbefa96/mac).

### Sequence the synth from Logic over RTP-MIDI

Choose this when you want to edit notes in Logic and hear the Uno play them over the network. Keep the Uno USB cable connected for audio.

1. Complete the [Mac RTP-MIDI connection](#on-a-mac).
2. Set Logic's audio input to **QuarkWave USB Audio** and its output to your listening device. Add an **External MIDI** track with the **External Instrument** plug-in.
3. Set **MIDI Destination** to your *Mac RTP session*, channel **1**, and **Audio Input** to **Input 1 (mono)**. Leave **Send Program Change** off for your first test; QuarkWave uses some Program Changes as sound commands.
4. Play or record a MIDI region. Edit a few notes and play it again. Logic sends the notes through the Pico; the Uno makes the sound.
5. Record the return on a mono audio track when you want a permanent audio take. Use a real-time bounce if you bounce the external return; an offline bounce cannot capture an external instrument playing in real time.

Monitor the External Instrument return *or* the recording track to avoid hearing the same signal twice. [Logic's External Instrument guide](https://support.apple.com/guide/logicpro/lgcp12e7acdc/mac) explains its routing.

### Sequence and record through the Uno USB cable

Choose this when you want the shortest path from Logic to the Uno. The Pico can stay connected for the browser panel and patches, but it is not needed for these notes.

1. In Audio MIDI Setup, find **QuarkWave USB MIDI** in MIDI Studio and **QuarkWave USB Audio** in Audio Devices.
2. In Logic, select QuarkWave USB Audio as the audio input. Add an External MIDI track or External Instrument plug-in. Set **MIDI Destination** to **QuarkWave USB MIDI**, channel **1**, and **Audio Input** to **Input 1 (mono)**.
3. Leave automatic Program Change off at first. Play a note; check the Uno matrix and Logic's audio meter. Record a MIDI phrase, replay it, then capture the audio in real time.

The Uno USB connection can carry audio, MIDI, programming, and serial traffic together with the [combined firmware](../experiments/usb-audio/README.md).

## If something is missing

| What you notice | Try this |
| --- | --- |
| QuarkWave is absent from the network Directory | Open the Pico page to check its Wi-Fi connection. Then add its IP address and port **5004** manually. Discovery and the MIDI invitation are separate steps. |
| The peer connects but you hear nothing | Check **Uno: Connected to Pico**, your app's MIDI output and channel, and Logic's audio input. Play the browser keyboard to check the audio path separately. |
| The panel still says **No controller** | Check **Connected Peers** on Mac or **Participants** on Windows. The Pico takes one RTP-MIDI peer at a time. |
| A wireless note keeps sounding | The Pico releases notes it owns when it detects a disconnection. Select **Panic** for an immediate release. |
| Logic sees MIDI but its audio meter does not move | Check the combined USB firmware, **QuarkWave USB Audio** as Logic's input device, and a mono track on **Input 1** with monitoring on. |

To send sound commands or MIDI clock from another controller, continue with the [external MIDI guide](external-midi-guide.md). The [technical guide](technical-guide.md#program-change-and-sysex-by-input) lists the exact messages.
