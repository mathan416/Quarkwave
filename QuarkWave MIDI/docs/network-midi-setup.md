# Connect a computer to QuarkWave

You have two useful ways to bring a computer into QuarkWave. **Network MIDI** lets a Mac or Windows app play the Uno through the Pico. With the optional **USB audio/MIDI firmware**, one Uno USB cable can carry MIDI into the synth and mono audio back to the computer. Choose the route that fits the session you want to make.

## What connects to what

| Route | MIDI goes… | Audio comes back… |
| --- | --- | --- |
| **RTP-MIDI** | Computer → Pico over Wi-Fi → Uno over the wired link | Through the Uno's USB audio input if that firmware is installed, or through a future physical line output |
| **Direct Uno USB** | Computer → **QuarkWave USB MIDI** → Uno | **QuarkWave USB Audio** on the same cable, in the composite firmware |
| **Browser keyboard** | Browser → Pico → Uno | The same Uno audio choices |

The Pico advertises one network session named **QuarkWave** on UDP port **5004** and accepts one RTP-MIDI peer at a time. Your computer and the Pico must be on a network where they can reach each other. The browser at `http://quarkwave.local/` shows **RTP-MIDI: Controller connected** when a peer joins. The **Pico** and **Uno** connection labels report separate links.

The Uno's USB audio input is **mono, 16-bit, 22,050 Hz**. It is part of the [USB audio/MIDI firmware](../experiments/usb-audio/README.md); the ordinary Uno sketch and the still-unbuilt A0 line circuit are different output arrangements.

## On a Mac

1. Power both boards and open the QuarkWave browser panel. Check that the Uno is connected.
2. Open **Audio MIDI Setup** in Applications → Utilities. Choose **Window → Show MIDI Studio**, then open **Configure Network Driver** (called **Network** on some versions of macOS).
3. Add a session under **My Sessions**, choose **RTP** as its type, name it something like `Mac to QuarkWave`, and enable it. Choose RTP rather than Network MIDI 2.0.
4. Under **Directory**, select **QuarkWave** and choose **Connect**. If discovery does not find it, add a manual entry with the Pico's IP address and port **5004**.
5. Confirm that QuarkWave appears under **Connected Peers**. In your music app, choose the *Mac session* as MIDI output and play a note. The Pico page should show **RTP-MIDI: Controller connected**.

The Mac session's local port is its own; **5004 is the Pico's destination port**. If you route a hardware keyboard into the session, avoid also routing the same keyboard through a DAW track or you may hear doubled notes. [Apple's Network MIDI guide](https://support.apple.com/guide/audio-midi-setup/ams1012/mac).

## On Windows

Windows needs an RTP-MIDI application that creates a MIDI port for your music software. One option is [rtpMIDI by Tobias Erichsen](https://www.tobias-erichsen.de/software/rtpmidi.html).

1. Put the computer and Pico on the same reachable network, power both boards, and check the browser panel.
2. Install and open rtpMIDI. If its installer asks for Bonjour, follow that prompt so it can discover network peers.
3. Under **My Sessions**, add a session, give it a name such as `Windows to QuarkWave`, and enable it.
4. Under **Directory**, select **QuarkWave** and choose **Connect**. If it does not appear, make a manual entry using the Pico's IP address and port **5004**. Confirm it appears under **Participants**.
5. In your DAW, choose the new rtpMIDI session as the **MIDI output**. Send and release a note; watch the Pico's RTP-MIDI label.

If the invitation fails, check the computer's firewall permissions for rtpMIDI on your local network. The Pico uses UDP **5004/5005** for the RTP session; this is a local connection, with no need to expose it to the internet. On Windows 11, a missing MIDI port can also be related to [Windows MIDI Services port visibility](https://devblogs.microsoft.com/windows-music-dev/windows-midi-services-rollout-known-issues-and-workarounds/). The [rtpMIDI tutorial](https://www.tobias-erichsen.de/software/rtpmidi/rtpmidi-tutorial.html) has screenshots of its session window.

## Use Logic Pro with QuarkWave

Logic can record QuarkWave as audio, sequence it through network MIDI, or use the Uno's direct USB MIDI and audio pair. Begin with low monitoring volume.

### Record the sound while playing from the Pico page

This is the simplest way to hear and capture a browser performance when the Uno USB audio firmware is installed. It does not need an RTP-MIDI session.

1. Connect the Uno's USB cable to the Mac. In **Audio MIDI Setup → Window → Show Audio Devices**, look for **QuarkWave USB Audio**.
2. In **Logic Pro → Settings → Audio → Devices**, choose it as the **Input Device** and your normal speakers or headphones as the **Output Device**.
3. Create a **mono audio track** on **Input 1**, enable **Input Monitoring (I)**, and play the Pico keyboard. Raise the listening level gently until you hear the note and see the track meter move.
4. Arm the track, record a short phrase, stop, and play the audio region back.

Logic may resample the Uno's 22.05 kHz input to match another project rate. [Logic's audio-device guidance](https://support.apple.com/en-gb/102171) and [input-monitoring guide](https://support.apple.com/guide/logicpro/turn-on-input-monitoring-for-audio-tracks-lgcpbfbefa96/mac) cover those controls.

### Sequence the synth from Logic over RTP-MIDI

1. Make the [Mac RTP connection](#on-a-mac). Keep the Uno USB cable connected too if you want to monitor or record its USB audio.
2. In Logic, choose **QuarkWave USB Audio** as the audio input and your usual listening device as output. Create an **External MIDI** track with the **External Instrument** plug-in.
3. Choose your **Mac RTP session** as **MIDI Destination**, channel **1**, and **Input 1 (mono)** as **Audio Input**. Leave **Send Program Change** off for the first test; QuarkWave reserves some Program Changes for its own sound commands.
4. Play or record a MIDI region. Logic sends the notes through the Pico, while the Uno generates the sound. Edit the MIDI region and play it again to hear the change.
5. To keep an audio take, record the Uno's Input 1 on a mono audio track or use a **real-time** bounce with the external return routed to the output. An offline bounce cannot render audio that the external Uno has not played in real time.

Monitor either the External Instrument return *or* a separate audio track while recording; monitoring both can sound doubled. [Logic's External Instrument guide](https://support.apple.com/guide/logicpro/lgcp12e7acdc/mac).

### Sequence and record through the Uno USB cable

This path uses the **combined USB audio/MIDI firmware** and does not need the Pico or a network session for note input.

1. In Audio MIDI Setup, find **QuarkWave USB MIDI** in MIDI Studio and **QuarkWave USB Audio** in Audio Devices.
2. In Logic, set the audio input to QuarkWave USB Audio. Create an External MIDI track or External Instrument plug-in. Set **MIDI Destination** to **QuarkWave USB MIDI**, channel **1**, and **Audio Input** to **Input 1 (mono)**.
3. Leave automatic Program Change off at first. Send a note and check the Uno matrix and Logic's audio meter. Record a MIDI phrase, replay it, and capture the audio in real time.

The same Uno USB cable can also carry programming/serial traffic while the composite interfaces are present. The Pico, if connected, can still show and manage patches. [USB audio/MIDI design](../experiments/usb-audio/README.md).

## If the connection or notes fail

| What happens | Where to look |
| --- | --- |
| QuarkWave does not appear in the network directory | Open the Pico web page to verify Wi-Fi, then use the Pico IP and port 5004 manually. Discovery and the MIDI invitation are separate. |
| The RTP peer connects but there is no sound | Check the Uno connection, the DAW's MIDI output and channel, and the listening path. Try a note from the browser keyboard to isolate the route. |
| The browser still says **No controller** | Check Connected Peers on Mac or Participants on Windows. The Pico accepts one peer at a time. |
| A note stays on after a wireless dropout | The Pico releases it when it detects the disconnect. Select **Panic** if you need an immediate release. |
| Logic receives MIDI but its audio meter is still | Check that the USB audio firmware is installed, QuarkWave USB Audio is the input device, and the track uses mono Input 1 with monitoring enabled. |

For another controller's sound commands and clock, continue with the [external MIDI guide](external-midi-guide.md). The [technical guide](technical-guide.md#program-change-and-sysex-by-input) lists the exact bytes.
