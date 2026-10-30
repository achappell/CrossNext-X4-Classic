#include "WifiDiagnostics.h"

#if defined(FREEINK_DEVICE_X4CLASSIC) && defined(CROSSNEXT_WIFI_DIAGNOSTICS) && CROSSNEXT_WIFI_DIAGNOSTICS
#include <Arduino.h>
#include <HalStorage.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <stdio.h>
#include <stdint.h>

namespace wifidiagnostics {
namespace {
constexpr size_t Capacity = 128;
constexpr size_t MaxBytes = 16 * 1024;
constexpr const char* Temp = "/.crosspoint/classic-wifi-diagnostic.tmp";
constexpr const char* Final = "/.crosspoint/classic-wifi-diagnostic.log";
struct Record { uint32_t elapsed, attempt, attemptElapsed, kind, value, extra; };
Record records[Capacity];
portMUX_TYPE mutex = portMUX_INITIALIZER_UNLOCKED;
size_t count = 0;
uint32_t dropped = 0, sessionStart = 0, attemptStart = 0, ordinal = 0;
uint32_t lastStatus = UINT32_MAX;
uint32_t finalOutcome = 0;
bool active = false, attempting = false, registered = false;
void append(Kind kind, uint32_t value, uint32_t extra, uint32_t now) {
  if (!active) return;
  if (count == Capacity) { if (dropped != UINT32_MAX) ++dropped; return; }
  records[count++] = {now - sessionStart, attempting ? ordinal : 0,
                      attempting ? now - attemptStart : 0, static_cast<uint32_t>(kind), value, extra};
}
void onEvent(arduino_event_t* event) {
  Station station;
  uint32_t extra = 0;
  switch (event->event_id) {
    case ARDUINO_EVENT_WIFI_STA_START: station = Station::Start; break;
    case ARDUINO_EVENT_WIFI_STA_STOP: station = Station::Stop; break;
    case ARDUINO_EVENT_WIFI_STA_CONNECTED: station = Station::Associated; break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      station = Station::Disconnected; extra = event->event_info.wifi_sta_disconnected.reason; break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP: station = Station::GotIp; extra = 1; break;
    case ARDUINO_EVENT_WIFI_STA_LOST_IP: station = Station::LostIp; break;
    default: return;
  }
  portENTER_CRITICAL(&mutex);
  const uint32_t now = millis();
  append(Kind::Station, static_cast<uint32_t>(station), extra, now);
  portEXIT_CRITICAL(&mutex);
}
}  // namespace

void startSession() {
  if (!registered) { WiFi.onEvent(onEvent); registered = true; }
  portENTER_CRITICAL(&mutex);
  const uint32_t now = millis();
  count = 0; dropped = 0; ordinal = 0; finalOutcome = 0; attempting = false; sessionStart = now; active = true;
  append(Kind::Session, 1, 0, now);
  portEXIT_CRITICAL(&mutex);
}
void startAttempt(bool automatic, Source source, uint32_t timeoutMs) {
  portENTER_CRITICAL(&mutex);
  const uint32_t now = millis();
  if (active) {
    if (attempting) append(Kind::Outcome, static_cast<uint32_t>(Outcome::Cancelled), 0, now);
    if (ordinal != UINT32_MAX) ++ordinal;
    attemptStart = now; attempting = true; lastStatus = UINT32_MAX;
    finalOutcome = 0;
    append(Kind::Attempt, static_cast<uint32_t>(source), automatic ? 1 : 0, now);
    append(Kind::Attempt, timeoutMs, 2, now);
  }
  portEXIT_CRITICAL(&mutex);
}
void record(Kind kind, uint32_t value) {
  portENTER_CRITICAL(&mutex);
  const uint32_t now = millis();
  if (kind != Kind::Status || value != lastStatus) {
    append(kind, value, 0, now);
    if (kind == Kind::Status) lastStatus = value;
  }
  portEXIT_CRITICAL(&mutex);
}
void finishAttempt(Outcome outcome) {
  portENTER_CRITICAL(&mutex);
  const uint32_t now = millis();
  if (attempting) {
    finalOutcome = static_cast<uint32_t>(outcome);
    append(Kind::Outcome, finalOutcome, 0, now);
  }
  attempting = false;
  portEXIT_CRITICAL(&mutex);
}
bool stopAndPublish() {
  portENTER_CRITICAL(&mutex);
  const uint32_t now = millis();
  if (!active) { portEXIT_CRITICAL(&mutex); return false; }
  if (attempting) {
    finalOutcome = static_cast<uint32_t>(Outcome::SessionEnded);
    append(Kind::Outcome, finalOutcome, 0, now);
  }
  attempting = false;
  append(Kind::End, 0, 0, now);
  active = false;
  portEXIT_CRITICAL(&mutex);
  // Capture is stopped before any formatting or storage; callbacks now cannot mutate records.
  if (!Storage.ready() || !Storage.ensureDirectoryExists("/.crosspoint")) return false;
  HalFile file;
  if (!Storage.openFileForWrite("WIFIDIAG", Temp, file)) return false;
  size_t bytes = 0;
  bool ok = true;
  auto writeLine = [&](const char* line, size_t length) {
    if (length > MaxBytes - bytes || file.write(reinterpret_cast<const uint8_t*>(line), length) != length) {
      ok = false; return;
    }
    bytes += length;
  };
  const char header[] = "classic-wifi-diagnostic,1,delivery-window\n";
  writeLine(header, sizeof(header) - 1);
  char line[128];
  for (size_t i = 0; i < count && ok; ++i) {
    const auto& r = records[i];
    const int n = snprintf(line, sizeof(line), "%lu,%lu,%lu,%lu,%lu,%lu\n",
      static_cast<unsigned long>(r.elapsed), static_cast<unsigned long>(r.attempt),
      static_cast<unsigned long>(r.attemptElapsed), static_cast<unsigned long>(r.kind),
      static_cast<unsigned long>(r.value), static_cast<unsigned long>(r.extra));
    if (n <= 0 || static_cast<size_t>(n) >= sizeof(line)) { ok = false; break; }
    writeLine(line, static_cast<size_t>(n));
  }
  const int n = snprintf(line, sizeof(line), "summary,%lu,%lu,%lu,%u,%lu,%lu\n", static_cast<unsigned long>(count),
      static_cast<unsigned long>(dropped), static_cast<unsigned long>(now - sessionStart), dropped ? 1 : 0,
      static_cast<unsigned long>(ordinal), static_cast<unsigned long>(finalOutcome));
  if (ok && n > 0 && static_cast<size_t>(n) < sizeof(line)) writeLine(line, static_cast<size_t>(n));
  else ok = false;
  file.flush();  // The storage API supplies no durability result.
  const bool closed = file.close();
  if (!ok || !closed) { Storage.remove(Temp); return false; }
  if (!Storage.replaceFile(Temp, Final)) { Storage.remove(Temp); return false; }
  return true;
}
}  // namespace wifidiagnostics
#endif
