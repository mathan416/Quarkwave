# Connect the pieces of QuarkWave

A note can reach QuarkWave through the Pico browser or an external MIDI controller. The **Uno R4 WiFi** makes the sound; the **Pico W** provides the browser panel, saved patches, and wireless MIDI. This guide shows how the two boards are connected now, then how to add a wired MIDI input and physical audio outputs.

The photographed instrument has **two USB power cables, a Pico–Uno MIDI connection, and a level shifter**. Its DIN jack, A0 line-output circuit, and headphone amplifier are still plans. To hear the present build through a computer, use the Uno's [USB audio connection](network-midi-setup.md#record-the-sound-while-playing-from-the-pico-page).

## The current setup

![Pico W, level shifter, and Uno R4 WiFi in the present breadboard build](images/board-overview.jpg)

*The Pico and Uno are each powered by their own USB cable. The small board on the breadboard shifts the voltage between them.*

![Close view of the level shifter and Uno UART jumpers](images/wiring-closeup.jpg)

*The jumper colors are useful for following the photograph. Use the pin labels in the drawing when checking a connection.*

![Current Pico–Uno MIDI UART connection schematic](images/pico-uno-current-uart.svg)

Follow a browser note through the drawing: the Pico sends MIDI from **GPIO 0 (TX)** to **Uno D0 (RX0)**. The Uno replies from **D1 (TX1)** to **Pico GPIO 1 (RX)** through the **5 V-to-3.3 V level shifter**. Both directions run at **31,250 baud**. The boards share ground, but their USB 5 V rails are not joined.

The return path matters. Once the Uno has started, it can answer the Pico's readiness request and acknowledge a patch. That is how the browser can show **Uno: Connected to Pico** rather than merely guessing from the presence of a wire. The owner has confirmed the level-shifted return path and common ground.

If you move this circuit off the breadboard, check the level shifter's actual **LV/HV supply terminals** and the channel pairs for both directions against the module's labels. The photographs do not establish those exact pad numbers or a measured voltage at Pico RX. [Pico UART pins](../QuarkWave_UI_MIDI/QuarkWave_UI_MIDI.ino), [Uno UART setup](../QuarkWave_MIDI/QuarkWave_MIDI.ino).

## Add a wired MIDI input

A conventional five-pin MIDI IN would let a wired keyboard play the Uno without the Pico. The Uno firmware already listens on **D2**, but the jack and receiver are **not installed** on this breadboard.

![Proposed opto-isolated DIN MIDI IN to Uno D2](images/proposed-din-midi-in.svg)

MIDI IN is a current loop. Put an **opto-isolated MIDI receiver** between the DIN jack and D2; do not connect the jack directly to the Uno pin. Choose a receiver or module made for the [MIDI 1.0 electrical specification](https://www.midi.org/wp-content/uploads/wpforo/default_attachments/1709416667-ca33-MIDI-10-Electrical-Specification-Update.pdf).

1. Use a female **180° five-pin DIN jack** marked MIDI IN. Connect pins **4** and **5** to the receiver's input as its circuit specifies. Check the jack's own pin drawing; its front and rear views are mirror images.
2. Power the receiver's **logic side** from Uno **5 V and GND**. Connect its **idle-high, 5 V logic output** to **D2**. Keep the incoming MIDI loop electrically isolated from Uno ground.
3. Leave DIN pins **1** and **3** unconnected. Do not make a DC connection from DIN pin **2** or the jack shield to Uno ground.

Build with the USB cables unplugged. Before connecting a controller, check supply polarity, the DIN pin numbering, the isolation boundary, and an idle-high signal at D2. Uno **D3** is configured as software-serial TX in the sketch, but this guide does not add a MIDI OUT or THRU jack; D3 must not be wired directly to a DIN jack. The [external MIDI guide](external-midi-guide.md) covers playing the Uno on its own.

## Build a mono line output from Uno A0

The Uno creates its analog signal at **A0**, its 12-bit DAC pin. A0 is not a line-output jack. The proposed circuit below uses an **MCP6002** to buffer the DAC, a capacitor to remove DC, and a small output divider before a mono jack. It is designed for a **powered speaker's line input or a mixer line input**. This circuit has **not been built or measured**.

![Uno A0 to buffered, AC-coupled mono line-output schematic](images/uno-r4-line-output.svg)

Read the audio row from left to right: **A0 → R1 → buffer → R2 → C2 → jack tip**. R3 gives the jack side of C2 a ground reference. The power and unused-amplifier rows in the drawing are separate connections to make, not audio flowing through those rows.

The **MCP6002** is an additional chip. A through-hole **MCP6002-I/P** is convenient on a breadboard. Its first amplifier is wired as a unity-gain buffer; it makes the DAC easier to connect to a cable and line input without intentionally adding gain. Tie off the unused second amplifier as shown. Check the [Microchip MCP6002 pinout](https://www.microchip.com/en-us/product/MCP6002) if you use another package.

| Part | Where it goes |
| --- | --- |
| **U1 power** | MCP6002 pin **8** to **Uno 5 V**; pin **4** to **Uno GND**. Keep Pico 5 V separate. |
| **C1** | **100 nF ceramic** between U1 pins 8 and 4, close to the chip. |
| **R1 and buffer** | **1 kΩ** from Uno A0 to U1 pin **3 (+A)**. Join pin **2 (−A)** to pin **1 (OUT A)**. |
| **Unused half** | Join U1 pin **6 (−B)** to pin **7 (OUT B)**; pin **5 (+B)** to Uno GND. |
| **R2 and C2** | **10 kΩ** from U1 pin 1 to a **10 µF** coupling capacitor. A bipolar capacitor is simplest. For a polarized part, its **positive** side faces U1/R2. |
| **R3 and J1** | **10 kΩ** from C2's jack side to Uno GND. That same point goes to a mono **TS jack tip**; the **sleeve** goes to Uno GND. |

C2 removes the DAC's DC offset from the signal going to the mixer. R2 and R3 reduce the AC level: the estimate is about half the buffered voltage into a high-impedance line input, or about a third into a 10 kΩ mixer input. These are **circuit estimates**, not measured QuarkWave output levels. The [Uno R4 datasheet](https://docs.arduino.cc/resources/datasheets/ABX00087-datasheet.pdf) and [RA4M1 DAC data](https://www.renesas.com/en/document/dst/ra4m1-group-datasheet) describe the DAC limits.

### Build and first-power checks

1. Unplug both USB cables. Build the circuit using **Uno 5 V and GND**. Keep the Pico–Uno ground and level-shifter wiring in place.
2. Before attaching a mixer or speakers, check the jack sleeve to Uno GND, U1 pin 8 to Uno 5 V, and U1 pin 4 to Uno GND. Check that 5 V is not shorted to ground and that a polarized C2 faces the right way.
3. Power the Uno by USB. Measure about **5 V** across U1 pins 8 and 4. After C2 settles, measure the jack tip against sleeve: its **DC voltage should be near zero**. If substantial DC remains, fix the circuit before connecting audio equipment.
4. Connect a short shielded cable to a **line input**. Start with low input gain and speaker level, play a note, then raise the level gradually. Record the measured jack level, hum, or clipping in the [hardware test log](hardware-test-log.md).

This jack is **mono**. Do not connect headphones or a passive speaker directly to it. A stereo aux input needs an intentional mono-to-stereo connection; do not tie left and right outputs together. The next section shows a separate headphone route.

## Add headphones with the TDA1308 board

The available [TDA1308 headphone amplifier module](https://www.amazon.ca/dp/B0GVTKNFDS) has pads marked `VCC +/−`, `IN L/GND/R`, and `OUT L/G/R`. Its photographed chip and pad labels identify the kind of board, but not its internal gain or every component value. Check those details on the actual module before using headphones; the [TDA1308 datasheet](https://www.nxp.com/docs/en/data-sheet/TDA1308.pdf) describes the amplifier chip.

![Owner's TDA1308 headphone amplifier module](images/headphone-amplifier-photo.jpg)

Take audio from the **line node after C2/R3**, not directly from A0. A volume knob controls both ears. Split its mono output through two separate resistors into the module's left and right *inputs*. Keep the module's left and right *outputs* separate all the way to the headphone jack.

![Mono line node through volume control and TDA1308 to both ears](images/tda1308-headphone-wiring.svg)

![Component-level headphone connection schematic](images/tda1308-headphone-schematic.svg)

| From | To |
| --- | --- |
| Node after C2/R3 | One outer terminal of a **10 kΩ audio-taper potentiometer**; the other outer terminal to Uno GND. |
| Potentiometer wiper | Two separate **1 kΩ resistors**: one to module `IN L`, one to `IN R`. |
| Uno GND | Module `IN GND` and `VCC −`. Check that module `OUT G` has continuity to this ground. |
| Uno 5 V | Module `VCC +`; do not join it to Pico 5 V. |
| Module `OUT L`, `OUT R`, `OUT G` | Stereo **TRS jack tip**, **ring**, and **sleeve**, respectively. |

The physical knob is useful because the Uno's master-gain setting does not reach zero. Before inserting headphones, turn the knob fully down and power the module. Measure DC from **OUT L to OUT G** and **OUT R to OUT G**; both should settle near zero. If either has substantial DC, stop and check the wiring. Try inexpensive headphones at the lowest level first. For powered speakers or a mixer, use the mono line jack instead.

## After you build these circuits

The Pico–Uno link in the photographs is the present hardware. The DIN receiver, buffered A0 line output, and headphone amplifier are the next hardware pieces. As each is built, record the actual jumper endpoints and shifter voltage, then the DIN isolation and MIDI reception, line-jack DC and signal level, and headphone-board output DC and gain in the [hardware test log](hardware-test-log.md). Those measurements turn the proposed drawings into an as-built connection record.
