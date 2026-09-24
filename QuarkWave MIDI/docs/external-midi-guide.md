# Play QuarkWave from another controller

The Uno is a synth in its own right. The Pico gives it a browser and wireless connections, but you can play the sound engine from a separate MIDI source. This guide helps you choose a path and explains what happens when more than one controller is present.

## Choose an input

| Route | What it gives you |
| --- | --- |
| **DIN MIDI → Uno** | A conventional wired controller straight into the sound engine. The Uno firmware listens on D2, but the photographed breadboard does **not** yet have a DIN jack or its required isolated receiver. Build the [DIN input circuit](connection-guide.md#add-a-wired-midi-input) before using this route. |
| **USB MIDI → Uno** | In the optional composite firmware, a computer sees **QuarkWave USB MIDI** and can receive **QuarkWave USB Audio** on the same cable. The Pico is optional for this path. |
| **RTP-MIDI → Pico → Uno** | A Mac, Windows computer, or network MIDI controller joins the Pico's wireless session. The Uno's own Wi-Fi MIDI is off in this build. |
| **BLE-MIDI** | Disabled in this build. |

The Uno accepts ordinary notes on all MIDI channels, pitch bend with a fixed ±2-semitone span, sustain pedal **CC64**, and the sound controls in the [MIDI table](technical-guide.md#control-changes).

## Play without the Pico

Once the DIN input and an audio output are built, power the Uno and connect a controller's MIDI OUT to QuarkWave MIDI IN. Begin with the listening level low. Play and release a note; the Uno starts from its own built-in default sound when no Pico is present. That is different from the Pico's **Warm Pad** factory preset.

Try pitch bend, sustain, and a few CC controls. These changes live in the Uno until it is reset or another sound is loaded; the Uno has no user patch files. If a note hangs, send **All Notes Off (CC123)** or **All Sound Off (CC120)** from your controller. For the current breadboard, direct USB MIDI is the practical no-Pico route when the composite firmware is installed.

## Play over Wi-Fi with the Pico attached

1. Power both boards and open the panel to check **Uno: Connected to Pico**.
2. Connect one RTP-MIDI peer to the Pico's **QuarkWave** session on UDP port **5004**. The [Mac and Windows setup guide](network-midi-setup.md) gives the menu steps.
3. Choose the network session as your music app's MIDI output. Send a note and release it. The Pico forwards it to the Uno over the wired link.

If the peer disconnects while holding notes or sustain, the Pico releases only that peer's notes and pedal. A silent network failure may take until the RTP session times out; **Panic** on the browser panel provides an immediate manual release.

## Play and record over one USB cable

With the [composite USB audio/MIDI firmware](../experiments/usb-audio/README.md), set your DAW's MIDI destination to **QuarkWave USB MIDI** and its mono recording input to **QuarkWave USB Audio**. The Uno plays the notes directly and sends generated audio back to the computer. Its connection to the Pico can remain active. The [Logic Pro steps](network-midi-setup.md#sequence-and-record-through-the-uno-usb-cable) show a complete track setup.

Each input owns its own held notes and sustain state. A USB disconnect releases USB-owned notes without cutting off a matching pitch held through DIN or the Pico. MIDI Clock, by itself, does not count as someone changing the sound.

## Use an external controller while the Pico is present

If a controller plays or changes the Uno before the Pico sends its startup patch, QuarkWave preserves the Uno's sound. The browser says **Uno: Connected to Pico · standalone sound**. Continue with that sound, load a patch from the panel, or choose **More actions → Sync Pico patch to Uno** when you want to replace it. Sync resets the current sound and held notes.

After a Pico patch is loaded, other controllers can still play and edit the Uno. A direct MIDI edit may not move the Pico's sliders, so saving the Pico patch does not necessarily capture it. The [musician guide](user-guide.md#take-over-a-standalone-sound) explains the explicit takeover.

## Follow MIDI Clock

1. In **Explore**, enable **Use external MIDI clock**, or send QuarkWave Program Change **100**. Clock pulses alone do not select external tempo. Program Change **101** returns to internal tempo.
2. Send MIDI Clock from the controller. For arpeggiator transport, send **Start**, **Continue**, and **Stop** as well. Start resets the pattern, Continue resumes, and Stop releases its current note while preserving the keys you hold.
3. Use a tempo from **40–240 BPM**. If several inputs send valid clock, the Uno prefers **DIN → direct USB → RTP**. When a clock disappears for two seconds, it chooses another recent input or keeps the last valid tempo. Until the first valid clock, it uses 120 BPM.

Some controller menus display Program Change numbers one higher than the MIDI data byte. If a command seems wrong, use a MIDI monitor to inspect the transmitted number. [Clock design](technical-guide.md#external-midi-clock-and-transport).

## QuarkWave-specific sound commands

Ordinary CCs cover most sound controls. A controller that can send custom messages may also use QuarkWave Program Changes **100–103** and **110–112**, or SysEx commands **01–03**, to select tempo source, delay sync, vibrato wave, internal BPM, LFO routing, and chorus depth. The [exact bytes and input rules](technical-guide.md#program-change-and-sysex-by-input) are in the technical guide.

Generic Program Changes do **not** choose the Pico's eight factory patches. Display, reset, and handshake commands are reserved for the Pico link, and QuarkWave does not forward custom commands to other synths.
