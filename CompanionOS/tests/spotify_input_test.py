"""Compile and exercise the actual SPOTIFY_AUTH handler without radio operations."""

from pathlib import Path
import os
import shutil
import subprocess
import tempfile

HOST = r'''
#define ESP32
#include "Arduino.h"
#include <ArduinoJson.h>
#include <cassert>
#include <utility>
uint32_t fakeNow=1;
MockSerial Serial;
String spotifyAccessToken="old-access", spotifyRefreshToken="old-refresh";
String spotifyClientId="old-client", spotifyClientSecret="old-secret";
bool isHostAndroid=false;
unsigned changes=0, notifications=0, acknowledgements=0;
void notifyStandaloneCredentialsChanged() { ++changes; }
void showFlashNotification(const char*) { ++notifications; }
void sendCommand(const char* msg) { assert(!strcmp(msg,"SPOTIFY_AUTH_OK")); ++acknowledgements; }
'''

TESTS = r'''
int main() {
  const char* valid=R"(SPOTIFY_AUTH:{"access":"new-access","refresh":"new-refresh","client_id":"new-client","client_secret":"new-secret"})";
  syncAuth(valid);
  assert(changes==1 && notifications==1 && acknowledgements==1 && !isHostAndroid);
  assert(spotifyAccessToken=="new-access" && spotifyRefreshToken=="new-refresh");
  syncAuth(valid);
  assert(changes==1 && notifications==1 && acknowledgements==2); // Duplicate ACK, no NVS write.
  for (const char* input : {
      "SPOTIFY_AUTH:{}", "SPOTIFY_AUTH:bad",
      R"(SPOTIFY_AUTH:{"access":"newer"})",
      R"(SPOTIFY_AUTH:{"access":7,"refresh":"r","client_id":"c","client_secret":"s"})",
      R"(SPOTIFY_AUTH:{"access":"a","refresh":"","client_id":"c","client_secret":"s"})",
      R"(SPOTIFY_AUTH:{"access":"a","refresh":"r\nx","client_id":"c","client_secret":"s"})",
      R"(SPOTIFY_AUTH:{"access":"a","refresh":"r\u0000x","client_id":"c","client_secret":"s"})"
    }) {
    syncAuth(input);
    assert(changes==1 && acknowledgements==2);
    assert(spotifyAccessToken=="new-access" && spotifyRefreshToken=="new-refresh");
    assert(spotifyClientId=="new-client" && spotifyClientSecret=="new-secret");
  }
  syncAuth(String("SPOTIFY_AUTH:")+std::string(1801,'x'));
  assert(changes==1 && acknowledgements==2);
  syncAuth(R"(SPOTIFY_AUTH:{"access":"","refresh":"public-refresh","client_id":"public-client","client_secret":""})");
  assert(changes==2 && acknowledgements==3 && spotifyClientSecret.empty() && !isHostAndroid);
  assert(Serial.output.empty());
  puts("Spotify input tests passed: atomic tuples, persistence callback, ACK, identity, bounds and redaction");
}
'''


def main():
    tests = Path(__file__).resolve().parent
    source = (tests.parent / "arduino/CompanionOS_Main/companion_net.h").read_text(encoding="utf-8")
    marker = '  } else if (msg.startsWith("SPOTIFY_AUTH:")) {'
    start = source.index(marker) + len(marker)
    end = source.index('  } else if (msg.startsWith("STATE:")) {', start)
    handler = "void syncAuth(String msg) {\n" + source[start:end] + "}\n"
    compiler = shutil.which("g++") or "C:/mingw/bin/g++.exe"
    libraries = Path(os.environ["USERPROFILE"]) / "OneDrive/Documents/Arduino/libraries"
    with tempfile.TemporaryDirectory(prefix="companion-spotify-input-") as directory:
        executable = Path(directory) / "spotify-input.exe"
        subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                        "-Wno-deprecated-declarations", "-Wno-unused-function",
                        "-I", str(tests / "standalone_stubs"),
                        "-I", str(libraries / "ArduinoJson/src"),
                        "-x", "c++", "-", "-o", str(executable)],
                       input=HOST + handler + TESTS, text=True, encoding="utf-8", check=True)
        subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
