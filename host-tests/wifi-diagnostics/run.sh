#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
mkdir -p "$TMP/freertos"
cat > "$TMP/Arduino.h" <<'EOF'
#pragma once
#include <stdint.h>
extern uint32_t fakeMillis;
inline uint32_t millis() { return fakeMillis; }
inline void delay(unsigned ms) { fakeMillis += ms; }
EOF
cat > "$TMP/freertos/FreeRTOS.h" <<'EOF'
#pragma once
EOF
cat > "$TMP/freertos/portmacro.h" <<'EOF'
#pragma once
#include <mutex>
using portMUX_TYPE = std::mutex;
extern void (*lockEntryHook)();
inline void enterCritical(portMUX_TYPE* mutex) {
  auto hook = lockEntryHook;
  lockEntryHook = nullptr;
  if (hook) hook();
  mutex->lock();
}
#define portMUX_INITIALIZER_UNLOCKED {}
#define portENTER_CRITICAL(x) enterCritical(x)
#define portEXIT_CRITICAL(x) (x)->unlock()
EOF
cat > "$TMP/WiFi.h" <<'EOF'
#pragma once
#include <stdint.h>
#include <string>
enum wl_status_t { WL_IDLE_STATUS = 0, WL_NO_SSID_AVAIL = 1, WL_CONNECTED = 3, WL_CONNECT_FAILED = 4 };
constexpr int WIFI_MODE_NULL = 0, WIFI_OFF = 0, WIFI_STA = 1;
constexpr int WIFI_ALL_CHANNEL_SCAN = 0, WIFI_CONNECT_AP_BY_SIGNAL = 0;
struct IPAddress {
  unsigned operator[](unsigned) const { return 0; }
  std::string toString() const { return "test-address-not-captured"; }
};
enum WiFiEvent_t { ARDUINO_EVENT_WIFI_STA_START, ARDUINO_EVENT_WIFI_STA_STOP,
 ARDUINO_EVENT_WIFI_STA_CONNECTED, ARDUINO_EVENT_WIFI_STA_DISCONNECTED,
 ARDUINO_EVENT_WIFI_STA_GOT_IP, ARDUINO_EVENT_WIFI_STA_LOST_IP, OTHER_EVENT };
struct WiFiEventInfo_t { struct { uint16_t reason; } wifi_sta_disconnected; };
struct arduino_event_t { WiFiEvent_t event_id; WiFiEventInfo_t event_info; };
struct FakeWiFi {
 using Callback = void (*)(arduino_event_t*);
 Callback callback = nullptr;
 unsigned registrations = 0;
 wl_status_t currentStatus = WL_IDLE_STATUS;
 int currentMode = WIFI_MODE_NULL;
 std::string ssid;
 unsigned begins = 0, disconnects = 0, modes = 0;
 wl_status_t status() const { return currentStatus; }
 std::string SSID() const { return ssid; }
 int RSSI() const { return -50; }
 int getMode() const { return currentMode; }
 bool mode(int m) { ++modes; currentMode = m; return true; }
 bool disconnect(bool = false, bool = false) { ++disconnects; return true; }
 void scanDelete() {}
 void scanNetworks(bool) {}
 void persistent(bool) {}
 void setScanMethod(int) {}
 void setSortMethod(int) {}
 void setHostname(const char*) {}
 wl_status_t begin(const char* name, const char* = nullptr) { ++begins; ssid = name; return WL_CONNECT_FAILED; }
 IPAddress localIP() const { return {}; }
 void onEvent(Callback cb) { callback = cb; ++registrations; }
 void deliver(WiFiEvent_t event, uint16_t reason = 0) {
   arduino_event_t payload{event, {{reason}}};
   if (callback) callback(&payload);
 }
};
extern FakeWiFi WiFi;
EOF
cat > "$TMP/HalStorage.h" <<'EOF'
#pragma once
#include <cstdio>
#include <filesystem>
#include <string>
#include <stdint.h>
extern std::string fsRoot;
extern bool storageReady, shortWrite, closeFailure, replaceFailure, removeFailure, openFailure;
extern unsigned storageCalls;
struct HalFile {
 FILE* file = nullptr;
 ~HalFile() { if (file) fclose(file); }
 size_t write(const uint8_t* data, size_t n) {
   ++storageCalls;
   return fwrite(data, 1, shortWrite && n ? n - 1 : n, file);
 }
 void flush() { if (file) fflush(file); }
 bool close() { if (!file) return false; bool ok = fclose(file) == 0; file = nullptr; return ok && !closeFailure; }
};
struct FakeStorage {
 bool ready() { ++storageCalls; return storageReady; }
 bool ensureDirectoryExists(const char* path) {
   ++storageCalls; std::error_code ec; std::filesystem::create_directories(fsRoot + path, ec); return !ec;
 }
 bool openFileForWrite(const char*, const char* path, HalFile& out) {
   ++storageCalls; if (openFailure) return false; out.file = fopen((fsRoot + path).c_str(), "wb"); return out.file;
 }
 bool remove(const char* path) { ++storageCalls; return std::remove((fsRoot + path).c_str()) == 0; }
 bool replaceFile(const char* from, const char* to) {
   ++storageCalls;
   const std::string destination = fsRoot + to;
   if (std::filesystem::exists(destination) && (removeFailure || std::remove(destination.c_str()) != 0)) return false;
   if (replaceFailure) return false;
   return std::rename((fsRoot + from).c_str(), destination.c_str()) == 0;
 }
};
extern FakeStorage Storage;
EOF
CXX=${CXX:-c++}
for mode in enabled disabled nonclassic; do
  case "$mode" in
    enabled) FLAGS='-DFREEINK_DEVICE_X4CLASSIC=1 -DCROSSNEXT_WIFI_DIAGNOSTICS=1';;
    disabled) FLAGS='-DFREEINK_DEVICE_X4CLASSIC=1';;
    nonclassic) FLAGS='-DCROSSNEXT_WIFI_DIAGNOSTICS=1';;
  esac
  "$CXX" -std=c++17 -Wall -Wextra -Werror $FLAGS -I"$TMP" -I"$ROOT/src/util" \
    "$ROOT/src/util/WifiDiagnostics.cpp" "$ROOT/host-tests/wifi-diagnostics/test_wifi_diagnostics.cpp" -o "$TMP/$mode"
  mkdir "$TMP/fs-$mode"
  "$TMP/$mode" "$TMP/fs-$mode"
done
# The real production header is used; only its environmental includes are substituted.
mkdir -p "$TMP/activities" "$TMP/components" "$TMP/util"
for header in activities/Activity.h components/UiAppHost.h util/ButtonNavigator.h; do
  printf '#pragma once\n' > "$TMP/$header"
done
python3 "$ROOT/host-tests/wifi-diagnostics/extract_activity.py" \
  "$ROOT/src/activities/network/WifiSelectionActivity.cpp" "$TMP/activity.cpp"
"$CXX" -std=c++17 -Wall -Wextra -Werror -fno-access-control \
  -DFREEINK_DEVICE_X4CLASSIC=1 -DCROSSNEXT_WIFI_DIAGNOSTICS=1 \
  -I"$TMP" -I"$ROOT/host-tests/wifi-diagnostics" -I"$ROOT/src/util" -I"$ROOT/src" \
  "$ROOT/src/util/WifiDiagnostics.cpp" "$TMP/activity.cpp" \
  "$ROOT/host-tests/wifi-diagnostics/test_activity.cpp" -o "$TMP/consumer"
mkdir "$TMP/fs-consumer"
"$TMP/consumer" "$TMP/fs-consumer"
