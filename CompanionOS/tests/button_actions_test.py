"""Host regression for real virtual/physical handlers; no device or RF operations.

Run with Python when CLI execution is permitted. Requires g++ (MinGW supported).
The virtual function is extracted from the current firmware, not reimplemented.
"""

from pathlib import Path
import os
import shutil
import subprocess
import tempfile


HOST = r'''
#include "host_arduino.h"
#include <cassert>
#include <cctype>
#include <vector>

// Extend the existing test String locally; shared test scaffolding stays intact.
class ButtonString : public String {
 public:
  using String::String;
  void toUpperCase() {
    std::string text(c_str());
    for (char& c : text) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    static_cast<String&>(*this) = String(text);
  }
};
#define String ButtonString
#define GLOBALS_H
constexpr int BTN_LEFT=1, BTN_RIGHT=2, BTN_SELECT=3;
constexpr int HIGH=1, BTN_ACTIVE_LEVEL=0, BTN_PIN_MODE=2;
constexpr int THEME_COUNT=3, EEPROM_SIZE=512, EEPROM_ACTIVE_THEME_ADDR=2;
void pinMode(int, int) {}
int digitalRead(int) { return HIGH; }

AppState currentState=STATE_EYES;
Emotion currentEmotion=EMO_NEUTRAL;
// Real DrHack enum and passive-page predicate are injected by the runner.
DrHackSubState dhCurrentState=DH_MENU;
bool virtualHomePressed=false, virtualLeftPressed=false, virtualRightPressed=false;
bool virtualUpPressed=false, virtualDownPressed=false, virtualSelectPressed=false;
bool customEyeActive=false, pomoActive=false, flashNotifEnabled=true, t2_initialized=true;
unsigned long lastInteractionTime=0, virtualSelectHoldUntil=0;
int activeTheme=0, settingsScrollY=0, renders=0, settingsDraws=0, notificationDraws=0;
int classicResets=0, softResets=0, subpages=0, expressionTheme=-1, emotionTheme=-1;
int rfCalls=0;
std::vector<int> pageDeltas;
std::vector<std::string> commands;
std::vector<std::string> order;
struct FakeEEPROM {
  int savedTheme=-1, commits=0, begins=0, ends=0;
  bool begin(int size) { assert(size==EEPROM_SIZE); ++begins; return true; }
  void write(int address, int value) { assert(address==EEPROM_ACTIVE_THEME_ADDR); savedTheme=value; }
  bool commit() { ++commits; return true; }
  void end() { ++ends; }
} EEPROM;
void changePage(int delta) {
  pageDeltas.push_back(delta);
  currentState=static_cast<AppState>((int(currentState)+delta+STATE_COUNT)%STATE_COUNT);
}
void renderCurrentPage() { ++renders; order.push_back("render"); }
void redrawSettingsPartial() { ++settingsDraws; }
void redrawNotificationsPartial() { ++notificationDraws; }
void nextPcDashSubPage() { ++subpages; }
void resetSpotifyDrawState() { ++classicResets; order.push_back("classic"); }
void t2_resetSpotifyDrawState() { ++softResets; order.push_back("soft"); }
void setEmotion(Emotion e) { currentEmotion=e; emotionTheme=0; }
void t2_setEmotion(Emotion e) { currentEmotion=e; emotionTheme=1; }
void t3_setEmotion(Emotion e) { currentEmotion=e; emotionTheme=2; }
void t2_nextExpression() { expressionTheme=1; }
void t3_nextExpression() { expressionTheme=2; }
void sendCommand(String text) { commands.push_back(text.c_str()); }
void dhNavigate(int) { ++rfCalls; }
void dhSelect() { ++rfCalls; }

#include "buttons.h"
#include "virtual_button.inc"

void reset(AppState page) {
  currentState=page; currentEmotion=EMO_NEUTRAL; activeTheme=0;
  virtualHomePressed=virtualLeftPressed=virtualRightPressed=false;
  virtualUpPressed=virtualDownPressed=virtualSelectPressed=false;
  customEyeActive=pomoActive=false; flashNotifEnabled=t2_initialized=true;
  virtualSelectHoldUntil=lastInteractionTime=0; hostMillis=10000;
  settingsScrollY=renders=settingsDraws=notificationDraws=0;
  classicResets=softResets=subpages=rfCalls=0;
  expressionTheme=emotionTheme=-1;
  pageDeltas.clear(); commands.clear(); order.clear(); EEPROM=FakeEEPROM();
#ifdef ESP32
  btnLeft=ButtonState{BTN_LEFT,false,false,false,false,0,false,false};
  btnRight=ButtonState{BTN_RIGHT,false,false,false,false,0,false,false};
  btnSelect=ButtonState{BTN_SELECT,false,false,false,false,0,false,false};
#endif
}
void invoke(const char* command) {
  handleVirtualButton(command);
  assert(lastInteractionTime==millis());
  assert(rfCalls==0);
}
void expectCommand(const char* command) {
  assert(commands.size()==1 && commands.front()==command);
  assert(pageDeltas.empty());
}
void expectSettings() {
  assert(activeTheme==1 && EEPROM.savedTheme==1);
  assert(EEPROM.begins==1 && EEPROM.commits==1 && EEPROM.ends==1);
  assert(classicResets==1 && softResets==1 && !t2_initialized && renders==1);
  assert((order==std::vector<std::string>{"classic","soft","render"}));
}

int main() {
  // General long navigation; intentionally exclude the unchanged RF page.
  for (int i=0; i<STATE_COUNT; ++i) {
    if (i==STATE_DR_HACK || i==STATE_SPOTIFY || i==STATE_LYRICS) continue;
    reset(static_cast<AppState>(i)); invoke("LEFT_LONG");
    assert(currentState==STATE_EYES && commands.empty());
    assert(pageDeltas.size()==static_cast<unsigned>(i!=STATE_EYES));
    reset(static_cast<AppState>(i)); invoke("RIGHT_LONG");
    assert(currentState==STATE_SETTINGS && commands.empty());
    assert(pageDeltas.size()==static_cast<unsigned>(i!=STATE_SETTINGS));
    if (i!=STATE_EYES && i!=STATE_POMODORO) {
      reset(static_cast<AppState>(i)); invoke("SELECT_LONG");
      assert(currentState==STATE_EYES && commands.empty());
    }
  }
  for (const auto& item : std::vector<std::pair<const char*,const char*>>{
       {"LEFT_LONG","PREV"},{"RIGHT_LONG","NEXT"},{"SELECT_LONG","SHUFFLE:TOGGLE"},
       {"SELECT","PLAY_PAUSE"}}) {
    reset(STATE_SPOTIFY); invoke(item.first); expectCommand(item.second);
    reset(STATE_LYRICS); invoke(item.first); expectCommand(item.second);
  }
  reset(STATE_POMODORO); invoke("SELECT_LONG"); expectCommand("POMO:RESET");
  for (bool active : {false,true}) {
    reset(STATE_POMODORO); pomoActive=active; invoke("SELECT");
    expectCommand(active ? "POMO:PAUSE" : "POMO:START");
  }
  for (int theme=0; theme<THEME_COUNT; ++theme) {
    reset(STATE_EYES); activeTheme=theme; currentEmotion=static_cast<Emotion>(EMO_COUNT-1);
    invoke("SELECT_LONG"); assert(currentEmotion==EMO_NEUTRAL && emotionTheme==theme);
    reset(STATE_EYES); activeTheme=theme; customEyeActive=true; invoke("SELECT");
    assert(!customEyeActive && renders==1 && emotionTheme==-1 && expressionTheme==-1);
    reset(STATE_EYES); activeTheme=theme; invoke("SELECT");
    assert(theme==0 ? emotionTheme==0 : expressionTheme==theme);
  }
  reset(STATE_PC_DASH); invoke(" select ");
  assert(subpages==1 && pageDeltas.empty() && renders==0);
  reset(STATE_SETTINGS); invoke("SELECT"); expectSettings();
  reset(STATE_NOTIFICATIONS); invoke("SELECT");
  assert(!flashNotifEnabled && notificationDraws==1);
  invoke("SELECT"); assert(flashNotifEnabled && notificationDraws==2);

  reset(STATE_SETTINGS); settingsScrollY=10; invoke("UP");
  assert(settingsScrollY==0 && settingsDraws==1 && renders==0);
  assert(virtualUpPressed && !virtualLeftPressed && !virtualRightPressed && !virtualDownPressed);
  reset(STATE_SETTINGS); settingsScrollY=110; invoke("DOWN");
  assert(settingsScrollY==120 && settingsDraws==1 && renders==0);
  assert(virtualDownPressed && !virtualLeftPressed && !virtualRightPressed && !virtualUpPressed);
  reset(STATE_WEATHER); invoke("UP");
  assert(virtualUpPressed && !virtualLeftPressed && pageDeltas==std::vector<int>{-1});
  reset(STATE_WEATHER); invoke("DOWN");
  assert(virtualDownPressed && !virtualRightPressed && pageDeltas==std::vector<int>{1});
  reset(STATE_WEATHER); invoke("LEFT"); assert(virtualLeftPressed && !virtualUpPressed);
  reset(STATE_WEATHER); invoke("RIGHT"); assert(virtualRightPressed && !virtualDownPressed);
  reset(STATE_SETTINGS); invoke("HOME"); assert(currentState==STATE_EYES && virtualHomePressed);
  reset(STATE_WEATHER); handleVirtualButton("UNKNOWN");
  assert(currentState==STATE_WEATHER && lastInteractionTime==0 && commands.empty());

#ifdef ESP32
  // Only passive, page-driven diagnostics use normal navigation/select routing.
  for(DrHackSubState state : {DH_WIFI_SCANNER,DH_BT_SCANNER,DH_INFO,DH_HW_DIAG,DH_ABOUT,DH_WIFI_CONFIG}) {
    for(const char* input : {"LEFT","RIGHT","SELECT","SELECT_LONG"}) {
      reset(STATE_DR_HACK);dhCurrentState=state;handleVirtualButton(input);
      assert(rfCalls==1 && pageDeltas.empty() && !virtualSelectPressed && !virtualSelectHoldUntil);
      assert(!virtualLeftPressed && !virtualRightPressed);
    }
    reset(STATE_DR_HACK);dhCurrentState=state;btnSelect.wasPressed=true;handleButtons();assert(rfCalls==1);
  }
  reset(STATE_DR_HACK);dhCurrentState=DH_MENU;handleVirtualButton("SELECT_LONG");
  assert(currentState==STATE_EYES && !rfCalls && !virtualSelectHoldUntil);
  reset(STATE_DR_HACK);dhCurrentState=DH_BLE_INSPECTOR;handleVirtualButton("SELECT_LONG");
  assert(dhCurrentState==DH_MENU && renders==1 && !rfCalls && !virtualSelectHoldUntil);
  reset(STATE_DR_HACK);dhCurrentState=DH_BLE_INSPECTOR;handleVirtualButton("SELECT");
  assert(rfCalls==1 && !virtualSelectPressed);
  // Execute the real physical SELECT path, not a copied branch.
  for (bool enabled : {false,true}) {
    reset(STATE_NOTIFICATIONS); flashNotifEnabled=enabled; btnSelect.wasPressed=true;
    handleButtons();
    assert(flashNotifEnabled!=enabled && notificationDraws==1);
    assert(lastInteractionTime==millis() && commands.empty() && pageDeltas.empty());
    handleButtons(); assert(notificationDraws==1); // Press consumed exactly once.
  }
  reset(STATE_SETTINGS); btnSelect.wasPressed=true; handleButtons(); expectSettings();
  reset(STATE_PC_DASH); btnSelect.wasPressed=true; handleButtons(); assert(subpages==1);
  reset(STATE_LYRICS); btnSelect.wasPressed=true; handleButtons(); expectCommand("PLAY_PAUSE");
  reset(STATE_LYRICS); btnRight.wasPressed=true; handleButtons(); assert(currentState==STATE_POMODORO);
  reset(STATE_LYRICS); btnLeft.wasPressed=true; handleButtons(); assert(currentState==STATE_SPOTIFY);
  for (AppState page : {STATE_SPOTIFY, STATE_LYRICS}) {
    reset(page); btnLeft.longFired=true; handleButtons(); expectCommand("PREV");
    reset(page); btnRight.longFired=true; handleButtons(); expectCommand("NEXT");
    reset(page); btnSelect.longFired=true; handleButtons(); expectCommand("SHUFFLE:TOGGLE");
  }
  for (int theme=0; theme<THEME_COUNT; ++theme) {
    reset(STATE_EYES); activeTheme=theme; customEyeActive=true;
    btnSelect.wasPressed=true; handleButtons();
    assert(!customEyeActive && renders==1 && expressionTheme==-1 && emotionTheme==-1);
  }
#endif
  puts("PASS actual virtual button actions and physical SELECT parity");
}
'''


def extract(text, signature, terminator):
    start = text.index(signature)
    return text[start:text.index(terminator, start) + len(terminator)]


def main():
    tests = Path(__file__).resolve().parent
    firmware = tests.parent / "arduino/CompanionOS_Main"
    network = (firmware / "companion_net.h").read_text(encoding="utf-8")
    globals_text = (firmware / "globals.h").read_text(encoding="utf-8")
    handler = extract(network, "void handleVirtualButton(String btn) {", "\n}")
    enums = "\n".join(extract(globals_text, signature, "};")
                      for signature in ("enum AppState {", "enum Emotion {", "enum DrHackSubState {"))
    enums += '\n' + extract(globals_text, 'inline bool dhUsesPageInput(', '\n}')
    assert handler.count('if (btn == "UP")') == 1
    assert handler.count('if (btn == "DOWN")') == 1
    compiler = shutil.which("g++")
    if not compiler and Path("C:/mingw/bin/g++.exe").is_file():
        compiler = "C:/mingw/bin/g++.exe"
    if not compiler:
        raise SystemExit("g++ is required for native tests")
    env = dict(os.environ)
    env["PATH"] = str(Path(compiler).parent) + os.pathsep + env.get("PATH", "")
    with tempfile.TemporaryDirectory(prefix="companion-buttons-") as directory:
        directory = Path(directory)
        (directory / "virtual_button.inc").write_text(handler, encoding="utf-8")
        source = HOST.replace("AppState currentState=", enums + "\nAppState currentState=", 1)
        for platform in ("esp32", "common"):
            executable = directory / ("button_actions_" + platform + ".exe")
            defines = ["-DESP32"] if platform == "esp32" else []
            subprocess.run(
                [compiler, "-std=c++14", "-Wall", "-Wextra", "-Werror", *defines,
                 "-I", str(tests), "-I", str(firmware), "-I", str(directory),
                 "-x", "c++", "-", "-o", str(executable)],
                input=source, text=True, encoding="utf-8", env=env, check=True,
            )
            subprocess.run([str(executable)], env=env, check=True)


if __name__ == "__main__":
    main()
