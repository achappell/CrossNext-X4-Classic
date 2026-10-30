#include "consumer_environment.h"
#include <fstream>
#include <iostream>
uint32_t fakeMillis = 0;
void (*lockEntryHook)() = nullptr;
std::string fsRoot, keyboardText = "manual-private-ssid";
bool storageReady = true, shortWrite = false, closeFailure = false, replaceFailure = false, removeFailure = false, openFailure = false;
unsigned storageCalls = 0;
FakeWiFi WiFi;
FakeStorage Storage;
CredentialStore WIFI_STORE;
Clock halClock;
Settings SETTINGS;
Esp ESP;
bool finished = false, resultConnected = false, resumedAfterPublication = false;
// Unexercised UI scheduling/render entry points. Route/lifecycle bodies are extracted unchanged.
void WifiSelectionActivity::loop() {}
void WifiSelectionActivity::render(RenderLock&&) {}
void WifiSelectionActivity::listScreen(UiScreen&, void*) {}
void WifiSelectionActivity::onRowEvent(const fui::ActionEvent&, void*) {}
void WifiSelectionActivity::onScanEvent(const fui::ActionEvent&, void*) {}
void WifiSelectionActivity::onPromptEvent(const fui::ActionEvent&, void*) {}
static std::string readLog() {
  std::ifstream file(fsRoot + "/.crosspoint/classic-wifi-diagnostic.log");
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
int main(int argc, char** argv) {
  assert(argc == 2); fsRoot = argv[1];
  GfxRenderer renderer; MappedInputManager input;
  for (unsigned route = 0; route < 4; ++route) {
    std::filesystem::remove_all(fsRoot + "/.crosspoint");
    WIFI_STORE.credentials = {{"remembered-private-ssid", "private-password"}, {"scanned-private-ssid", "saved-password"}};
    WIFI_STORE.last = route == 0 ? "remembered-private-ssid" : "";
    WiFi.currentStatus = WL_IDLE_STATUS;
    const unsigned previousBegins = WiFi.begins;
    const unsigned before = storageCalls;
    resumedAfterPublication = false; finished = false;
    {
      WifiSelectionActivity activity(renderer, input, route < 2);
      activity.onEnter();
      assert(storageCalls == before && !std::filesystem::exists(fsRoot + "/.crosspoint/classic-wifi-diagnostic.log"));
      if (route == 1 || route == 2) {
        activity.networks = {{"scanned-private-ssid", -50, true, true, false}};
        activity.realNetworkCount = 1;
        activity.selectedNetworkIndex = 0;
        if (route == 1) assert(activity.tryNextSavedNetworkFromScan());
        else activity.selectNetwork(0);
      } else if (route == 3) {
        activity.networks = {{"", 0, true, false, true}};
        activity.realNetworkCount = 0;
        activity.selectedNetworkIndex = 0;
        activity.selectNetwork(0); // Real hidden-SSID prompt consumes the keyboard adapter result.
        assert(activity.selectedSSID == keyboardText);
        activity.state = WifiSelectionState::PASSWORD_REVIEW; // The excluded frame scheduler's handoff.
        WifiSelectionActivity::onReferenceEvent({Connect, false}, &activity);
      }
      assert(WiFi.begins == previousBegins + 1 && storageCalls == before);
      if (route == 1) {
        activity.showNetworkListFromAutoConnect();
        assert(activity.state == WifiSelectionState::NETWORK_LIST && !activity.autoConnecting);
      } else {
        WiFi.currentStatus = WL_CONNECTED;
        activity.checkConnectionStatus();
        assert(finished && resultConnected);
      }
      assert(storageCalls == before);
      activity.onExit();
      assert(resumedAfterPublication);
    }
    const auto log = readLog();
    const auto expectedSource = route == 0 ? 1 : (route == 3 ? 3 : 2);
    const bool automatic = route < 2;
    assert(log.find(",1,0,2," + std::to_string(expectedSource) + "," + std::to_string(automatic) + "\n") != std::string::npos);
    assert(log.find(",1,0,2," + std::to_string(automatic ? 7000 : 15000) + ",2\n") != std::string::npos);
    assert(log.find(",5,4,0\n") != std::string::npos); // Synchronous begin result did not decide the route.
    assert(log.find("private") == std::string::npos && log.find("password") == std::string::npos);
    const unsigned after = storageCalls;
    WiFi.deliver(ARDUINO_EVENT_WIFI_STA_DISCONNECTED, 202);
    assert(storageCalls == after && readLog() == log);
    assert(!wifidiagnostics::stopAndPublish());
    assert(log.find(",7," + std::to_string(route == 1 ? 4 : 1) + ",0\n") != std::string::npos);
    assert(log.find(",8,2,0\n") == std::string::npos); // resume's delivered STOP was outside capture.
  }
  assert(WiFi.registrations == 1);
  std::cout << "wifi production activity consumer assertions passed\n";
}
