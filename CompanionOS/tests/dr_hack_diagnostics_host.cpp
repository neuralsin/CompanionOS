#define COMPANION_HOST_MEMORY
#include "host_globals.h"
#include <cassert>
#include <iostream>
#include <functional>
using portMUX_TYPE = int;
constexpr int portMUX_INITIALIZER_UNLOCKED = 0;
void portENTER_CRITICAL(int* lock) { assert(!*lock);*lock=1; }
void portEXIT_CRITICAL(int* lock) { assert(*lock);*lock=0; }
struct { template<class... T> void printf(const char*,T...) {} } Serial;
constexpr int WIFI_SCAN_RUNNING=-1,WIFI_SCAN_FAILED=-2;

#undef SCALE_X
#undef SCALE_Y
#undef SCALE_MIN
#define SCALE_X(v) int((v) * (float(SCREEN_W) / 160.0f) + 0.5f)
#define SCALE_Y(v) int((v) * (float(SCREEN_H) / 128.0f) + 0.5f)
#define SCALE_MIN(v) max(1, SCALE_X(v))
namespace companion_memory { Snapshot testSnapshot{256*1024,200*1024}; }
bool artDrawn=false;
constexpr int BTN_SELECT=27,BTN_UNPRESSED_LEVEL=1,BTN_ACTIVE_LEVEL=0;
constexpr int BTN_LEFT=13,BTN_RIGHT=14,RF24_2MBPS=2,RF24_PA_MIN=0;
bool virtualLeftPressed=false,virtualRightPressed=false;
unsigned long virtualSelectHoldUntil=0;
constexpr int HIGH=1,LOW=0,INPUT=0,OUTPUT=1,WIFI_STA=1;
constexpr int TFT_CS=5,NRF1_CSN_PIN=33,NRF2_CSN_PIN=16,NRF1_CE_PIN=32,NRF2_CE_PIN=17;
constexpr int CC1101_CSN_PIN=21,IR_RX_PIN=34,IR_TX_PIN=26,CC1101_GDO0_PIN=35;
constexpr int WIFI_AUTH_OPEN=0,WIFI_AUTH_WEP=1,WIFI_AUTH_WPA_PSK=2,WIFI_AUTH_WPA2_PSK=3;
constexpr int WIFI_AUTH_WPA_WPA2_PSK=4,WIFI_AUTH_WPA3_PSK=5,WIFI_AUTH_WPA2_WPA3_PSK=6;
constexpr int CC_PARTNUM=0x30,CC_VERSION=0x31;
const char* PLATFORM_NAME="ESP32 test board";
int pins[64]={},pinModes[64]={};
int digitalRead(int pin) { return pins[pin]; }
void digitalWrite(int pin,int level) { pins[pin]=level; }
void pinMode(int pin,int mode) { pinModes[pin]=mode; }
void delayMicroseconds(unsigned us) { static unsigned pending=0;pending+=us;hostMillis+=pending/1000;pending%=1000; }
std::function<void()> networkStep=[]{};
void diagnosticNetwork() { networkStep(); }
#define handleNetwork diagnosticNetwork

struct DiagnosticESP {
  uint64_t getEfuseMac() { return 0x123456789abcULL; }
  int getCpuFreqMHz() { return 240; }
  unsigned getFreeHeap() { return 65920; }
  unsigned getFlashChipSize() { return 4*1024*1024; }
} diagnosticESP;
#define ESP diagnosticESP

struct DiagnosticWiFi {
  int count=0,scans=0,disconnects=0;
  bool deleted=false,scanning=false,neverComplete=false,modeOK=true;
  bool mode(int mode) { assert(mode==WIFI_STA);return modeOK; }
  int scanNetworks(bool async,bool hidden,bool passive,uint32_t=300,uint8_t=0) {
    assert(hidden&&passive);++scans;scanning=async&&count>=0;
    return async?(count<0?WIFI_SCAN_FAILED:WIFI_SCAN_RUNNING):count;
  }
  int scanComplete() { if(neverComplete)return WIFI_SCAN_RUNNING;scanning=false;return count; }
  void disconnect() { ++disconnects; }
  String SSID(int i) { assert(i>=0&&i<count);return String("Network ")+i+" with a long UTF-8 name \xF0\x9F\x8E\xB5"; }
  int channel(int i=-1) { if(i<0)return 6;assert(i<count);return i%13+1; }
  int RSSI(int i=-1) { if(i<0)return -62;assert(i<count);return -100+i; }
  String SSID() { return "Live network"; }
  String BSSIDstr(int) { return "01:23:45:67:89:AB"; }
  uint8_t encryptionType(int) { return WIFI_AUTH_WPA3_PSK; }
  void scanDelete() { deleted=true; }
  String macAddress() { return "01:23:45:67:89:AB"; }
  int status() { return WL_CONNECTED; }
  HostIP localIP() { return HostIP(); }
} diagnosticWiFi;
#define WiFi diagnosticWiFi
int esp_wifi_scan_stop() { WiFi.scanning=false;return 0; }
bool scanLease=false;
bool beginWiFiDiagnosticScan() { if(scanLease)return false;scanLease=true;return true; }
void endWiFiDiagnosticScan() { scanLease=false; }

int bleCount=0,bleInits=0,bleDeinits=0;
bool bleActive=false,noBleScan=false,noBleResults=false,bleInitFails=false,bleNeverComplete=false;
bool bleMetadata=true;
#define CONFIG_BLUEDROID_ENABLED 1
enum { ESP_BLUEDROID_STATUS_UNINITIALIZED,ESP_BLUEDROID_STATUS_INITIALIZED,ESP_BLUEDROID_STATUS_ENABLED };
int bluedroidState=ESP_BLUEDROID_STATUS_UNINITIALIZED,btStops=0;
int esp_bluedroid_get_status() { return bluedroidState; }
int esp_bluedroid_disable() { bluedroidState=ESP_BLUEDROID_STATUS_INITIALIZED;return 0; }
int esp_bluedroid_deinit() { bluedroidState=ESP_BLUEDROID_STATUS_UNINITIALIZED;return 0; }
bool btStop() { assert(bluedroidState==ESP_BLUEDROID_STATUS_UNINITIALIZED);++btStops;return true; }
struct BLEAdvertisedDevice {
  int i=0;
  bool haveName() { return bleMetadata; }
  String getName() { return String("A long BLE device name ")+i; }
  struct Address { int i;String toString() { char addr[18];snprintf(addr,sizeof(addr),"11:22:33:44:%02X:%02X",(i>>8)&255,i&255);return addr; } };
  Address getAddress() { return {i}; }
  int getRSSI() { return -100+i; }
  bool haveManufacturerData() { return bleMetadata; }
  String getManufacturerData() { return String(std::string("\x4c\x00",2)); }
  bool haveAppearance() { return bleMetadata; }
  uint16_t getAppearance() { return 0x0080; }
  int getServiceUUIDCount() { return bleMetadata?2:0; }
};
struct BLEAdvertisedDeviceCallbacks { virtual ~BLEAdvertisedDeviceCallbacks(){};virtual void onResult(BLEAdvertisedDevice)=0; };
struct BLEScanResults {
  int getCount() { return bleCount; }
  BLEAdvertisedDevice getDevice(int i) { assert(i>=0&&i<bleCount);return BLEAdvertisedDevice{i}; }
} scanResults;
struct BLEScan {
  BLEAdvertisedDeviceCallbacks* callback=nullptr;
  bool scanning=false;
  int polls=0,stops=0;
  void setActiveScan(bool active) { assert(!active); }
  void setInterval(int) {} void setWindow(int) {}
  void setAdvertisedDeviceCallbacks(BLEAdvertisedDeviceCallbacks* cb,bool duplicates) { assert(duplicates);callback=cb; }
  bool start(uint32_t,void(*complete)(BLEScanResults),bool continuing) { assert(!complete&&!continuing);scanning=!noBleResults;polls=0;return scanning; }
  bool isScanning() {
    if(scanning&&!bleNeverComplete&&++polls==2) {
      for(int i=0;i<bleCount;++i) if(callback) callback->onResult(BLEAdvertisedDevice{i});
      scanning=false;
    }
    return scanning;
  }
  bool stop() { ++stops;scanning=false;return true; }
  void clearResults() {}
} bleScan;
struct BLEDevice {
  static void init(const char*) { ++bleInits;bleActive=!bleInitFails;bluedroidState=ESP_BLUEDROID_STATUS_ENABLED; }
  static bool getInitialized() { return bleActive; }
  static BLEScan* getScan() { return noBleScan?nullptr:&bleScan; }
  static void deinit(bool release) { assert(!release);++bleDeinits;bleActive=false;bluedroidState=ESP_BLUEDROID_STATUS_UNINITIALIZED; }
};

bool radios[2]={true,false},chips[2]={true,false};
int poweredDown[2]={},forbiddenCalls=0;
bool detected=false;
int receiveSamples=0;
struct RF24 {
  int id;
  bool listening=false;
  RF24(int ce,int cs) : id(ce==NRF1_CE_PIN?0:1) { assert(cs==(id?NRF2_CSN_PIN:NRF1_CSN_PIN)); }
  bool begin() {
    assert(pins[TFT_CS]&&pins[CC1101_CSN_PIN]&&pins[NRF1_CSN_PIN]&&pins[NRF2_CSN_PIN]);
    assert(!pins[NRF1_CE_PIN]&&!pins[NRF2_CE_PIN]);return radios[id];
  }
  bool isChipConnected() { return chips[id]; }
  void powerDown() { ++poweredDown[id]; }
  void setAutoAck(bool ack) { assert(!ack); }
  void setDataRate(int rate) { assert(rate==RF24_2MBPS); }
  void setPALevel(int level) { assert(level==RF24_PA_MIN); }
  void setChannel(int channel) { assert(channel>=0&&channel<126); }
  void startListening() { listening=true; }
  void stopListening() { listening=false; }
  bool testCarrier() { assert(!listening);++receiveSamples;return detected; }
};
bool partReady=true,versionReady=true;
uint8_t part=0,version=0x14;
void dhCcInitPins() { assert(pins[TFT_CS]&&pins[NRF1_CSN_PIN]&&pins[NRF2_CSN_PIN]); }
uint8_t dhCcRead(uint8_t reg,bool* ready) {
  assert(reg==CC_PARTNUM||reg==CC_VERSION);
  *ready=reg==CC_PARTNUM?partReady:versionReady;
  return reg==CC_PARTNUM?part:version;
}

#include "../arduino/CompanionOS_Main/ui_components.h"
#include "../arduino/CompanionOS_Main/dh_diagnostic_ui.h"
#include "dr_hack_passive.inc"

int cases=0,checks=0;
std::string framePath;
void layout(const std::string& name) {
  ++cases;
  HostRect screen{0,0,SCREEN_W,SCREEN_H};
  for(size_t i=0;i<tft.texts.size();++i) {
    const auto& a=tft.texts[i];
    if(!screen.contains(a.measured)) {
      std::cerr<<name<<": out of bounds: "<<a.text<<" at "<<a.measured.x<<","<<a.measured.y<<std::endl;abort();
    }
    ++checks;
    for(size_t j=i+1;j<tft.texts.size();++j) {
      const auto& b=tft.texts[j];++checks;
      if(!intersect(a.measured,b.measured).empty()) {
        std::cerr<<name<<": collision: "<<a.text<<" / "<<b.text<<std::endl;abort();
      }
    }
  }
  assert(tft.pixelWrites>0);
  tft.savePPM(framePath+"/"+name+".ppm");
}

int main(int argc,char** argv) {
  assert(argc==2);framePath=argv[1];pins[BTN_SELECT]=pins[BTN_LEFT]=pins[BTN_RIGHT]=HIGH;
  currentState=STATE_DR_HACK;
  const auto entryTime=hostMillis;initDrHack();assert(hostMillis==entryTime&&dhCurrentState==DH_MENU);
  for(int page=0;page<DH_MENU_PAGES;++page) for(int cursor=0;cursor<DH_TOOLS_PER_PAGE;++cursor) {
    dhMenuPage=page;dhCursorIndex=cursor;tft.clearTrace();dhDrawMenu();
    layout("menu-"+std::to_string(page)+"-"+std::to_string(cursor));
  }
  dhMenuPage=0;dhCursorIndex=0;dhCurrentState=DH_MENU;
  dhNavigate(-1);assert(dhMenuPage==5&&dhCursorIndex==7);
  dhNavigate(1);assert(dhMenuPage==0&&dhCursorIndex==0);
  for(int n:{-1,0,1,20,50,100}) {
    WiFi.count=n;WiFi.deleted=false;dhRunWifiScan();
    assert(WiFi.deleted&&dhNetCount==constrain(n,0,20)&&!WiFi.disconnects);
    for(int i=1;i<dhNetCount;++i) assert(dhNetworks[i-1].rssi>=dhNetworks[i].rssi);
    for(int scroll:{0,19,999,-3}) {
      tft.clearTrace();dhNetScroll=scroll;dhDrawNetList();layout("wifi-"+std::to_string(n)+"-"+std::to_string(scroll));
    }
  }
  for(int n:{0,1,16,50}) {
    bleCount=n;dhRunBleScan();assert(!bleActive&&bleInits==bleDeinits&&dhBleCount==min(n,16));
    for(int scroll:{0,15,999,-3}) {
      dhBleScroll=scroll;tft.clearTrace();dhDrawBleList();layout("ble-"+std::to_string(n)+"-"+std::to_string(scroll));
    }
  }
  tft.clearTrace();noBleScan=true;dhRunBleScan();assert(!bleActive&&bleInits==bleDeinits&&!dhBleCount);noBleScan=false;
  layout("ble-no-controller");
  tft.clearTrace();noBleResults=true;dhRunBleScan();assert(!bleActive&&bleInits==bleDeinits&&!dhBleCount);noBleResults=false;
  layout("ble-no-results");
  for(const char* message:{"nRF24 not detected\nCE32 CS33\nA very long hardware diagnostic message that must wrap without covering the footer.",""}) {
    tft.clearTrace();virtualSelectPressed=true;dhShowError(message);layout(*message?"error-wrapped":"error-empty");
  }
  for(int mode=0;mode<3;++mode) {
    radios[0]=mode!=0;radios[1]=mode==2;chips[0]=mode==2;chips[1]=mode==2;
    poweredDown[0]=poweredDown[1]=0;
    tft.clearTrace();dhRunHwDiag();layout("hardware-"+std::to_string(mode));
    assert(poweredDown[0]==int(radios[0])&&poweredDown[1]==int(radios[1]));
    assert(!pins[NRF1_CE_PIN]&&!pins[NRF2_CE_PIN]&&pins[TFT_CS]&&pins[CC1101_CSN_PIN]);
  }
  assert(dhProbeCc1101());versionReady=false;assert(!dhProbeCc1101());versionReady=true;
  for(int v:{0,255}) {version=v;assert(!dhProbeCc1101());}version=0x14;
  part=255;assert(!dhProbeCc1101());part=0;
  tft.clearTrace();dhDrawSysInfo();layout("system");
  assert(tft.texts.size()==20); // Header/footer and every one of the 9 data rows.
  tft.clearTrace();dhDrawAbout();layout("about");
  for(int selected:{0,10,42,44,46}) {
    dhCurrentState=DH_MENU;dhMenuPage=selected/8;dhCursorIndex=selected%8;
    virtualSelectPressed=true;uint32_t before=hostMillis;dhSelect();
    assert(dhUsesPageInput(dhCurrentState)&&!virtualSelectPressed&&hostMillis>=before);
    if(selected>=42) assert(hostMillis==before);
    dhSelect();assert(dhCurrentState==DH_MENU);
  }
  for(bool waterfall:{false,true}) {
    currentState=STATE_DR_HACK;radios[0]=true;poweredDown[0]=receiveSamples=0;
    virtualSelectPressed=false;int step=0;
    networkStep=[&] {
      if(++step==4) { virtualSelectPressed=true;return; }
      tft.clearCalls();
      if(step==2) { dhScanValues[0]=70;dhScanPeaks[0]=80;virtualLeftPressed=waterfall; }
    };
    dhRunRadioScanner();
    assert(step==4&&poweredDown[0]==1&&receiveSamples==3*126&&!virtualSelectPressed&&!virtualSelectHoldUntil);
    layout(waterfall?"radio-waterfall":"radio-spectrum");
    if(!waterfall) for(int y=0;y<SCALE_Y(12);++y) {
      const auto expected=blendColor(CLR_SUCCESS,darkenColor(CLR_SUCCESS,50),float(y)/(SCALE_Y(12)-1));
      assert(tft.pixels[y*SCREEN_W]==expected); // Old peak scaling painted above the plot.
    }
  }
  currentState=STATE_DR_HACK;int step=0;
  networkStep=[&] { if(++step==2) currentState=STATE_EYES; };
  dhMenuPage=2;dhCursorIndex=1; // Passive Radio Scanner.
  dhCurrentState=DH_MENU;virtualSelectPressed=true;dhSelect();
  assert(step==2&&currentState==STATE_EYES&&dhCurrentState==DH_MENU); // Opening press cannot close the receiver; Home cannot be overpainted.
  currentState=STATE_DR_HACK;step=0;dhMenuPage=5;dhCursorIndex=5; // Passive Input Monitor.
  dhCurrentState=DH_MENU;virtualSelectPressed=true;dhSelect();
  assert(step==2&&currentState==STATE_EYES&&dhCurrentState==DH_MENU);
  for(bool changing:{false,true}) {
    currentState=STATE_DR_HACK;step=0;size_t firstWrites=0;
    pins[IR_RX_PIN]=pins[CC1101_GDO0_PIN]=LOW;tft.clearTrace();
    virtualLeftPressed=virtualRightPressed=true;
    networkStep=[&] {
      hostMillis+=20;++step;
      if(step==2) firstWrites=tft.pixelWrites;
      if(changing&&step==4) pins[IR_RX_PIN]=HIGH;
      if(step==12) {
        if(!changing) assert(tft.pixelWrites==firstWrites);
        virtualSelectPressed=true;
      }
    };
    dhRunInputMonitor();assert(step==12&&!virtualSelectPressed&&!virtualLeftPressed&&!virtualRightPressed);
    if(!changing) layout("input-quiet");
    int zeros=0,ones=0;
    for(const auto& call:tft.texts) { zeros+=call.text=="Changes: 0";ones+=call.text=="Changes: 1"; }
    assert(zeros>=2 && ones==int(changing));
  }
  networkStep=[]{};
  currentState=STATE_DR_HACK;dhCurrentState=DH_WIFI_SCANNER;virtualSelectPressed=false;
  WiFi.count=3;WiFi.neverComplete=false;
  assert(dhPassiveWifiScan()==3&&!scanLease);WiFi.scanDelete();
  WiFi.count=-2;assert(dhPassiveWifiScan()==DH_DISCOVERY_START_FAILED&&!scanLease);
  WiFi.count=3;WiFi.neverComplete=true;
  uint32_t scanStart=hostMillis;assert(dhPassiveWifiScan()==DH_DISCOVERY_TIMEOUT);
  assert(uint32_t(hostMillis-scanStart)==6000&&!scanLease);WiFi.neverComplete=false;
  WiFi.modeOK=false;assert(dhPassiveWifiScan()==DH_DISCOVERY_INIT_FAILED&&!scanLease);WiFi.modeOK=true;
  networkStep=[] { currentState=STATE_EYES; };
  assert(dhPassiveWifiScan()==DH_DISCOVERY_CANCELLED&&!scanLease);networkStep=[]{};currentState=STATE_DR_HACK;
  scanLease=true;assert(dhPassiveWifiScan()==DH_DISCOVERY_BUSY);scanLease=false;

  DhBleRecord found[20];dhCurrentState=DH_BT_SCANNER;
  bleCount=2000;assert(dhPassiveBleScan(found,20)==20&&!dhBleCapture&&!bleActive);
  assert(found[0].company==0x004C&&found[0].services==2);
  assert(dhPassiveBleScan(nullptr,0)==DH_DISCOVERY_NO_MEMORY);
  bleNeverComplete=true;scanStart=hostMillis;int stops=bleScan.stops;
  assert(dhPassiveBleScan(found,20,1)==DH_DISCOVERY_TIMEOUT);
  assert(uint32_t(hostMillis-scanStart)==3000&&bleScan.stops==stops+1&&!bleActive&&!dhBleCapture);
  bleNeverComplete=false;noBleResults=true;
  assert(dhPassiveBleScan(found,20)==DH_DISCOVERY_START_FAILED&&!bleActive&&!dhBleCapture);noBleResults=false;
  bleInitFails=true;int stoppedBefore=btStops;
  assert(dhPassiveBleScan(found,20)==DH_DISCOVERY_INIT_FAILED&&btStops==stoppedBefore+1&&!bleActive);
  bleInitFails=false;
  networkStep=[] { virtualSelectPressed=true; };
  assert(dhPassiveBleScan(found,20)==DH_DISCOVERY_CANCELLED&&!bleActive&&!dhBleCapture);networkStep=[]{};
  dhBleReceiver.onResult(BLEAdvertisedDevice{4000});assert(!dhBleCapture); // Late callbacks cannot touch a released capture.
  DhBleCapture duplicateCapture;dhBleCapture=&duplicateCapture;
  dhBleReceiver.onResult(BLEAdvertisedDevice{0});bleMetadata=false;
  dhBleReceiver.onResult(BLEAdvertisedDevice{0});bleMetadata=true;dhBleCapture=nullptr;
  assert(duplicateCapture.count==1&&duplicateCapture.records[0].name[0]&&
         duplicateCapture.records[0].company==0x004C&&duplicateCapture.records[0].appearance==0x0080&&
         duplicateCapture.records[0].services==2);

  currentState=STATE_DR_HACK;dhCurrentState=DH_BLE_INSPECTOR;bleCount=50;
  dhRunBleInspector();assert(dhBiiCount==20&&!bleActive);
  for(int idx:{0,10,19}) {
    dhBiiCursor=idx;tft.clearTrace();dhBiiDrawList();layout("inspector-list-"+std::to_string(idx));
    tft.clearTrace();dhBiiDrawDetail(idx);layout("inspector-detail-"+std::to_string(idx));
  }
  dhBiiDetail=false;assert(dhBiiSelect()&&dhBiiDetail);assert(dhBiiSelect()&&!dhBiiDetail);
  dhBiiCursor=0;dhBiiNavigate(-1);assert(dhBiiCursor==19);
  dhCurrentState=DH_WIFI_CONFIG;currentWiFiSSID="Wrong old profile";
  tft.clearTrace();dhRunWifiConfig();layout("wifi-live-status");
  bool liveSSID=false;for(const auto& text:tft.texts) liveSSID|=text.text=="Live network";assert(liveSSID);

  auto hasText=[](const char* value) {
    for(const auto& call:tft.texts) if(call.text==value)return true;
    return false;
  };
  // The front end must not turn a driver failure into an empty-results message.
  dhCurrentState=DH_WIFI_SCANNER;WiFi.count=-2;tft.clearTrace();dhRunWifiDiscovery();
  assert(hasText("Radio rejected scan")&&!hasText("No networks found"));
  tft.clearTrace();dhDiagnosticStatus("WIFI SCAN",dhDiscoveryErrorText(DH_DISCOVERY_START_FAILED));layout("wifi-start-failed");
  dhCurrentState=DH_BT_SCANNER;noBleResults=true;tft.clearTrace();dhRunBleScan();
  assert(hasText("Radio rejected scan")&&!hasText("No BLE devices"));noBleResults=false;
  tft.clearTrace();dhDiagnosticStatus("BLE SCAN",dhDiscoveryErrorText(DH_DISCOVERY_TIMEOUT));layout("ble-timeout");
  networkStep=[] { currentState=STATE_EYES;tft.clearTrace(); };
  dhRunBleScan();assert(currentState==STATE_EYES&&tft.texts.empty());
  currentState=STATE_DR_HACK;dhCurrentState=DH_WIFI_SCANNER;WiFi.count=5;
  dhRunWifiDiscovery();assert(currentState==STATE_EYES&&tft.texts.empty());
  currentState=STATE_DR_HACK;networkStep=[]{};

  dhCurrentState=DH_CHANNEL_SCAN;WiFi.count=100;
  assert(dhChScan()&&dhChNetCount==DH_CH_MAX_NETS);
  for(int scroll:{0,5,9}) {
    dhChScroll=scroll;tft.clearTrace();dhChDrawList();layout("channels-"+std::to_string(scroll));
  }
  WiFi.count=0;assert(dhChScan());tft.clearTrace();dhChDrawList();layout("channels-empty");
  dhCurrentState=DH_WIFI_RADAR;WiFi.count=1;
  dhRadarTarget={"Long selected access point name","01:23:45:67:89:ab",6,-100};
  assert(dhRadarScanTarget()&&dhRadarSeen&&dhRadarRssi==-100&&!dhRadarScanError);
  tft.clearTrace();dhRadarDrawScreen();layout("radar-signal");
  WiFi.count=0;assert(!dhRadarScanTarget()&&!dhRadarSeen&&!dhRadarScanError);
  tft.clearTrace();dhRadarDrawScreen();layout("radar-not-seen");
  WiFi.count=-2;assert(!dhRadarScanTarget()&&dhRadarScanError==DH_DISCOVERY_START_FAILED);
  tft.clearTrace();dhRadarDrawScreen();layout("radar-failed");
  WiFi.count=1;dhCurrentState=DH_WIFI_DIRECTION;dhDirTarget=dhRadarTarget;
  int measured=0;assert(dhDirMeasure(measured)&&measured==-100);
  int beforeScans=WiFi.scans;
  networkStep=[] { currentState=STATE_EYES;tft.clearTrace(); };
  assert(!dhWifiWaitSelect(DH_WIFI_DIRECTION)&&WiFi.scans==beforeScans&&tft.texts.empty());
  currentState=STATE_DR_HACK;assert(!dhDirMeasure(measured)&&measured==-127&&tft.texts.empty());
  networkStep=[]{};currentState=STATE_DR_HACK;
  scanStart=hostMillis;assert(!dhWifiWaitSelect(DH_WIFI_DIRECTION)&&hostMillis-scanStart==DH_BTN_TIMEOUT_MS);
  assert(!forbiddenCalls);
  std::cout<<"PASS Dr.Hack diagnostics: "<<cases<<" layouts, "<<checks
           <<" checks; bounded scans, reversible BLE cleanup, nonblocking info/hardware, no transmit code ("
           <<SCREEN_W<<"x"<<SCREEN_H<<")\n";
}
