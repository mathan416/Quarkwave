# Connection guide: photographed build and proposed MIDI/audio connections

**Status (2026-09-23):** The board and jumper layout below is photographed, and the owner confirms the return-wire level shifter and separate USB power. The Uno `A0` audio output is **not yet connected**. The line-output circuit on this page is a **proposed, unbuilt design** for a powered speaker's line input or mixer, not a measured property of the current instrument. The firmware snapshot is [2026-09-23](README.md#firmware-snapshot).

## The current setup

![Pico W, bidirectional level-shifter module, and Uno R4 WiFi on adjacent breadboard and base](images/board-overview.jpg)

*Owner-supplied photograph, 2026-09-23. Image metadata removed for the guide. The boards are powered through their individual USB ports; audio is not wired.*

![Closer view of the level shifter, jumper wires, and Uno UART header](images/wiring-closeup.jpg)

*The lower-right Uno `RX0`/`TX1` header, Pico, and level-shifter module are visible. Wire colors alone do not establish an electrical pin map or voltage.*

![Current Pico-to-Uno MIDI UART topology: separate USB power, two signal paths through the level-shifter module, and common ground](images/pico-uno-current-uart.svg)

*Current connection schematic based on the installed firmware, the photographs, and the owner's confirmation. Its blue and amber lines indicate MIDI direction, **not jumper colors**. It records the present two-board topology, not the proposed audio circuit below. The exact level-shifter channel numbers and LV/HV supply-pin endpoints have not been continuity-checked.*

In the photographs, both UART signal jumpers appear to enter separate channels of the level-shifter module. The owner has specifically confirmed **Uno TX → 5 V-to-3.3 V shifting → Pico RX** and a common ground. The Pico-to-Uno signal path is defined by the two sketches, but the module's individual channel pins are inferred from the photograph. The dashed shifter-supply lines show the expected Pico 3.3 V/LV and Uno 5 V/HV sides; check the actual terminal labels and continuity before treating them as a measured netlist. The boards remain independently USB-powered; their 5 V rails are not tied together.

| Path | Firmware or owner-confirmed connection | Status |
| --- | --- | --- |
| Power | Pico W and Uno R4 WiFi each use their own USB port. | **Owner-confirmed**. Exact USB supply arrangement and voltages have not been measured. Do not join the boards' 5 V rails for this setup. |
| Common reference | Pico and Uno grounds are connected through the level-shifter wiring. | **Owner-confirmed**; the photos show ground wires but cannot prove continuity. |
| Pico → Uno | Pico `Serial1` TX, GPIO 0 → a level-shifter channel on the Pico 3.3 V side → Uno `Serial1` RX (`RX0`/D0), 31,250 baud MIDI UART. | **Source-defined path; photographed shifter routing inferred.** Exact channel and endpoints have not been continuity-checked. |
| Uno → Pico | Uno `Serial1` TX (`TX1`/D1) → a separate **5 V-to-3.3 V level-shifter channel** → Pico `Serial1` RX, GPIO 1. | **Owner-confirmed** path and shifter; exact module pins and Pico RX voltage still need measurement. |
| Browser ↔ Pico | Pico serves the panel by Wi-Fi; browser control uses WebSocket on port `8081`. | Source-defined; [deployment smoke check](hardware-test-log.md#deployment-smoke-check--2026-09-23) showed both connection labels. |
| Separate DIN MIDI | Uno software serial RX is D2; TX is configured on D3. | Source-defined only. A DIN connector and its input circuit are not identified in these photographs. |
| Audio | Uno writes 12-bit samples to DAC pin `A0` at a nominal 22,050 samples/s. | **Not built**: no A0 output circuit, jack, or powered-speaker/mixer connection yet. |

[Pico UART setup](../QuarkWave_UI_MIDI/QuarkWave_UI_MIDI.ino), [Uno MIDI instances and DAC output](../QuarkWave_MIDI/QuarkWave_MIDI.ino), [technical guide](technical-guide.md#connections-and-build-flags).

## Proposed physical DIN MIDI IN for standalone Uno use

The Uno firmware already listens at **31,250 baud** on `D2` through `SoftwareSerial(2, 3)`. The photographs do not show a DIN connector. A controller's MIDI OUT must reach `D2` through an **opto-isolated MIDI IN receiver**, not a direct jumper from a DIN jack. The plan below follows the [MIDI Association's 5-pin DIN electrical specification](https://www.midi.org/wp-content/uploads/wpforo/default_attachments/1709416667-ca33-MIDI-10-Electrical-Specification-Update.pdf); the receiver module or component-level circuit has not yet been chosen or built.

![Proposed isolated 5-pin DIN MIDI IN feeding Uno D2](images/proposed-din-midi-in.svg)

| Connection | Proposed wiring |
| --- | --- |
| DIN jack | Female 5-pin, 180°, labeled **MIDI IN**. Pins **4** and **5** feed the isolated receiver's current-loop input with the polarity specified by its circuit. Confirm pin numbering from the actual jack datasheet; rear views are easy to reverse. |
| Isolation | Use a MIDI IN circuit or module designed to meet the MIDI 1.0 electrical specification, including input current limiting and reverse-voltage protection for the optocoupler. The input loop stays electrically isolated from the Uno. |
| Receiver output | An **idle-high, 5 V logic UART output** goes to Uno **D2**. Power and ground for the receiver's *logic/output side* come from Uno 5 V and GND. |
| Unused contacts | DIN pins **1** and **3** remain unconnected. DIN pin **2** and the jack shield have **no DC path** to Uno ground; the MIDI specification permits optional small RF capacitors, which are not needed for this initial build. |
| Uno D3 | Configured as software-serial TX but no MIDI OUT or THRU connector is implemented. Do not wire D3 directly to a DIN jack. |

Build this input with both USB cables unplugged. Check that DIN pin 2 and the jack shield are isolated from Uno ground, verify receiver supply polarity and an idle-high output at D2, then power the Uno and test Note On/Off from a known MIDI controller. The Uno remains usable without the Pico once this input and the separate audio output are built. Record the chosen receiver part, exact pin map, and measurements before replacing this proposed diagram with an as-built schematic. [Uno DIN receiver](../QuarkWave_MIDI/QuarkWave_MIDI.ino), [external MIDI guide](external-midi-guide.md).

## Proposed mono line output from Uno A0

This circuit makes a **mono, AC-coupled line output** for a powered speaker **line input** or mixer line input. It uses a high-input-impedance buffer so the Uno's DAC is not loaded directly by the cable or destination. The 10 kΩ/10 kΩ output network reduces the possible signal level before it reaches the destination. It is **not** a speaker driver or headphone output. The diagram separates its audio, power, and unused-amplifier connections so each path can be read without crossing another.

![Proposed Uno A0 to buffered, AC-coupled mono line-output schematic](images/uno-r4-line-output.svg)

**What is U1?** The **MCP6002 is a separate, small analog chip** to add to the breadboard; it is not already inside the Uno, Pico, or TDA1308 module. It has two amplifiers in one package. This circuit uses one as a unity-gain **buffer**: the sound voltage stays approximately the same, while the following line and headphone circuits draw their signal from U1 instead of loading the Uno DAC. For a breadboard, look for **MCP6002-I/P**, the 8-pin through-hole PDIP version shown in [Microchip's datasheet](https://ww1.microchip.com/downloads/aemDocuments/documents/MSLD/ProductDocuments/DataSheets/MCP6001-1R-1U-2-4-1-MHz-Low-Power-Op-Amp-DS20001733L.pdf). An SOIC or MSOP version needs an adapter and a different physical mounting method. The chip itself needs no programming.

| Part | Value and connection |
| --- | --- |
| U1 | MCP6002 dual, rail-to-rail op amp, **DIP-8 pinout shown**. Power pin 8 from **Uno 5 V** and pin 4 from **Uno GND**. The op amp uses the Uno's USB-derived 5 V only; leave the Pico 5 V rail separate. |
| C1 | 100 nF ceramic directly across U1 pins 8 and 4, close to the IC. |
| R1 | 1 kΩ from Uno `A0` to U1 pin 3 (`+A`). U1 pin 2 (`−A`) joins pin 1 (`OUT A`) for unity-gain buffering. |
| Unused U1 channel | Join pins 6 (`−B`) and 7 (`OUT B`); connect pin 5 (`+B`) to Uno GND. Do not leave its inputs floating. |
| R2 | 10 kΩ from U1 pin 1 to coupling capacitor C2. |
| C2 | 10 µF coupling capacitor. A **bipolar/nonpolar** part is simplest. For a polarized electrolytic, its **positive** lead faces U1/R2 and its negative lead faces the jack. |
| R3 | 10 kΩ from the output side of C2 to Uno GND; provides a DC reference and sets the nominal output divider with R2. |
| J1 | Mono 6.35 mm or 3.5 mm **TS** line jack: tip to the output side of C2, sleeve to Uno GND. Use a short shielded cable to the destination's **line input**. |

The Uno sketch centers its signed audio at DAC midscale before `analogWrite(A0, ...)`; C2 removes that DC component before the line jack. The [Arduino board documentation](https://docs.arduino.cc/resources/datasheets/ABX00087-datasheet.pdf) identifies `A0` as its 12-bit DAC. The [Renesas RA4M1 datasheet](https://www.renesas.com/en/document/dst/ra4m1-group-datasheet) specifies a minimum 30 kΩ resistive DAC load and maximum 50 pF capacitive load in its stated test conditions, which is why the circuit buffers `A0` rather than attaching a jack, headphones, or a filter capacitor to it. The [MCP6002 product page and datasheet](https://www.microchip.com/en-us/product/MCP6002) cover the proposed 5 V, unity-gain buffer; check the package pinout before wiring another op amp or package.

### Build and first-power checks

1. Unplug both USB cables. Build U1, C1, R1, R2, C2, R3, and J1 as shown. Connect the new circuit's 5 V and ground **only to the Uno**. Keep the existing Pico–Uno ground link and level shifter as they are.
2. Before plugging into a mixer or powered speaker, check continuity: jack sleeve → Uno GND, U1 pin 8 → Uno 5 V, U1 pin 4 → Uno GND. Confirm no 5 V-to-GND short and verify C2 polarity if it is electrolytic.
3. Power the Uno by USB. With a multimeter, check approximately 5 V between U1 pins 8 and 4. Check the jack tip against sleeve for **near-zero DC** after C2 settles. If a substantial DC voltage persists, disconnect the audio destination and correct the circuit.
4. Set the destination input gain and speaker level low. Connect the mono line output to a line input, play a note, then raise the listening level gradually. Record hum, clipping, and measured jack levels in the [hardware test log](hardware-test-log.md).

The divider gives approximately one-half of the buffered AC voltage into a high-impedance input; a 10 kΩ mixer input gives approximately one-third. These are circuit estimates, **not measured QuarkWave output levels**. A stereo aux input needs a separate mono-to-stereo distribution arrangement; do not tie its left and right outputs or contacts together. A passive loudspeaker or headphones need an amplifier.

## Proposed headphones with the owner's TDA1308 board

The owner identified the pictured module from its [purchase listing](https://www.amazon.ca/dp/B0GVTKNFDS) as a **TDA1308 headphone amplifier board**. Its photographed face has `VCC +/−`, `IN L/GND/R`, and `OUT L/G/R` pads. The chip marking and the board's component values cannot be read from the photo, so its internal circuit and actual gain remain unverified. The [NXP TDA1308 datasheet](https://www.nxp.com/docs/en/data-sheet/TDA1308.pdf) describes a stereo headphone driver that operates from a single 3–7 V supply and is characterized with 32 Ω headphones; that supports 5 V as a chip supply but does not by itself validate this particular assembled module.

![Owner's TDA1308 headphone amplifier module, photographed before installation](images/headphone-amplifier-photo.jpg)

*Owner-supplied photograph, 2026-09-23; metadata removed. The printed `IN`, `OUT`, and `VCC` labels identify the pad groups. The module is not connected to QuarkWave yet.*

Use the **buffered and AC-coupled output node** shown after C2/R3 in the line-output diagram as the headphone board's source. It is mono; feed that one source to *both inputs*, but keep the board's left and right **outputs separate**. Add a physical input volume control before the headphone module; the Uno's master gain cannot turn fully to zero.

![Proposed connection map from the mono line-output node through a volume control and TDA1308 board to a stereo headphone jack](images/tda1308-headphone-wiring.svg)

The map above shows the order of the modules. This second drawing shows the **individual external components and connections**. It starts at the same output node after C2/R3 in the line-output schematic; it does not connect directly to Uno A0. The TDA1308 board is shown as a labeled module because its internal PCB schematic has not been verified.

![Component-level schematic for a mono volume potentiometer, two input resistors, TDA1308 board pads, and a stereo headphone jack](images/tda1308-headphone-schematic.svg)

| From | To | Purpose |
| --- | --- | --- |
| Output node after C2/R3 | One outer terminal of a **10 kΩ audio-taper potentiometer** | Mono headphone level control. The other outer terminal goes to Uno GND. |
| Potentiometer wiper | Two separate **1 kΩ resistors**, then module `IN L` and `IN R` respectively | Feeds the mono sound to both headphone channels without joining their outputs. |
| Uno GND | Module `IN GND` and `VCC −` | Common input and supply reference. Confirm module `OUT G` is continuous with this ground before fitting the jack. |
| Uno 5 V | Module `VCC +` | Module power from the Uno's USB-derived rail; do not connect Pico 5 V here. |
| Module `OUT L` / `OUT R` / `OUT G` | 3.5 mm stereo **TRS** jack tip / ring / sleeve | Left ear / right ear / common return. Do not join `OUT L` and `OUT R`. |

**First check:** Leave headphones unplugged, set the physical volume control fully down, power the Uno, and measure DC from each module output (`OUT L`, `OUT R`) to `OUT G`. Both should settle near zero before headphones are connected. If either output holds substantial DC, stop and inspect the board and wiring. Then test with inexpensive headphones at minimum level and raise the control slowly. The board's headphone response, output coupling, and possible USB noise remain **hardware untested**. This route does not replace the mono line output for a mixer or powered speakers.

## Still to establish on the assembled build

| Item | Evidence needed |
| --- | --- |
| Pico RX logic level | Measure Uno return-data high level at Pico GPIO 1 and confirm it stays within 3.3 V logic limits. |
| UART and common-ground pin map | Continuity-check both UART channel pairs and the photographed jumper endpoints; record the shifter's actual HV/LV supply and ground terminals. |
| USB supply and noise | Record the two USB power sources and measure supply voltage and any audio hum once the line stage exists. |
| Proposed line stage | Build, photograph, measure DC and audio at the jack, and test with the intended powered speaker or mixer. |
| TDA1308 headphone module | Build the buffered feed and volume control, confirm the pad ground with continuity, measure output DC with no headphones, then test low-volume playback. |
| DIN MIDI input | Provide the connector and input-interface circuit if external DIN operation is part of this build. |

The [quick start](quick-start.md) continues to assume a listening setup supplied by the builder until the line-output circuit is assembled and verified.
