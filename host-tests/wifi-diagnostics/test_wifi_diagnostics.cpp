#include "WifiDiagnostics.h"
#include <HalStorage.h>
#include <WiFi.h>
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <algorithm>
uint32_t fakeMillis = 0;
void (*lockEntryHook)() = nullptr;
std::string fsRoot;
bool storageReady = true, shortWrite = false, closeFailure = false, replaceFailure = false, removeFailure = false, openFailure = false;
unsigned storageCalls = 0;
FakeWiFi WiFi;
FakeStorage Storage;
using namespace wifidiagnostics;
[[maybe_unused]] static std::string load() {
  std::ifstream file(fsRoot + "/.crosspoint/classic-wifi-diagnostic.log");
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
static void attempt() { startSession(); startAttempt(true, Source::Remembered, 7000); }
int main(int argc, char** argv) {
  assert(argc == 2); fsRoot = argv[1];
#if defined(FREEINK_DEVICE_X4CLASSIC) && defined(CROSSNEXT_WIFI_DIAGNOSTICS) && CROSSNEXT_WIFI_DIAGNOSTICS
  // Runs the production observer/encoder/publisher, not assertions on activity wiring.
  // This shim cannot prove SDK task scheduling, SD/FAT durability or physical WiFi/USB behavior.
  fakeMillis = UINT32_MAX - 10;
  attempt();
  assert(storageCalls == 0 && WiFi.registrations == 1);
  record(Kind::Mode, 1); record(Kind::Disconnect, 1); record(Kind::Begin, 4);
  record(Kind::Status, 0); record(Kind::Status, 0);
  fakeMillis = 5;
  WiFi.deliver(ARDUINO_EVENT_WIFI_STA_DISCONNECTED, 201);
  WiFi.deliver(ARDUINO_EVENT_WIFI_STA_GOT_IP);
  finishAttempt(Outcome::Failed);
  assert(storageCalls == 0);
  WiFi.deliver(ARDUINO_EVENT_WIFI_STA_LOST_IP);
  assert(stopAndPublish());
  const auto first = load();
  assert(first.find("16,1,16,8,4,201\n") != std::string::npos);
  assert(first.find("16,0,0,8,6,0\n") != std::string::npos);
  assert(first.find(",5,4,0\n") != std::string::npos);
  assert(first.find("summary,12,0,16,0,1,2\n") != std::string::npos);
  const unsigned calls = storageCalls;
  WiFi.deliver(ARDUINO_EVENT_WIFI_STA_START);
  assert(!stopAndPublish() && storageCalls == calls && load() == first);
  attempt();
  finishAttempt(Outcome::Cancelled);
  startAttempt(false, Source::Manual, 15000);
  // A late event is attributed to attempt 2 at delivery, with no activity pointer.
  WiFi.deliver(ARDUINO_EVENT_WIFI_STA_STOP);
  finishAttempt(Outcome::Connected);
  assert(stopAndPublish() && WiFi.registrations == 1);
  assert(load().find("0,2,0,8,2,0\n") != std::string::npos);
  assert(load().find("summary,9,0,0,0,2,1\n") != std::string::npos);
  const auto previous = load();
  for (int failure = 0; failure < 5; ++failure) {
    attempt();
    storageReady = failure != 0; openFailure = failure == 1; shortWrite = failure == 2;
    closeFailure = failure == 3; removeFailure = failure == 4;
    assert(!stopAndPublish());
    assert(load() == previous);
    assert(!std::filesystem::exists(fsRoot + "/.crosspoint/classic-wifi-diagnostic.tmp"));
    storageReady = true; openFailure = shortWrite = closeFailure = removeFailure = false;
  }
  attempt(); replaceFailure = true;
  assert(!stopAndPublish()); replaceFailure = false;
  assert(!std::filesystem::exists(fsRoot + "/.crosspoint/classic-wifi-diagnostic.log"));
  assert(!std::filesystem::exists(fsRoot + "/.crosspoint/classic-wifi-diagnostic.tmp"));
  attempt();
  for (unsigned i = 0; i < 300; ++i) { ++fakeMillis; WiFi.deliver(ARDUINO_EVENT_WIFI_STA_DISCONNECTED, 202); }
  finishAttempt(Outcome::Timeout);
  assert(stopAndPublish());
  const auto overflow = load();
  assert(overflow.size() <= 16384);
  assert(overflow.find("summary,128,177,300,1,1,3\n") != std::string::npos);
  // All data rows contain numeric scalars only, with bounded schema constants.
  std::istringstream lines(overflow); std::string line; unsigned rows = 0;
  std::getline(lines, line); assert(line == "classic-wifi-diagnostic,1,delivery-window");
  while (std::getline(lines, line)) {
    if (line.rfind("summary,", 0) == 0) continue;
    assert(std::count(line.begin(), line.end(), ',') == 5);
    assert(std::all_of(line.begin(), line.end(), [](char c) { return (c >= '0' && c <= '9') || c == ','; }));
    ++rows;
  }
  assert(rows == 128);
  attempt(); WiFi.deliver(OTHER_EVENT); assert(stopAndPublish());
  assert(load().find("summary,5,0,0,0,1,5\n") != std::string::npos);
  fakeMillis = 1000;
  attempt();
  // Simulate waiting at lock entry: delivery timestamps must be sampled afterwards.
  lockEntryHook = [] { fakeMillis = 1025; };
  WiFi.deliver(ARDUINO_EVENT_WIFI_STA_START);
  assert(lockEntryHook == nullptr);
  finishAttempt(Outcome::Connected);
  assert(stopAndPublish());
  assert(load().find("25,1,25,8,1,0\n") != std::string::npos);
  struct Metadata { bool automatic; Source source; uint32_t deadline; };
  const Metadata cases[] = {{true, Source::Remembered, 7000}, {true, Source::Scanned, 7000},
                            {false, Source::Scanned, 15000}, {false, Source::Manual, 15000}};
  for (const auto& metadata : cases) {
    fakeMillis = 2000; startSession();
    startAttempt(metadata.automatic, metadata.source, metadata.deadline);
    assert(stopAndPublish());
    const auto trace = load();
    const auto source = "0,1,0,2," + std::to_string(static_cast<unsigned>(metadata.source)) +
                        "," + std::to_string(metadata.automatic) + "\n";
    assert(trace.find(source) != std::string::npos);
    assert(trace.find("0,1,0,2," + std::to_string(metadata.deadline) + ",2\n") != std::string::npos);
  }
  fakeMillis = 3000;
  lockEntryHook = [] { fakeMillis = 3100; };
  startSession();
  lockEntryHook = [] { fakeMillis = 3110; WiFi.deliver(ARDUINO_EVENT_WIFI_STA_START); fakeMillis = 3120; };
  startAttempt(false, Source::Manual, 15000);
  lockEntryHook = [] { fakeMillis = 3130; WiFi.deliver(ARDUINO_EVENT_WIFI_STA_CONNECTED); fakeMillis = 3140; };
  record(Kind::Begin, 0);
  lockEntryHook = [] { fakeMillis = 3150; WiFi.deliver(ARDUINO_EVENT_WIFI_STA_GOT_IP); fakeMillis = 3160; };
  finishAttempt(Outcome::Connected);
  lockEntryHook = [] { fakeMillis = 3170; WiFi.deliver(ARDUINO_EVENT_WIFI_STA_STOP); fakeMillis = 3180; };
  assert(stopAndPublish());
  const auto ordered = load();
  assert(ordered.find("10,0,0,8,1,0\n20,1,0,2,3,0\n") != std::string::npos);
  assert(ordered.find("40,1,20,5,0,0\n") != std::string::npos);
  assert(ordered.find("60,1,40,7,1,0\n") != std::string::npos);
  assert(ordered.find("80,0,0,9,0,0\nsummary,10,0,80,0,1,1\n") != std::string::npos);
#else
  attempt(); record(Kind::Begin, 4); finishAttempt(Outcome::Failed);
  assert(!stopAndPublish());
  assert(storageCalls == 0 && WiFi.registrations == 0);
  assert(std::filesystem::is_empty(fsRoot));
#endif
  std::cout << "wifi diagnostics assertions passed\n";
}
