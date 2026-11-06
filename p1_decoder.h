// p1_decoder.h : décodeur HDLC/DLMS non chiffré pour compteur AM550 (port P1)
// Testé sur PC contre 3 messages réels (CRC/FCS valides, valeurs cohérentes).
// À placer à côté du YAML dans le dossier de configuration ESPHome.
#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace p1 {

static const char *const TAG = "p1";

constexpr size_t BUF_MAX = 2048;
constexpr uint16_t ALL_FIELDS = 0x3FF;  // 10 champs : P, Etot, T1, T2, V1-3, I1-3

struct Meter {
  float power_w = 0, e_total_kwh = 0, e_t1_kwh = 0, e_t2_kwh = 0;
  float v[3] = {0, 0, 0};
  float i[3] = {0, 0, 0};
};

inline Meter g_meter;        // en cours de remplissage
inline Meter g_ready_meter;  // dernier message complet
inline bool g_ready = false;
inline uint16_t g_mask = 0;
inline uint8_t g_payload[BUF_MAX];
inline size_t g_payload_len = 0;
inline uint32_t g_bytes = 0;      // octets reçus sur l'UART (diagnostic)
inline uint32_t g_frames_ok = 0;  // trames HDLC valides (longueur + FCS) (diagnostic)

// ---------- CRC HDLC (X.25) ----------
inline uint16_t crc16_x25(const uint8_t *d, size_t n) {
  uint16_t crc = 0xFFFF;
  while (n--) {
    crc ^= *d++;
    for (int b = 0; b < 8; b++) crc = (crc & 1) ? (crc >> 1) ^ 0x8408 : crc >> 1;
  }
  return crc ^ 0xFFFF;
}

// ---------- Lecture d'une valeur DLMS ----------
inline int read_value(const uint8_t *p, size_t left, int64_t *out) {
  if (left < 2) return 0;
  switch (p[0]) {
    case 0x11: *out = p[1]; return 2;
    case 0x0F: *out = (int8_t) p[1]; return 2;
    case 0x12:
      if (left < 3) return 0;
      *out = (uint16_t) (p[1] << 8 | p[2]);
      return 3;
    case 0x10:
      if (left < 3) return 0;
      *out = (int16_t) (p[1] << 8 | p[2]);
      return 3;
    case 0x06:
      if (left < 5) return 0;
      *out = (uint32_t) p[1] << 24 | (uint32_t) p[2] << 16 | (uint32_t) p[3] << 8 | p[4];
      return 5;
    case 0x05:
      if (left < 5) return 0;
      *out = (int32_t) ((uint32_t) p[1] << 24 | (uint32_t) p[2] << 16 | (uint32_t) p[3] << 8 | p[4]);
      return 5;
    case 0x15: {
      if (left < 9) return 0;
      uint64_t v = 0;
      for (int i = 1; i < 9; i++) v = (v << 8) | p[i];
      *out = (int64_t) v;
      return 9;
    }
    default: return 0;
  }
}

// ---------- Extraction OBIS ("09 06 <OBIS 6 octets>" puis valeur) ----------
// Échelles confirmées par les trames : V x0.1, I x0.01, énergies en Wh, puissance en W.
inline void parse_dlms(const uint8_t *p, size_t n) {
  for (size_t i = 0; i + 8 < n; i++) {
    if (p[i] != 0x09 || p[i + 1] != 0x06) continue;
    const uint8_t *o = &p[i + 2];
    if (o[0] != 1 || o[1] != 0) continue;
    int64_t v;
    if (!read_value(&p[i + 8], n - (i + 8), &v)) continue;
    uint8_t c = o[2], d = o[3], e = o[4];
    if (c == 1 && d == 7 && e == 0) { g_meter.power_w = (float) v; g_mask |= 1 << 0; }
    else if (c == 1 && d == 8 && e == 0) { g_meter.e_total_kwh = v / 1000.0f; g_mask |= 1 << 1; }
    else if (c == 1 && d == 8 && e == 1) { g_meter.e_t1_kwh = v / 1000.0f; g_mask |= 1 << 2; }
    else if (c == 1 && d == 8 && e == 2) { g_meter.e_t2_kwh = v / 1000.0f; g_mask |= 1 << 3; }
    else if (d == 7 && e == 0) {
      if (c == 32) { g_meter.v[0] = v / 10.0f; g_mask |= 1 << 4; }
      if (c == 52) { g_meter.v[1] = v / 10.0f; g_mask |= 1 << 5; }
      if (c == 72) { g_meter.v[2] = v / 10.0f; g_mask |= 1 << 6; }
      if (c == 31) { g_meter.i[0] = v / 100.0f; g_mask |= 1 << 7; }
      if (c == 51) { g_meter.i[1] = v / 100.0f; g_mask |= 1 << 8; }
      if (c == 71) { g_meter.i[2] = v / 100.0f; g_mask |= 1 << 9; }
    }
  }
}

// ---------- Trame HDLC complète (entre les flags 0x7E) ----------
inline int handle_frame(const uint8_t *f, size_t n) {
  if (n < 9 || (f[0] & 0xF0) != 0xA0) { g_payload_len = 0; return 0; }
  size_t len = ((f[0] & 0x07) << 8) | f[1];
  if (len != n) { ESP_LOGW(TAG, "longueur HDLC %u != %u", (unsigned) len, (unsigned) n); g_payload_len = 0; return 0; }
  uint16_t fcs = f[n - 2] | (f[n - 1] << 8);
  if (crc16_x25(f, n - 2) != fcs) { ESP_LOGW(TAG, "FCS invalide, message abandonne"); g_payload_len = 0; return 0; }

  g_frames_ok++;
  bool segmented = f[0] & 0x08;
  size_t pos = 2;
  while (pos < n && !(f[pos] & 1)) pos++;  // adresse destination
  pos++;
  while (pos < n && !(f[pos] & 1)) pos++;  // adresse source
  pos++;
  pos++;                                   // octet de contrôle
  if (pos + 2 > n - 2) return 0;
  pos += 2;                                // HCS

  size_t info = (n - 2) - pos;
  const uint8_t *data = &f[pos];
  if (info >= 3 && data[0] == 0xE6 && data[1] == 0xE7 && data[2] == 0x00) g_payload_len = 0;  // début de message
  if (g_payload_len + info > BUF_MAX) { g_payload_len = 0; return 0; }
  memcpy(&g_payload[g_payload_len], data, info);
  g_payload_len += info;

  if (!segmented) {
    g_mask = 0;
    parse_dlms(g_payload, g_payload_len);
    g_payload_len = 0;
    if (g_mask == ALL_FIELDS) {
      g_ready_meter = g_meter;
      g_ready = true;
    } else {
      ESP_LOGW(TAG, "message incomplet (masque 0x%03X), non publie", g_mask);
    }
  }
  return 1;
}

// ---------- Découpage par LONGUEUR (un octet de donnée peut valoir 0x7E) ----------
enum State { S_IDLE, S_FLAG, S_LEN, S_BODY, S_END };
inline State g_st = S_IDLE;
inline uint8_t g_frame[BUF_MAX];
inline size_t g_flen = 0, g_want = 0;
inline uint32_t g_last_byte_ms = 0;

inline void reset() { g_st = S_IDLE; g_flen = 0; g_payload_len = 0; }

inline void feed(uint8_t b) {
  g_last_byte_ms = esphome::millis();
  g_bytes++;
  switch (g_st) {
    case S_IDLE:
      if (b == 0x7E) g_st = S_FLAG;
      break;
    case S_FLAG:
      if (b == 0x7E) break;
      if ((b & 0xF0) == 0xA0) { g_frame[0] = b; g_flen = 1; g_st = S_LEN; }
      else g_st = S_IDLE;
      break;
    case S_LEN:
      g_frame[1] = b;
      g_flen = 2;
      g_want = ((g_frame[0] & 0x07) << 8) | b;
      g_st = (g_want >= 9 && g_want <= BUF_MAX) ? S_BODY : S_IDLE;
      break;
    case S_BODY:
      g_frame[g_flen++] = b;
      if (g_flen == g_want) g_st = S_END;
      break;
    case S_END:
      if (b == 0x7E) {
        handle_frame(g_frame, g_flen);
        g_st = S_FLAG;
      } else {
        ESP_LOGW(TAG, "0x7E de fin attendu, recu 0x%02X : resynchronisation", b);
        g_payload_len = 0;
        g_st = S_IDLE;
      }
      break;
  }
}

// À appeler régulièrement : si le flux se tait en pleine trame, on repart de zéro
inline void tick() {
  if (g_st != S_IDLE && esphome::millis() - g_last_byte_ms > 1000) reset();
}

// Récupère le dernier message complet (une seule fois)
inline bool take(Meter &out) {
  if (!g_ready) return false;
  out = g_ready_meter;
  g_ready = false;
  return true;
}

}  // namespace p1
