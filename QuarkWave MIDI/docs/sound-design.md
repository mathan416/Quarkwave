# Learn the sound by listening

The quickest way to understand a synth control is to change it while playing the *same* short phrase. These six small experiments start from factory sounds, so you can reload the original whenever you want. Use [Save As](user-guide.md#save-load-and-recover-patches) before reloading if you have made a variation worth keeping.

Start with a comfortable listening level. The current breadboard needs the [Uno USB audio route](network-midi-setup.md#record-the-sound-while-playing-from-the-pico-page) to be heard; the physical line output is still a [proposed circuit](connection-guide.md#proposed-mono-line-output-from-uno-a0).

## 1. Hear the oscillator morph

1. Load **Solid Bass**. Hold a middle-range note and sweep **Morph** slowly from one end to the other. Listen for the tone changing while the pitch stays put.
2. Reload Solid Bass. Change **Unison** from one oscillator to three, then raise **Detune** a little at a time. Listen for the extra thickness and motion.
3. Play a chord. Unison adds oscillators *inside each voice*; QuarkWave still has four note voices.

Try to describe the change in your own words: brighter, buzzier, wider, rougher? That description is more useful when you return to the sound later.

## 2. Shape a note's beginning and end

1. Load **Pluck** and repeat a short note. Raise **Attack** toward 200 ms. A sharp beginning should become slower.
2. Lower Attack again and raise **Release** toward 800 ms. Lift the key and listen to the tail.
3. Raise **Sustain** and hold a note. **Decay** governs the move from the initial level to that held level.

The envelope drawing in Shape is a picture of the settings, not an audio meter. Play both a short stab and a held note to hear all four ADSR stages.

## 3. Connect filter tone with playing strength

1. Load **EP Keys**. In Shape, compare **Cutoff** near 1 kHz and near 2.2 kHz while repeating the same note.
2. In Explore, set **Velocity to cutoff** to zero. Set keyboard **Velocity** to 40, then 120, and play a *new* note each time. Raise Velocity to cutoff and repeat. Listen for how much more the filter opens on a harder note.
3. Increase **Resonance** gently while moving Cutoff. It emphasizes the region around the filter's cutoff frequency.

Velocity beside the keyboard affects new browser notes only. It is not saved in a patch; the patch's **Velocity curve** and **Velocity to cutoff** settings are.

## 4. Add slow movement

1. Load **Warm Pad** and hold a chord. In Explore, set LFO 01 **Cutoff amount** to zero, then raise it gradually.
2. Change the LFO **Rate** from slow to faster. Return Cutoff amount to zero and try **To morph**, then **To amplitude**, one at a time.
3. Turn on **Sync LFO to tempo**. With external clock off, change **Internal BPM** and listen for the motion following it.

Separating destinations makes it easier to hear whether an LFO is changing brightness, waveform, or loudness.

## 5. Follow the effects chain

1. Load **Noise Perc** and repeat short notes. Raise **Bitcrush Mix**, then lower **Bits** and change its **Rate divider**. Return Mix to zero before the next comparison.
2. Load **EP Keys**. Compare **Chorus Mix** at zero and near 35%. Add a little **Tremolo Depth** and adjust its Rate.
3. Turn on Delay **Sync to 1/16 note**, raise **Delay Mix** gradually, and change Internal BPM. Listen to the spacing of the repeats.

QuarkWave processes chorus, drive/fold, bitcrush, tremolo, and delay in that order. Keep an ear on level as you combine them.

## 6. Build a playable variation

Choose a factory sound and decide on one goal: a quicker start, darker tone, more motion, or a stronger effect. Change one section at a time, then try the result with short notes, long notes, and a chord. If the part uses more than four overlapping notes, compare the **Voice steal** choices. Play softly and strongly to judge the **Velocity curve**.

Name the variation and use **Save As**. Reload it once to check that it still feels like the sound you intended. For more starting points, try the five recipes in the [patch book](patch-book.md#new-sounds-to-try). The [technical guide](technical-guide.md#how-the-sound-engine-works) follows the sound through the DSP stages.
