"""Compile the real UDP time parser: python CompanionOS/tests/clock_input_test.py."""

from pathlib import Path
import shutil
import subprocess
import tempfile


HOST = r'''
#include "host_arduino.h"
#include <cassert>
int displayHour=0, displayMinute=0, displaySecond=0;
bool timeReceived=false;
unsigned long lastTimeUpdateMillis=0;
'''

TESTS = r'''
int main() {
  struct Valid { const char* input; int h,m,s; };
  const Valid good[] = {
    {"00:00",0,0,0}, {"00:00:00",0,0,0}, {"23:59:59",23,59,59},
    {"12:34",12,34,0}, {"7:05",7,5,0}, {"08:01:02",8,1,2}, {"1:2:3",1,2,3}
  };
  for(const auto& item:good) {
    hostMillis+=123;
    timeReceived=false;
    updateTimeFromUDP(item.input);
    assert(displayHour==item.h&&displayMinute==item.m&&displaySecond==item.s);
    assert(timeReceived&&lastTimeUpdateMillis==millis());
  }
  const char* bad[] = {
    "", "bad", "abcd", "24:00", "99:00", "23:60", "23:59:60", "-1:00",
    "12:-1", "12:00:-1", ":00", "12:", "12::34", "12:34:", "12:34x",
    "12:34:5x", "12:34:56x", "99999:00", "999999999999999999999999999:00",
    "12:34 ", "12:34\n", "12:34:56:7", "12:34abc", "1e1:00"
  };
  for(bool received:{false,true}) {
    for(const char* input:bad) {
      displayHour=14;displayMinute=27;displaySecond=39;
      timeReceived=received;lastTimeUpdateMillis=42;hostMillis+=123;
      updateTimeFromUDP(input);
      assert(displayHour==14&&displayMinute==27&&displaySecond==39);
      assert(timeReceived==received&&lastTimeUpdateMillis==42);
    }
  }
  puts("PASS clock input: actual ino parser, valid H:M[:S], bounds/tail validation, malformed input preserves all state");
}
'''


def main():
    tests = Path(__file__).resolve().parent
    sketch = (tests.parent / "arduino/CompanionOS_Main/CompanionOS_Main.ino").read_text(encoding="utf-8")
    start = sketch.index("void updateTimeFromUDP(String t) {")
    end = sketch.index("\n}", start) + 2
    parser = sketch[start:end]
    compiler = shutil.which("g++")
    if not compiler:
        raise SystemExit("g++ is required for native tests")
    with tempfile.TemporaryDirectory(prefix="companion-clock-") as directory:
        executable = Path(directory) / "clock_input_test.exe"
        subprocess.run(
            [compiler, "-std=c++11", "-Wall", "-Wextra", "-Werror", "-I", str(tests),
             "-x", "c++", "-", "-o", str(executable)],
            input=HOST + parser + TESTS, text=True, encoding="utf-8", check=True,
        )
        subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
