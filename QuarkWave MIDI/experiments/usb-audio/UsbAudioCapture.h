#pragma once

#include <Arduino.h>
#include <stdio.h>
extern "C" {
#include "tusb.h"
}

// Both audioTick() and the TinyUSB callbacks run in the sketch's main loop.
// This short ring absorbs the difference between micros() sample scheduling
// and the host's 1 ms USB audio requests.
static int16_t usbAudioRing[256];
static uint16_t usbAudioRead = 0;
static uint16_t usbAudioWrite = 0;
static bool usbAudioStreaming = false;
static uint8_t usbAudioPacketRemainder = 0;
static uint32_t usbAudioUnderruns = 0;
static uint32_t usbAudioOverruns = 0;
static uint32_t usbAudioPackets = 0;
static uint32_t usbAudioServiceCalls = 0;
static uint32_t usbAudioMaxServiceUs = 0;
static uint32_t usbAudioServiceTotalUs = 0;
static uint32_t usbAudioDacWrites = 0;
static uint32_t usbAudioSlipEvents = 0;

static inline void usbAudioPush(float sample) {
  if (!usbAudioStreaming) return;
  const uint16_t next = (usbAudioWrite + 1) & 255;
  if (next == usbAudioRead) {
    usbAudioRead = (usbAudioRead + 1) & 255;
    ++usbAudioOverruns;
  }
  usbAudioRing[usbAudioWrite] = (int16_t)(sample * 32767.0f);
  usbAudioWrite = next;
}

extern "C" bool tud_audio_set_itf_cb(uint8_t, tusb_control_request_t const* req) {
  usbAudioRead = usbAudioWrite = 0;
  usbAudioPacketRemainder = 0;
  usbAudioStreaming = (req->wValue & 0xff) != 0;
  return true;
}

extern "C" bool tud_audio_tx_done_pre_load_cb(uint8_t, uint8_t, uint8_t, uint8_t alt) {
  if (!alt) return true;
  ++usbAudioPackets;
  int16_t packet[23];
  uint8_t count = 22;
  usbAudioPacketRemainder += 5;  // 22,050 / 1000: one extra sample every 20 ms.
  if (usbAudioPacketRemainder >= 100) { usbAudioPacketRemainder -= 100; ++count; }
  for (uint8_t i = 0; i < count; ++i) {
    if (usbAudioRead != usbAudioWrite) {
      packet[i] = usbAudioRing[usbAudioRead];
      usbAudioRead = (usbAudioRead + 1) & 255;
    } else {
      packet[i] = 0;
      ++usbAudioUnderruns;
    }
  }
  return tud_audio_write(packet, count * sizeof(int16_t)) == count * sizeof(int16_t);
}

// USB work runs after MIDI, control and matrix updates. Servicing it at most
// twice per millisecond prevents a busy USB event queue from starving those
// tasks. The CDC status line is best-effort and never waits for a USB host.
static inline void usbAudioService() {
  static uint32_t lastServiceUs = 0;
  const uint32_t nowUs = micros();
  if ((uint32_t)(nowUs - lastServiceUs) < 500) return;
  lastServiceUs = nowUs;

  const uint32_t startedUs = micros();
  tud_task();
  const uint32_t elapsedUs = (uint32_t)(micros() - startedUs);
  ++usbAudioServiceCalls;
  usbAudioServiceTotalUs += elapsedUs;
  if (elapsedUs > usbAudioMaxServiceUs) usbAudioMaxServiceUs = elapsedUs;

  static uint32_t lastReportMs = 0;
  const uint32_t nowMs = millis();
  if ((uint32_t)(nowMs - lastReportMs) < 500 || !tud_cdc_connected()) return;
  lastReportMs = nowMs;
  if (tud_cdc_write_available() < 128) return;
  char line[128];
  const int len = snprintf(line, sizeof(line),
      "USB stream=%u packets=%lu task=%lu max=%lu underrun=%lu overrun=%lu dac=%lu slip=%lu taskUs=%lu\r\n",
      (unsigned)usbAudioStreaming, (unsigned long)usbAudioPackets,
      (unsigned long)usbAudioServiceCalls, (unsigned long)usbAudioMaxServiceUs,
      (unsigned long)usbAudioUnderruns, (unsigned long)usbAudioOverruns,
      (unsigned long)usbAudioDacWrites, (unsigned long)usbAudioSlipEvents,
      (unsigned long)usbAudioServiceTotalUs);
  if (len > 0 && len < (int)sizeof(line)) {
    tud_cdc_write(line, (uint32_t)len);
    tud_cdc_write_flush();
  }
}

extern "C" bool tud_audio_get_req_entity_cb(uint8_t rhport, tusb_control_request_t const* req) {
  const uint8_t entity = req->wIndex >> 8;
  const uint8_t control = req->wValue >> 8;
  if (entity == 1 && control == AUDIO_TE_CTRL_CONNECTOR && req->bRequest == AUDIO_CS_REQ_CUR) {
    static const audio_desc_channel_cluster_t cluster = {1, AUDIO_CHANNEL_CONFIG_NON_PREDEFINED, 0};
    return tud_audio_buffer_and_schedule_control_xfer(rhport, req, (void*)&cluster, sizeof(cluster));
  }
  if (entity == 2 && control == AUDIO_FU_CTRL_MUTE && req->bRequest == AUDIO_CS_REQ_CUR) {
    static const audio_control_cur_1_t mute = {0};
    return tud_audio_buffer_and_schedule_control_xfer(rhport, req, (void*)&mute, sizeof(mute));
  }
  if (entity == 2 && control == AUDIO_FU_CTRL_VOLUME && req->bRequest == AUDIO_CS_REQ_CUR) {
    static const audio_control_cur_2_t volume = {0};
    return tud_audio_buffer_and_schedule_control_xfer(rhport, req, (void*)&volume, sizeof(volume));
  }
  if (entity == 2 && control == AUDIO_FU_CTRL_VOLUME && req->bRequest == AUDIO_CS_REQ_RANGE) {
    static const audio_control_range_2_n_t(1) range = {1, {{-60 * 256, 0, 256}}};
    return tud_audio_buffer_and_schedule_control_xfer(rhport, req, (void*)&range, sizeof(range));
  }
  if (entity == 4 && control == AUDIO_CS_CTRL_SAM_FREQ && req->bRequest == AUDIO_CS_REQ_CUR) {
    static const audio_control_cur_4_t rate = {22050};
    return tud_audio_buffer_and_schedule_control_xfer(rhport, req, (void*)&rate, sizeof(rate));
  }
  if (entity == 4 && control == AUDIO_CS_CTRL_SAM_FREQ && req->bRequest == AUDIO_CS_REQ_RANGE) {
    static const audio_control_range_4_n_t(1) range = {1, {{22050, 22050, 0}}};
    return tud_audio_buffer_and_schedule_control_xfer(rhport, req, (void*)&range, sizeof(range));
  }
  if (entity == 4 && control == AUDIO_CS_CTRL_CLK_VALID && req->bRequest == AUDIO_CS_REQ_CUR) {
    static const audio_control_cur_1_t valid = {1};
    return tud_audio_buffer_and_schedule_control_xfer(rhport, req, (void*)&valid, sizeof(valid));
  }
  return false;
}

extern "C" bool tud_audio_set_req_entity_cb(uint8_t, tusb_control_request_t const* req, uint8_t*) {
  const uint8_t entity = req->wIndex >> 8;
  const uint8_t control = req->wValue >> 8;
  return entity == 2 && req->bRequest == AUDIO_CS_REQ_CUR &&
      (control == AUDIO_FU_CTRL_MUTE || control == AUDIO_FU_CTRL_VOLUME);
}
