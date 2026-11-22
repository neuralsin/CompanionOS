"""Exercise actual tile packet handlers with ArduinoJson, not copied parsers."""

from pathlib import Path
import os
import shutil
import subprocess
import tempfile

HOST = r'''
#include "Arduino.h"
#include <ArduinoJson.h>
#include <cassert>
#include <cstring>
#include <cmath>
using std::min;
using std::isfinite;
#define COMPANION_HOST_MEMORY
#include "memory_policy.h"
namespace companion_memory { Snapshot testSnapshot = {256*1024,200*1024}; }
uint32_t fakeNow=100;
MockSerial Serial;
int currentState=0;
constexpr int STATE_EYES=0, STATE_STOCKS=6, STATE_GAMING=7, STATE_SOCIAL=8;
constexpr int STATE_PRODUCTIVITY=9, STATE_RUVIEW=13, STATE_COUNT=17;
unsigned long lastHostPacketMillis=0,lastHostPomodoroMillis=0,pomoClockBaseline=0;
int pomoRemaining=1500,pomoTotal=1500,pomoSessions=0;
bool pomoActive=false,pomoIsBreak=false;
String notifApps[3],notifTitles[3],notifTimes[3],currentNotes[4];
int notifTotal=0,notifCount=0,redraws=0,banners=0,samples=0;
char stockSymbol[16]="",stockPrice[16]="",stockDelta[16]="",stockPctChg[16]="";
char wlSymbol[3][16]={},wlPrice[3][16]={},wlDelta[3][16]={};
bool stockIsUp=false,wlIsUp[3]={};
int16_t stockHistory[40]={};int stockHistoryLen=0;
char gameTitle[24]="",sessionTime[12]="",gameStatus[16]="",recentGame[3][24]={};
uint16_t recentPlaytime[3]={};uint8_t achievePct=0,friendsOnline=0;bool gameActive=false;
char socialUser[16]="",socialApp[12]="",socialBody[80]="",socialTime[8]="";
uint16_t socialLikes=0,socialComments=0;
char taskCurrent[32]="",taskCurrentTime[20]="",taskNext1[32]="",taskNext1Time[16]="";
char taskNext2[32]="",taskNext2Time[16]="";bool taskActive=false;uint8_t taskProgressPct=0;
bool rvOccupied=false,rvMotion=false,rvCalibrating=false;
float rvConfidence=0,rvVariance=0,rvPPS=0;int8_t rvRSSI=0;int rvSubcarriers=0;
char rvStatus[24]="Offline",rvZoneLabel[16]="Room";
void redrawPomodoroPartial(){++redraws;} void redrawNotificationsPartial(){++redraws;}
void redrawNotesPartial(){++redraws;} void redrawStocksPartial(){++redraws;}
void redrawGamingPartial(){++redraws;} void redrawSocialPartial(){++redraws;}
void redrawProductivityPartial(){++redraws;} void redrawRuviewPartial(){++redraws;}
void showFlashNotification(String){++banners;} void noteRuviewPacket(){++samples;}
'''

TESTS = r'''
int main() {
  for (const char* packet : {"POMO:bad", "POMO:[]", "POMO:{}",
       R"(POMO:{"remaining":-1})", R"(POMO:{"total":1,"remaining":2})",
       R"(POMO:{"active":"true"})", R"(POMO:{"remaining":1.5})",
       R"(POMO:{"sessions":10000})", R"(POMO:{"active":true,"total":0,"remaining":0})"}) {
    receivePomo(packet);
    assert(!lastHostPomodoroMillis && !pomoClockBaseline && pomoRemaining==1500);
    assert(!pomoActive && !redraws);
  }
  receivePomo(R"(POMO:{"total":300,"remaining":150,"active":true,"is_break":true,"sessions":4})");
  assert(lastHostPomodoroMillis==fakeNow && pomoClockBaseline==fakeNow);
  assert(pomoRemaining==150 && pomoTotal==300 && pomoActive && pomoIsBreak && pomoSessions==4);
  fakeNow+=100;receivePomo(R"(POMO:{"remaining":null})");
  assert(lastHostPomodoroMillis!=fakeNow && pomoRemaining==150);

  receiveNotif(R"(NOTIF:[{"app":"Mail","title":"Hello","time":"10:00"}])");
  assert(notifTotal==1 && banners==1 && notifTitles[0]=="Hello");
  receiveNotif(R"(NOTIF:[{"app":"Mail","title":"Hello","time":"10:00"}])");
  assert(banners==1); // Identical polling cannot perpetually restart the banner.
  receiveNotif("NOTIF:{}");assert(notifTitles[0]=="Hello");
  receiveNotif("NOTIF:[]");assert(!notifTotal && notifTitles[0].empty());
  receiveNotes(R"(NOTES:["one","two","three","four"])");
  receiveNotes("NOTES:{}");assert(currentNotes[3]=="four");
  receiveNotes(R"(NOTES:["new"])");assert(currentNotes[0]=="new" && currentNotes[1].empty());

  currentState=STATE_STOCKS;
  receiveStocks(R"(STOCKS:{"symbol":"REAL","wl":[{"s":"A"},{"s":"B"},{"s":"C"}],"hist":[1,2,3]})");
  assert(!strcmp(wlSymbol[2],"C") && stockHistoryLen==3);
  receiveStocks(R"(STOCKS:{"symbol":"WRONG","wl":"bad"})");assert(!strcmp(stockSymbol,"REAL"));
  receiveStocks(R"(STOCKS:{"wl":[{"s":"NEW"}],"hist":[]})");
  assert(!strcmp(wlSymbol[0],"NEW") && !wlSymbol[1][0] && !wlPrice[2][0] && !stockHistoryLen);
  currentState=STATE_EYES;receiveStocks(R"(STOCKS:{"symbol":"DISTANT"})");
  assert(!strcmp(stockSymbol,"REAL"));

  currentState=STATE_GAMING;
  receiveGaming(R"(GAMING:{"title":"Real game","recent":[{"name":"A","time":100000},{"name":"B"},{"name":"C"}],"achieve":999,"friends":999})");
  assert(achievePct==100 && friendsOnline==255 && recentPlaytime[0]==65535);
  receiveGaming(R"(GAMING:{"title":"Wrong","recent":"bad"})");assert(!strcmp(gameTitle,"Real game"));
  receiveGaming(R"(GAMING:{"recent":[{"name":"NEW","time":-7}],"achieve":-1,"friends":-1})");
  assert(!recentGame[1][0] && !recentPlaytime[0] && !recentPlaytime[2] && !achievePct && !friendsOnline);
  currentState=STATE_PRODUCTIVITY;receiveTasks(R"(TASKS:{"progress":300,"current":"Real task"})");
  assert(taskProgressPct==100);receiveTasks(R"(TASKS:{"progress":-5})");assert(!taskProgressPct);
  currentState=STATE_SOCIAL;receiveSocial(R"(SOCIAL:{"likes":-7,"comments":100000})");
  assert(socialLikes==0 && socialComments==65535);

  currentState=STATE_RUVIEW;
  for(const char* packet : {"RUVIEW:{}", "RUVIEW:[]", R"(RUVIEW:{"status":"Motion","pps":-1})",
      R"(RUVIEW:{"status":7,"pps":20})",R"(RUVIEW:{"status":"Motion","pps":9999})"}) receiveRuview(packet);
  assert(!samples && !rvOccupied);
  receiveRuview(R"(RUVIEW:{"status":"Motion","pps":20,"occupied":true,"motion":true,"confidence":500,"variance":-1,"rssi":-999,"subcarriers":9999,"label":"Long room label beyond capacity"})");
  assert(samples==1 && rvOccupied && rvMotion && rvPPS==20 && rvConfidence==100);
  assert(!rvVariance && rvRSSI==-127 && rvSubcarriers==1024 && rvZoneLabel[15]==0);
  receiveRuview(R"(RUVIEW:{"status":"Offline","pps":0,"occupied":true,"motion":true,"confidence":99})");
  assert(samples==2 && !rvOccupied && !rvMotion && !rvConfidence);
  currentState=STATE_EYES;receiveRuview(R"(RUVIEW:{"status":"Motion","pps":20})");assert(samples==2);
  puts("PASS actual tile handlers: atomic timer lease, duplicate banners, shrinking lists, numeric bounds, CSI validity");
}
'''


def main():
    tests = Path(__file__).resolve().parent
    firmware = tests.parent / "arduino/CompanionOS_Main"
    source = (firmware / "companion_net.h").read_text(encoding="utf-8")
    macro_start = source.index("#define SAFESTR(")
    macro_end = source.index("while (0)", macro_start) + len("while (0)")
    handlers = []
    for prefix, name in [('POMO', 'Pomo'), ('NOTIF', 'Notif'), ('NOTES', 'Notes'),
                         ('STOCKS', 'Stocks'), ('GAMING', 'Gaming'), ('SOCIAL', 'Social'),
                         ('TASKS', 'Tasks'), ('RUVIEW', 'Ruview')]:
        marker = f'msg.startsWith("{prefix}:")) {{'
        start = source.index(marker) + len(marker)
        end = source.index('\n  }', start)
        handlers.append(f'void receive{name}(String msg) {{\n{source[start:end]}\n}}\n')
    compiler = shutil.which('g++') or 'C:/mingw/bin/g++.exe'
    libraries = Path(os.environ['USERPROFILE']) / 'OneDrive/Documents/Arduino/libraries'
    env = dict(os.environ)
    env['PATH'] = str(Path(compiler).parent) + os.pathsep + env.get('PATH', '')
    with tempfile.TemporaryDirectory(prefix='companion-tiles-') as directory:
        executable = Path(directory) / 'tile-input.exe'
        subprocess.run([compiler, '-std=c++17', '-Wall', '-Wextra', '-Werror',
                        '-Wno-deprecated-declarations', '-Wno-unused-function',
                        '-I', str(tests / 'standalone_stubs'), '-I', str(firmware),
                        '-I', str(libraries / 'ArduinoJson/src'), '-x', 'c++', '-', '-o', str(executable)],
                       input=HOST + source[macro_start:macro_end] + '\n' + ''.join(handlers) + TESTS,
                       text=True, encoding='utf-8', env=env, check=True)
        subprocess.run([str(executable)], env=env, check=True)


if __name__ == '__main__':
    main()
