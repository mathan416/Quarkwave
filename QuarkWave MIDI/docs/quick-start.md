# Start playing QuarkWave

QuarkWave has two parts: the **Uno R4** makes the sound, and the **Pico W** provides the browser panel, patches, and wireless MIDI. This short tour gets you from a powered instrument to a sound of your own.

## 1. Connect your listening setup

Power the Pico and Uno through their USB cables. On the photographed development build, the physical line-output circuit is not yet installed. If the Uno has the USB audio firmware, you can hear it through a mono **QuarkWave USB Audio** input in Logic Pro; [the Logic steps](network-midi-setup.md#record-the-sound-while-playing-from-the-pico-page) walk through monitoring. Start with a low listening level. The proposed [A0 line-output circuit](connection-guide.md#build-a-mono-line-output-from-uno-a0) is a future physical connection, not a jack on the current breadboard.

## 2. Open the instrument

Connect your computer or phone to the same Wi-Fi network as the Pico, then open `http://quarkwave.local/` in a browser. If the name does not resolve, use the Pico's IP address. Wait for **Pico: Connected** and **Uno: Connected to Pico** at the top of the page.

The **RTP-MIDI** label is for a separate wireless MIDI controller. **No controller** there is normal when you are playing the browser keyboard.

## 3. Play and change a sound

1. In **Perform**, turn **Volume** down, then play a key on the screen. The Uno's LED matrix should react. Raise your listening level gently once your audio path is ready.
2. Try the computer keys **A W S E D F T G Y H U J**. The **−** and **+** buttons shift the keyboard octave. **Velocity** sets how strongly *new* browser notes are played; it starts at 100 each time you open the page.
3. Choose **Pluck** from the patch menu and select **Load patch**. Play a few short notes, then move **Cutoff** and **Morph** to hear how the tone changes.

The factory sounds are starting points. You can edit them freely while you play; loading another sound replaces unsaved edits.

## 4. Keep a sound you like

Give the sound a name, select **Save As**, choose one of the eight user slots, and select **Save to slot**. The panel asks before it replaces an occupied slot. Factory presets are always read-only.

If a note keeps sounding, select **Panic** in the top strip. For a more complete tour, continue with the [musician guide](user-guide.md), try a [patch recipe](patch-book.md), or follow the [sound-design lessons](sound-design.md).
