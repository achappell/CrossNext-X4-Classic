#pragma once
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <ctime>
#include <functional>
#include <memory>
#include <string>
#include <variant>
#include <vector>
#include <Arduino.h>
#include <HalStorage.h>
#include <WiFi.h>
#include "WifiDiagnostics.h"

// Only environmental/UI dependencies are substituted. The activity declaration
// and each tested method definition come unchanged from production.
class GfxRenderer {};
class MappedInputManager {};
struct KeyboardResult { std::string text; };
struct WifiResult { bool connected; std::string ssid, ip; };
struct ActivityResult { bool isCancelled = false; std::variant<KeyboardResult, WifiResult> data; };
extern std::string keyboardText;
extern bool finished, resultConnected, resumedAfterPublication;
class Activity {
 public:
  GfxRenderer& renderer;
  MappedInputManager& mappedInput;
  Activity(const char*, GfxRenderer& r, MappedInputManager& i) : renderer(r), mappedInput(i) {}
  virtual ~Activity() = default;
  struct RenderLock { explicit RenderLock(Activity&) {} };
  virtual void onEnter() {}
  virtual void onExit() {}
  virtual void loop() {}
  virtual void render(RenderLock&&) {}
  virtual bool preventAutoSleep() { return false; }
  void requestUpdate() {}
  void setResult(ActivityResult result) { resultConnected = !result.isCancelled; }
  void finish() { finished = true; }
  template<class T, class F> void startActivityForResult(std::unique_ptr<T>, F callback) {
    ActivityResult result; result.data = KeyboardResult{keyboardText}; callback(result);
  }
};
namespace freeink { namespace ui {
using ActionId = int;
struct ActionEvent { int value = 0; bool longPress = false; };
struct ButtonProps {};
struct ListItem {};
struct ListNav { void reset() {} };
} }
struct UiScreen {};
class UiAppHost {
 public:
  struct App {
    void clearTapFlash() {}
    template<class... T> void on(T...) {}
    template<class... T> void setScreen(T...) {}
  } app;
  explicit UiAppHost(GfxRenderer&) {}
  void resetUi() {}
  void closeRouting() {}
};
struct ButtonNavigator {};
#include "activities/network/WifiSelectionActivity.h"
namespace fui = freeink::ui;
constexpr fui::ActionId ACTION_ROW = 1, ACTION_SCAN = 2, ACTION_PROMPT = 3, ACTION_REFERENCE = 4;
enum ReferenceAction { Toggle, Current, EditPassword, RevealPassword, Connect, Cancel, Disconnect, Forget };
#define LOG_DBG(...) ((void)0)
#define LOG_ERR(...) ((void)0)
inline bool clockPredatesBuild() { return false; }
enum { STR_MAC_ADDRESS, STR_ENTER_WIFI_PASSWORD, STR_ENTER_WIFI_SSID, STR_ERROR_GENERAL_FAILURE,
       STR_ERROR_NETWORK_NOT_FOUND, STR_ERROR_CONNECTION_TIMEOUT };
inline const char* tr(int) { return "environment"; }
struct WifiCredential { std::string ssid, password; };
struct CredentialStore {
  std::vector<WifiCredential> credentials;
  std::string last;
  void loadFromFile() {}
  size_t getCredentialCount() const { return credentials.size(); }
  std::string getLastConnectedSsid() const { return last; }
  const WifiCredential* findCredential(const std::string& ssid) {
    for (const auto& c : credentials) if (c.ssid == ssid) return &c;
    return nullptr;
  }
  void setLastConnectedSsid(const std::string& ssid) { last = ssid; }
};
extern CredentialStore WIFI_STORE;
namespace devmode {
inline void pause() {}
inline void setRadioAllowed(bool) {}
inline void resume() {
  resumedAfterPublication = std::filesystem::exists(fsRoot + "/.crosspoint/classic-wifi-diagnostic.log");
  WiFi.deliver(ARDUINO_EVENT_WIFI_STA_STOP);
}
}
struct Clock { bool isAvailable() { return false; } bool syncFromNTP() { return false; } };
extern Clock halClock;
struct Settings { bool clockHasBeenSynced = false; void saveToFile() {} };
extern Settings SETTINGS;
namespace trustedtime { inline void startSync() {} }
namespace pluginevents { inline void drain(GfxRenderer*) {} }
struct Esp { unsigned getFreeHeap() { return 0; } };
extern Esp ESP;
using esp_err_t = int;
constexpr int ESP_OK = 0, ESP_MAC_WIFI_STA = 0;
inline int esp_read_mac(uint8_t*, int) { return 1; }
enum class InputType { Text, Password };
struct KeyboardEntryActivity { template<class... T> explicit KeyboardEntryActivity(T&&...) {} };
template<class T, class... Args> std::unique_ptr<T> makeUniqueNoThrow(Args&&... args) {
  return std::make_unique<T>(std::forward<Args>(args)...);
}
