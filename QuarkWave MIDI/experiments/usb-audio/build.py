#!/usr/bin/env python3
"""Build the Uno native-USB audio and MIDI prototype without editing the core."""

from pathlib import Path
import shutil
import subprocess
import tempfile


HERE = Path(__file__).resolve().parent
UNO = HERE.parent.parent / "QuarkWave_MIDI"
FQBN = "arduino:renesas_uno:unor4wifi"
OUTPUT = Path("/private/tmp/quarkwave-usb-audio-midi.bin")
FLAGS = " ".join(
    (
        "-DF_CPU=48000000",
        "-DARDUINO_UNOR4_WIFI",
        "-DCFG_TUD_AUDIO=1",
        "-DCFG_TUD_AUDIO_FUNC_1_DESC_LEN=TUD_AUDIO_MIC_ONE_CH_DESC_LEN",
        "-DCFG_TUD_AUDIO_FUNC_1_N_AS_INT=1",
        "-DCFG_TUD_AUDIO_FUNC_1_CTRL_BUF_SZ=64",
        "-DCFG_TUD_AUDIO_ENABLE_EP_IN=1",
        "-DCFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX=46",
        "-DCFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ=256",
        "-DCFG_TUD_MIDI_RX_BUFSIZE=256",
        "-DCFG_TUD_MIDI_TX_BUFSIZE=64",
        "-DQUARKWAVE_USB_MIDI=1",
    )
)


def replace_once(text: str, old: str, new: str) -> str:
    if text.count(old) != 1:
        raise RuntimeError(f"Expected one core/sketch insertion point: {old[:70]!r}")
    return text.replace(old, new)


def patch_core(text: str) -> str:
    # CoreMIDI names this single-cable USB MIDI destination from the USB
    # product string, rather than the MIDI interface string on the tested Mac.
    # Keep the audio interface's separate "QuarkWave USB Audio" label.
    text = replace_once(
        text,
        "        [USBD_STR_PRODUCT] = USB_NAME,",
        '        [USBD_STR_PRODUCT] = "QuarkWave USB MIDI",',
    )
    # macOS CoreMIDI retains the old UNO R4 name for the board's original
    # VID/PID/serial combination. Give this optional firmware a distinct,
    # stable serial suffix while keeping 120 bits of the hardware unique ID.
    text = replace_once(
        text,
        '        utox8(t->unique_id_words[3], &idString[24]);',
        '        utox8(t->unique_id_words[3], &idString[24]);\n'
        "        idString[30] = 'Q';\n"
        "        idString[31] = 'W';",
    )
    text = replace_once(
        text,
        "#define USBD_STR_DFU_RT (0x05)",
        "#define USBD_STR_DFU_RT (0x05)\n"
        "#define USBD_STR_AUDIO (0x06)\n"
        "#define USBD_STR_MIDI (0x07)\n"
        "#define USBD_AUDIO_EP_IN (0x85)\n"
        "#define USBD_MIDI_EP_OUT (0x06)\n"
        "#define USBD_MIDI_EP_IN (0x86)",
    )
    text = replace_once(
        text,
        "        interface_count += (install_CDC ? 2 : 0);",
        "        interface_count += (install_CDC ? 2 : 0);\n"
        "#if CFG_TUD_AUDIO\n"
        "        uint8_t audio_desc[TUD_AUDIO_MIC_ONE_CH_DESC_LEN] = {\n"
        "            TUD_AUDIO_MIC_ONE_CH_DESCRIPTOR(interface_count, USBD_STR_AUDIO, 2, 16,\n"
        "                                            USBD_AUDIO_EP_IN, 46)\n"
        "        };\n"
        "        interface_count += 2;\n"
        "#endif\n"
        "#if CFG_TUD_MIDI\n"
        "        uint8_t midi_desc[TUD_MIDI_DESC_LEN] = {\n"
        "            TUD_MIDI_DESCRIPTOR(interface_count, USBD_STR_MIDI,\n"
        "                                USBD_MIDI_EP_OUT, USBD_MIDI_EP_IN, 64)\n"
        "        };\n"
        "        interface_count += 2;\n"
        "#endif",
    )
    text = replace_once(
        text,
        "            + (install_MSD ? sizeof(msd_desc) : 0);",
        "            + (install_MSD ? sizeof(msd_desc) : 0);\n"
        "#if CFG_TUD_AUDIO\n"
        "        usbd_desc_len += sizeof(audio_desc);\n"
        "#endif\n"
        "#if CFG_TUD_MIDI\n"
        "        usbd_desc_len += sizeof(midi_desc);\n"
        "#endif",
    )
    text = replace_once(
        text,
        "            if (install_MSD) {\n"
        "                memcpy(ptr, msd_desc, sizeof(msd_desc));\n"
        "                ptr += sizeof(msd_desc);\n"
        "            }",
        "            if (install_MSD) {\n"
        "                memcpy(ptr, msd_desc, sizeof(msd_desc));\n"
        "                ptr += sizeof(msd_desc);\n"
        "            }\n"
        "#if CFG_TUD_AUDIO\n"
        "            memcpy(ptr, audio_desc, sizeof(audio_desc));\n"
        "            ptr += sizeof(audio_desc);\n"
        "#endif\n"
        "#if CFG_TUD_MIDI\n"
        "            memcpy(ptr, midi_desc, sizeof(midi_desc));\n"
        "            ptr += sizeof(midi_desc);\n"
        "#endif",
    )
    return replace_once(
        text,
        '        [USBD_STR_DFU_RT] = "DFU-RT Port",',
        '        [USBD_STR_DFU_RT] = "DFU-RT Port",\n'
        '        [USBD_STR_AUDIO] = "QuarkWave USB Audio",\n'
        '        [USBD_STR_MIDI] = "QuarkWave USB MIDI",',
    )


def patch_bootloader_reset(text: str) -> str:
    # Native USB on core 1.6.0 does not re-enter the bootloader through the
    # normal NVIC software reset. Reuse the core's watchdog reset path.
    text = replace_once(
        text, '#include "boot.h"',
        '#include "boot.h"\nextern "C" void tud_dfu_runtime_reboot_to_dfu_cb(void);',
    )
    return replace_once(
        text,
        "   NVIC_SystemReset();\n   while (1); // WDT will fire here",
        "   tud_dfu_runtime_reboot_to_dfu_cb();\n   while (1);",
    )


def patch_rusb2_driver(text: str) -> str:
    # The 1.6.0 RUSB2 driver spins forever if an isochronous IN pipe's
    # double buffer remains full after a host stops polling. Upstream TinyUSB
    # bounds the FIFO-ready wait and no longer waits for INBUFM after an IN
    # transfer. Port those two changes into this experiment's core copy.
    text = replace_once(
        text,
        "static inline void pipe_wait_for_ready(rusb2_reg_t * rusb, unsigned num)\n"
        "{\n"
        "  while ( rusb->D0FIFOSEL_b.CURPIPE != num ) {}\n"
        "  while ( !rusb->D0FIFOCTR_b.FRDY ) {}\n"
        "}",
        "#define RUSB2_FIFO_READY_SPIN 100000u\n"
        "static inline bool pipe_wait_for_ready(rusb2_reg_t * rusb, unsigned num)\n"
        "{\n"
        "  uint32_t spin = RUSB2_FIFO_READY_SPIN;\n"
        "  while (rusb->D0FIFOSEL_b.CURPIPE != num) { if (!spin--) return false; }\n"
        "  spin = RUSB2_FIFO_READY_SPIN;\n"
        "  while (!rusb->D0FIFOCTR_b.FRDY) { if (!spin--) return false; }\n"
        "  return true;\n"
        "}",
    )
    text = replace_once(
        text,
        "  if (!rem) {\n"
        "    wait_pipe_fifo_empty(rusb, num);\n"
        "    pipe->buf = NULL;\n"
        "    return true;\n"
        "  }",
        "  if (!rem) {\n"
        "    pipe->buf = NULL;\n"
        "    return true;\n"
        "  }",
    )
    text = replace_once(
        text,
        "  const uint16_t mps  = edpt_max_packet_size(rusb, num);\n"
        "  pipe_wait_for_ready(rusb, num);\n"
        "  const uint16_t len  = tu_min16(rem, mps);",
        "  const uint16_t mps  = edpt_max_packet_size(rusb, num);\n"
        "  if (!pipe_wait_for_ready(rusb, num)) {\n"
        "    rusb->D0FIFOSEL = 0;\n"
        "    return false;\n"
        "  }\n"
        "  const uint16_t len  = tu_min16(rem, mps);",
    )
    text = replace_once(
        text,
        "  const uint16_t mps = edpt_max_packet_size(rusb, num);\n"
        "  pipe_wait_for_ready(rusb, num);\n\n"
        "  const uint16_t vld  = rusb->D0FIFOCTR_b.DTLN;",
        "  const uint16_t mps = edpt_max_packet_size(rusb, num);\n"
        "  if (!pipe_wait_for_ready(rusb, num)) {\n"
        "    rusb->D0FIFOSEL = 0;\n"
        "    return false;\n"
        "  }\n\n"
        "  const uint16_t vld  = rusb->D0FIFOCTR_b.DTLN;",
    )
    text = replace_once(
        text,
        "      rusb->D0FIFOSEL = num;\n"
        "      pipe_wait_for_ready(rusb, num);\n"
        "      rusb->D0FIFOCTR = RUSB2_CFIFOCTR_BVAL_Msk;",
        "      rusb->D0FIFOSEL = num;\n"
        "      if (!pipe_wait_for_ready(rusb, num)) {\n"
        "        rusb->D0FIFOSEL = 0;\n"
        "        return false;\n"
        "      }\n"
        "      rusb->D0FIFOCTR = RUSB2_CFIFOCTR_BVAL_Msk;",
    )
    return text


def patch_sketch(text: str) -> str:
    text = replace_once(
        text, '#include "secrets.h"',
        '#include "secrets.h"\n#include "UsbAudioCapture.h"',
    )
    if text.count("Serial1") != 4:
        raise RuntimeError("Expected four Serial1 references in the Uno sketch")
    # Native USB shifts the D0/D1 hardware UART from Serial1 to Serial2.
    text = text.replace("Serial1", "Serial2")
    text = replace_once(
        text,
        "  R_DAC->DADR[0] = dac;",
        "  R_DAC->DADR[0] = dac;\n  ++usbAudioDacWrites;\n  usbAudioPush(y);",
    )
    text = replace_once(
        text,
        "    audioSlipPipHoldUntil = now + 700;",
        "    audioSlipPipHoldUntil = now + 700;\n    ++usbAudioSlipEvents;",
    )
    text = replace_once(text, "void setup() {", '#include "UsbMidiInput.h"\n\nvoid setup() {')
    if not text.rstrip().endswith("\n}"):
        raise RuntimeError("Expected the Uno loop to end the sketch")
    return text.rstrip()[:-2] + "\n  usbAudioService();\n  usbMidiService();\n}\n"


def installed_paths() -> tuple[Path, Path]:
    result = subprocess.run(
        ["arduino-cli", "compile", "--fqbn", FQBN, "--show-properties", str(UNO)],
        check=True, capture_output=True, text=True,
    )
    props = dict(line.split("=", 1) for line in result.stdout.splitlines() if "=" in line)
    core = Path(props["build.core.path"])
    if core.parent.parent.name != "1.6.0":
        raise RuntimeError("This experiment is pinned to Arduino Renesas core 1.6.0")
    return core, Path(props["build.variant.path"])


def main() -> None:
    source = UNO / "QuarkWave_MIDI.ino"
    secrets = UNO / "secrets.h"
    if not secrets.is_file():
        raise RuntimeError("Local secrets.h is needed to compile the existing sketch")

    with tempfile.TemporaryDirectory(prefix="quarkwave-usb-audio-") as directory:
        stage = Path(directory)
        core = stage / "core"
        variant = stage / "variant"
        sketch = stage / "QuarkWaveUsbSynth"
        sketch.mkdir()
        source_core, source_variant = installed_paths()
        shutil.copytree(source_core, core)
        shutil.copytree(source_variant, variant)
        variant_config = variant / "tusb_config.h"
        variant_config.write_text(replace_once(
            variant_config.read_text(),
            "#define CFG_TUD_MIDI             0",
            "#define CFG_TUD_MIDI             1",
        ))
        core_usb = core / "USB" / "USB.cpp"
        core_usb.write_text(patch_core(core_usb.read_text()))
        core_boot = core / "boot.cpp"
        core_boot.write_text(patch_bootloader_reset(core_boot.read_text()))
        rusb2 = core / "tinyusb" / "rusb2" / "dcd_rusb2.c"
        rusb2.write_text(patch_rusb2_driver(rusb2.read_text()))
        (sketch / "QuarkWaveUsbSynth.ino").write_text(patch_sketch(source.read_text()))
        shutil.copy2(HERE / "UsbAudioCapture.h", sketch / "UsbAudioCapture.h")
        shutil.copy2(HERE / "UsbMidiInput.h", sketch / "UsbMidiInput.h")
        shutil.copy2(secrets, sketch / "secrets.h")
        subprocess.run(
            ["arduino-cli", "compile", "--fqbn", FQBN,
             "--build-path", str(stage / "build"),
             "--build-property", f"build.core.path={core}",
             "--build-property", f"build.variant.path={variant}",
             "--build-property", f"build.defines={FLAGS}", str(sketch)],
            check=True,
        )
        shutil.copy2(stage / "build" / "QuarkWaveUsbSynth.ino.bin", OUTPUT)
    print(f"USB audio/MIDI prototype: {OUTPUT}")


if __name__ == "__main__":
    main()
