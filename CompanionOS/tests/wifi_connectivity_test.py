"""Native WiFi-runtime regression: python CompanionOS/tests/wifi_connectivity_test.py."""

from pathlib import Path
import shutil
import subprocess
import tempfile


HOST = r'''
#include "host_arduino.h"
#include <array>
#include <cassert>
#include <cstdarg>
#include <functional>
#include <memory>
#include <vector>
#define F(s) s
const char* WIFI_SSID = "Stuart";
const char* WIFI_PASS = "configured-one";
const char* WIFI_SSID2 = "P202_4G";
const char* WIFI_PASS2 = "configured-two";
const char* WIFI_PREFERRED_SSID = "P202_4G";
const int EEPROM_SIZE=512, EEPROM_WIFI_MAGIC_ADDR=10;
const int EEPROM_WIFI_SSID_ADDR=16, EEPROM_WIFI_PASS_ADDR=48;
const int NTP_OFFSET=19800, UDP_PORT_RX=8888, UDP_PORT_TX=8889;
const char* NTP_SERVER="test.invalid";
enum { WL_IDLE_STATUS, WL_NO_SSID_AVAIL, WL_SCAN_COMPLETED, WL_CONNECTED,
       WL_CONNECT_FAILED, WL_CONNECTION_LOST, WL_DISCONNECTED };
const int WIFI_STA=1, ARDUINO_EVENT_WIFI_STA_DISCONNECTED=5;
using WiFiEvent_t=int;
struct WiFiEventInfo_t { struct { uint8_t reason=0; } wifi_sta_disconnected; };
struct WiFiEventStationModeDisconnected { uint8_t reason=0; };
using WiFiEventHandler=std::shared_ptr<int>;
struct FakeWiFi {
  int state=WL_DISCONNECTED, modeValue=0, events=0, modeCalls=0;
  bool persistentValue=true, autoReconnect=true, modeSucceeds=true, eventsSucceed=true;
  std::vector<String> attempts;
  std::function<void(WiFiEvent_t, WiFiEventInfo_t)> callback;
  std::function<void(const WiFiEventStationModeDisconnected&)> callback8266;
  void persistent(bool v) { persistentValue=v; }
  bool mode(int v) { assert(!persistentValue); ++modeCalls; if(modeSucceeds) modeValue=v; return modeSucceeds; }
  void setAutoReconnect(bool v) { autoReconnect=v; }
  int onEvent(std::function<void(WiFiEvent_t,WiFiEventInfo_t)> cb,int) {
    if(!eventsSucceed) return 0;
    callback=cb; return ++events;
  }
  WiFiEventHandler onStationModeDisconnected(std::function<void(const WiFiEventStationModeDisconnected&)> cb) {
    if(!eventsSucceed) return nullptr;
    callback8266=cb; ++events; return std::make_shared<int>(1);
  }
  void lost(uint8_t reason=202) {
    if(callback) { WiFiEventInfo_t info; info.wifi_sta_disconnected.reason=reason; callback(5,info); }
    if(callback8266) { WiFiEventStationModeDisconnected info; info.reason=reason; callback8266(info); }
  }
  void disconnect(bool off,bool erase,unsigned long timeout=0) {
    assert(!off&&!erase&&timeout==0); state=WL_DISCONNECTED; lost(8);
  }
  void begin(const char* ssid,const char*) { attempts.emplace_back(ssid); }
  int status() { return state; }
  const char* localIP() { return "192.0.2.1"; }
} WiFi;
struct FakeEEPROM {
  std::array<uint8_t,512> data;
  bool available=true;
  int commits=0;
  FakeEEPROM() { data.fill(255); }
  bool begin(int) { return available; }
  uint8_t read(int a) { assert(a>=0&&a<512); return data[a]; }
  void write(int a,int v) { assert(a>=0&&a<512); data[a]=v; }
  bool commit() { ++commits; return true; }
  void end() {}
} EEPROM;
struct FakeSerial {
  std::string output;
  void print(const char* s) { output+=s; }
  void println(const char* s) { output+=s; output+='\n'; }
  void printf(const char* format,...) {
    char b[256];va_list args;va_start(args,format);vsnprintf(b,sizeof(b),format,args);va_end(args);output+=b;
  }
} Serial;
enum { HTTP_GET,HTTP_POST,UPLOAD_FILE_START,UPLOAD_FILE_WRITE,UPLOAD_FILE_END,UPLOAD_FILE_ABORTED };
struct HTTPUpload { int status=0;size_t totalSize=0,currentSize=0;uint8_t* buf=nullptr; };
struct FakeServer {
  int routes=0,starts=0,stops=0;
  HTTPUpload uploadData;
  template<class... T> void on(const char*,int,T...) { ++routes; }
  void send(int,const char*,const char*) {}
  HTTPUpload& upload() { return uploadData; }
  void begin() { ++starts; }
  void stop() { ++stops; }
} sysWebServer;
struct FakeUDP {
  int starts=0,stops=0,hellos=0,port=0;
  void begin(int p) { ++starts;port=p; }
  void stop() { ++stops; }
  void beginPacket(const char*,int p) { assert(p==UDP_PORT_TX);++hellos; }
  void write(const uint8_t*,size_t) {}
  void endPacket() {}
} udp,udpPcDash;
int ntpStarts=0;
void configTime(int,int,const char*) { ++ntpStarts; }
struct FakeTimeClient { void begin() { ++ntpStarts; } } timeClient;
bool prepareCustomEyeImage() { return false; }
bool wifiConnected=false,dhNetPaused=false,pcFound=true,pcDashListening=false;
unsigned long lastDiscoveryShout=0;
bool customEyeReady=false,customEyeActive=false,memoryUploadValid=false;
uint16_t* customEyeImg=nullptr;
const char* MEMORIES_HTML="test";
const int STATE_EYES=0;
int currentState=1;
void renderCurrentPage() { assert(false&&"WiFi must not draw UI"); }
bool memoryAvailable=true;
int memoryPrepares=0;
bool prepareWiFiMemory() { ++memoryPrepares; return memoryAvailable; }
#define COMPANION_REMOTE_SCREEN_H
void registerRemoteScreenRoutes() {}
#define COMPANION_PLANNER_STORE_H
namespace companion_planner { void registerRoutes() {} }
'''

TESTS = r'''
static void saved(std::initializer_list<std::pair<String,String>> profiles) {
  EEPROM.data.fill(255);
  EEPROM.write(10,0xC0);EEPROM.write(11,0x56);EEPROM.write(12,profiles.size());
  int addr=16;
  for(const auto& p:profiles) {
    for(int i=0;i<32;i++) EEPROM.write(addr++,i<(int)p.first.length()?p.first[i]:0);
    for(int i=0;i<64;i++) EEPROM.write(addr++,i<(int)p.second.length()?p.second[i]:0);
  }
}
int main() {
  companion_startup::gate.firstFrame(0);
  saved({{"P202_4G","stale-secret"},{"Stuart","another-stale"},{"Third","third-secret"},{"Fourth",""}});
  loadWiFiCredentials();
  assert(wifiProfileCount==4);
  assert(wifiProfiles[0].ssid=="P202_4G"&&wifiProfiles[0].pass==WIFI_PASS2);
  assert(wifiProfiles[1].ssid=="Stuart"&&wifiProfiles[1].pass==WIFI_PASS);
  assert(wifiProfiles[2].ssid=="Third"&&wifiProfiles[3].ssid=="Fourth");
  assert(EEPROM.commits==1&&savedWiFiConfigMatches(wifiConfigFingerprint()));
  loadWiFiCredentials();assert(EEPROM.commits==1); // Migration is one-shot.
  assert(!validWiFiProfile("","")&&!validWiFiProfile(String(std::string(33,'s')), ""));
  assert(!validWiFiProfile("ssid","short")&&!validWiFiProfile("ssid","line\nbreak"));
  assert(validWiFiProfile("open","")&&validWiFiProfile("psk",String(std::string(64,'a'))));
  assert(!validWiFiProfile("psk",String(std::string(64,'z'))));
  assert(!validWiFiProfile("bad\nssid","password"));
  hostMillis=0;setupWiFi();
  assert(hostMillis==0&&!wifiConnected&&WiFi.attempts.size()==1);
  assert(WiFi.attempts.back()=="P202_4G"&&!WiFi.autoReconnect&&!WiFi.persistentValue);
  assert(sysWebServer.starts==0&&ntpStarts==0&&udp.starts==0);
  WiFi.state=WL_CONNECT_FAILED;
  hostMillis=11999;tickWiFiConnectivity();assert(WiFi.attempts.size()==1);
  for(int i=1;i<=4;i++) {
    hostMillis=i*12000;tickWiFiConnectivity();
    assert(WiFi.attempts.size()==(size_t)min(i+1,4));
  }
  assert(!wifiAttemptActive&&wifiCycleFinishedAt==48000);
  hostMillis=167999;tickWiFiConnectivity();assert(WiFi.attempts.size()==4);
  hostMillis=168000;tickWiFiConnectivity();assert(WiFi.attempts.size()==5);
  assert(WiFi.attempts.back()=="P202_4G");
  WiFi.state=WL_CONNECTED;tickWiFiConnectivity();
  assert(wifiConnected&&!wifiServicesStarted);
  hostMillis+=1499;tickWiFiConnectivity();assert(!wifiServicesStarted);
  hostMillis+=1;tickWiFiConnectivity();
  assert(wifiConnected&&wifiServicesStarted&&!wifiAttemptActive);
  assert(sysWebServer.routes==3&&sysWebServer.starts==1&&ntpStarts==1);
  assert(udp.starts==1&&udp.port==8888&&udpPcDash.starts==0);
  assert(!pcFound&&udp.hellos==1);
  tickWiFiConnectivity();assert(sysWebServer.starts==1);
  WiFi.state=WL_CONNECTION_LOST;WiFi.lost();tickWiFiConnectivity();
  assert(!wifiConnected&&wifiAttemptActive&&!wifiServicesStarted);
  assert(sysWebServer.stops==1&&udp.stops==1&&udpPcDash.stops==1);
  WiFi.state=WL_CONNECTED;tickWiFiConnectivity();
  assert(!wifiServicesStarted);
  hostMillis+=1500;tickWiFiConnectivity();
  assert(sysWebServer.routes==3&&sysWebServer.starts==2&&ntpStarts==2&&udp.starts==2);
  // A full link flap between polls must also rebind services.
  WiFi.lost();tickWiFiConnectivity();assert(!wifiServicesStarted);
  hostMillis+=1500;tickWiFiConnectivity();assert(sysWebServer.starts==3&&ntpStarts==3);
  dhNetPaused=true;WiFi.state=WL_DISCONNECTED;
  size_t attempts=WiFi.attempts.size();hostMillis+=30000;tickWiFiConnectivity();
  assert(WiFi.attempts.size()==attempts);
  dhNetPaused=false;tickWiFiConnectivity();assert(WiFi.attempts.size()==attempts+1);
  hostMillis=UINT32_MAX-1000;setupWiFi();attempts=WiFi.attempts.size();
  hostMillis+=11999;tickWiFiConnectivity();assert(WiFi.attempts.size()==attempts);
  hostMillis+=1;tickWiFiConnectivity();assert(WiFi.attempts.size()==attempts+1);
  // EEPROM unavailable, corrupt records and duplicate SSIDs cannot remove config.
  EEPROM.available=false;loadWiFiCredentials();assert(wifiProfileCount==2);
  EEPROM.available=true;saved({{"Extra","short"},{"Extra","valid-pass"},{"Extra","newer-pass"},{"Other","other-pass"}});
  loadWiFiCredentials();assert(wifiProfileCount==4&&wifiProfiles[2].ssid=="Extra");
  int beforeSave=EEPROM.commits;
  saveMultiWiFiCredentials({{"Extra","valid-pass"},{"Extra","updated-pass"},{"Invalid","short"}});
  assert(EEPROM.commits==beforeSave+1&&EEPROM.read(12)==1);
  assert(wifiProfileCount==3&&wifiProfiles[2].pass=="updated-pass");
  int commits=EEPROM.commits;saveWiFiCredentials("Bad","short");assert(EEPROM.commits==commits);
  // Legacy single-profile storage still merges and updates configured passwords.
  EEPROM.data.fill(0);EEPROM.write(10,0xC0);EEPROM.write(11,0x55);
  const char* s="P202_4G";const char* p="old-password";
  for(size_t i=0;i<strlen(s);i++) EEPROM.write(16+i,s[i]);
  for(size_t i=0;i<strlen(p);i++) EEPROM.write(48+i,p[i]);
  loadWiFiCredentials();assert(wifiProfileCount==2&&wifiProfiles[0].pass==WIFI_PASS2);
  WIFI_PREFERRED_SSID="Stuart";loadWiFiCredentials();assert(wifiProfiles[0].ssid=="Stuart");
  WIFI_PREFERRED_SSID="Unknown";loadWiFiCredentials();assert(wifiProfiles[0].ssid=="Stuart");
  // Deliberate runtime edits to configured SSIDs must survive a fresh load/reboot.
  WIFI_PREFERRED_SSID="P202_4G";
  beforeSave=EEPROM.commits;
  saveMultiWiFiCredentials({{"P202_4G","user-new-secret"},{"Stuart","user-stuart-secret"},
                            {"Extra","extra-password"},{"Guest",""}});
  assert(EEPROM.commits==beforeSave+1);
  assert(wifiProfileCount==4&&wifiProfiles[0].pass=="user-new-secret");
  assert(wifiProfiles[1].pass=="user-stuart-secret");
  for(int reboot=0;reboot<3;++reboot) {
    wifiProfileCount=0;wifiProfiles[0]={};loadWiFiCredentials();
    assert(wifiProfileCount==4&&wifiProfiles[0].ssid=="P202_4G");
    assert(wifiProfiles[0].pass=="user-new-secret"&&wifiProfiles[1].pass=="user-stuart-secret");
    assert(wifiProfiles[2].ssid=="Extra"&&wifiProfiles[3].ssid=="Guest"&&wifiProfiles[3].pass=="");
    assert(EEPROM.commits==beforeSave+1);
  }
  // A new compiled config overrides old saved values exactly once, keeping other profiles.
  WIFI_PASS2="compiled-next-secret";
  beforeSave=EEPROM.commits;loadWiFiCredentials();
  assert(EEPROM.commits==beforeSave+1&&wifiProfiles[0].pass==WIFI_PASS2);
  assert(wifiProfileCount==4&&wifiProfiles[2].ssid=="Extra"&&wifiProfiles[3].ssid=="Guest");
  loadWiFiCredentials();assert(EEPROM.commits==beforeSave+1&&wifiProfiles[0].pass==WIFI_PASS2);
  beforeSave=EEPROM.commits;saveWiFiCredentials("P202_4G","single-save-secret");
  assert(EEPROM.commits==beforeSave+1&&wifiProfiles[0].pass=="single-save-secret");
  loadWiFiCredentials();assert(EEPROM.commits==beforeSave+1&&wifiProfiles[0].pass=="single-save-secret");
  // Explicitly changing a configured network to open is also durable.
  beforeSave=EEPROM.commits;saveWiFiCredentials("P202_4G","");
  assert(EEPROM.commits==beforeSave+1&&wifiProfiles[0].pass=="");
  loadWiFiCredentials();assert(EEPROM.commits==beforeSave+1&&wifiProfiles[0].pass=="");
  WIFI_SSID="";WIFI_SSID2="";EEPROM.data.fill(255);
  setupWiFi();assert(wifiProfileCount==0&&!wifiAttemptActive);
  attempts=WiFi.attempts.size();hostMillis+=120000;tickWiFiConnectivity();assert(WiFi.attempts.size()==attempts);
  assert(WiFi.events==1);
  // A passive scan owns the radio briefly, without erasing profiles or racing association.
  WIFI_SSID="P202_4G";WIFI_PASS="diagnostic-test-password";setupWiFi();
  attempts=WiFi.attempts.size();int scanCommits=EEPROM.commits;
  assert(beginWiFiDiagnosticScan()&&!beginWiFiDiagnosticScan());
  hostMillis+=20000;tickWiFiConnectivity();
  assert(WiFi.attempts.size()==attempts&&!wifiAttemptActive&&EEPROM.commits==scanCommits);
  endWiFiDiagnosticScan();tickWiFiConnectivity();assert(WiFi.attempts.size()==attempts+1);
  WiFi.state=WL_CONNECTED;tickWiFiConnectivity();
  assert(beginWiFiDiagnosticScan());tickWiFiConnectivity();assert(WiFi.state==WL_CONNECTED);
  endWiFiDiagnosticScan();dhNetPaused=true;assert(!beginWiFiDiagnosticScan());dhNetPaused=false;
  // A failed driver must not be followed by association or a retry storm.
  WiFi.state=WL_DISCONNECTED;WiFi.modeSucceeds=false;
  attempts=WiFi.attempts.size();setupWiFi();int modes=WiFi.modeCalls;
  assert(!wifiDriverReady&&!wifiAttemptActive&&!wifiServicesStarted&&!beginWiFiDiagnosticScan());
  assert(WiFi.attempts.size()==attempts);
  hostMillis+=119999;tickWiFiConnectivity();assert(WiFi.modeCalls==modes);
  WiFi.modeSucceeds=true;hostMillis+=1;tickWiFiConnectivity();
  assert(wifiDriverReady&&WiFi.modeCalls==modes+1&&WiFi.attempts.size()==attempts+1);
  // Failed callback allocation also stays offline and recovers without reboot.
  wifiEventsRegistered=false;WiFi.eventsSucceed=false;
  attempts=WiFi.attempts.size();setupWiFi();assert(!wifiDriverReady&&WiFi.attempts.size()==attempts);
  WiFi.eventsSucceed=true;hostMillis+=120000;tickWiFiConnectivity();
  assert(wifiDriverReady&&WiFi.attempts.size()==attempts+1);
  #ifdef ESP32
  // Reserve failure happens before driver entry; backoff also works at rollover.
  hostMillis=UINT32_MAX-1000;memoryAvailable=false;
  attempts=WiFi.attempts.size();modes=WiFi.modeCalls;setupWiFi();
  assert(!wifiDriverReady&&WiFi.modeCalls==modes&&WiFi.attempts.size()==attempts);
  hostMillis+=119999;tickWiFiConnectivity();assert(WiFi.modeCalls==modes);
  memoryAvailable=true;hostMillis+=1;tickWiFiConnectivity();
  assert(wifiDriverReady&&WiFi.modeCalls==modes+1&&WiFi.attempts.size()==attempts+1&&memoryPrepares>0);
  #endif
  assert(Serial.output.find("configured-one")==std::string::npos);
  assert(Serial.output.find("configured-two")==std::string::npos);
  assert(Serial.output.find("stale-secret")==std::string::npos);
  assert(Serial.output.find("user-new-secret")==std::string::npos);
  assert(Serial.output.find("compiled-next-secret")==std::string::npos);
  puts("PASS WiFi: credentials, reconnect, failed driver/events/RAM, 120s backoff, services, pause, rollover, no secret logs");
}
'''


def main():
    tests = Path(__file__).resolve().parent
    runtime = (tests.parent / "arduino/CompanionOS_Main/wifi_connectivity.h").read_text(encoding="utf-8")
    sketch = (tests.parent / "arduino/CompanionOS_Main/CompanionOS_Main.ino").read_text(encoding="utf-8")
    setup = sketch[sketch.index("void setup() {"):sketch.index("void loop() {")]
    assert setup.index("setupWiFi();") < setup.index("runBootSequence();"), "WiFi must reserve RAM before graphics startup"
    assert "setupWiFi();" not in sketch[sketch.index("void loop() {"):], "Do not reinitialize WiFi after the first rendered page"
    for forbidden in ("delay(", "timeClient.update(", "setupBluetooth(", "tft.", "setupCSICollector("):
        assert forbidden not in runtime, forbidden
    compiler = shutil.which("g++")
    if not compiler:
        raise SystemExit("g++ is required for native tests")
    with tempfile.TemporaryDirectory(prefix="companion-wifi-") as directory:
        for platform in ("ESP32", "ESP8266"):
            executable = Path(directory) / f"wifi_test_{platform}.exe"
            subprocess.run(
                [compiler, "-std=c++11", "-Wall", "-Wextra", "-Werror", f"-D{platform}=1", "-I", str(tests),
                 "-I", str(tests.parent / "arduino/CompanionOS_Main"),
                 "-x", "c++", "-", "-o", str(executable)],
                input=HOST + runtime + TESTS, text=True, encoding="utf-8", check=True,
            )
            print(f"{platform} native runtime:", flush=True)
            subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
