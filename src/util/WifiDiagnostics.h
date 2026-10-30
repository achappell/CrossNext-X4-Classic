#pragma once

#include <stddef.h>
#include <stdint.h>

namespace wifidiagnostics {
enum class Kind : uint8_t { Session = 1, Attempt, Mode, Disconnect, Begin, Status, Outcome, Station, End };
enum class Source : uint8_t { Remembered = 1, Scanned, Manual };
enum class Outcome : uint8_t { Connected = 1, Failed, Timeout, Cancelled, SessionEnded };
// Station records describe delivery during the active window, not radio-generation provenance.
enum class Station : uint8_t { Start = 1, Stop, Associated, Disconnected, GotIp, LostIp };
#if defined(FREEINK_DEVICE_X4CLASSIC) && defined(CROSSNEXT_WIFI_DIAGNOSTICS) && CROSSNEXT_WIFI_DIAGNOSTICS
void startSession();
void startAttempt(bool automatic, Source source, uint32_t timeoutMs);
void record(Kind kind, uint32_t value);
void finishAttempt(Outcome outcome);
bool stopAndPublish();
#else
inline void startSession() {}
inline void startAttempt(bool, Source, uint32_t) {}
inline void record(Kind, uint32_t) {}
inline void finishAttempt(Outcome) {}
inline bool stopAndPublish() { return false; }
#endif
}  // namespace wifidiagnostics
