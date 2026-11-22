#include "../arduino/CompanionOS_Main/standalone_services_detail.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <vector>
#include <string>
using namespace standalone_detail;

static void imageTest(unsigned width, unsigned height) {
  uint16_t dest[4096] = {};
  for (unsigned y = 0; y < height; y += 16) {
    for (unsigned x = 0; x < width; x += 16) {
      unsigned w = width - x < 16 ? width - x : 16;
      unsigned h = height - y < 16 ? height - y : 16;
      std::vector<uint16_t> block(w * h);
      for (unsigned yy = 0; yy < h; ++yy)
        for (unsigned xx = 0; xx < w; ++xx) block[yy * w + xx] = uint16_t((y + yy) * width + x + xx);
      sampleBlock(dest, width, height, x, y, w, h, block.data());
    }
  }
  for (unsigned y = 0; y < 64; ++y)
    for (unsigned x = 0; x < 64; ++x)
      assert(dest[y * 64 + x] == uint16_t((y * height / 64) * width + x * width / 64));
}

int main() {
  companion_lyrics::Navigation nav;
  assert(nav.tick(0,true,false)==companion_lyrics::Navigation::Open);
  nav.manual(false);
  assert(nav.tick(1,true,false)==companion_lyrics::Navigation::None);
  assert(nav.tick(UINT32_MAX-2000,false,false)==companion_lyrics::Navigation::None);
  assert(nav.tick(2998,false,false)==companion_lyrics::Navigation::None);
  assert(nav.session);
  assert(nav.tick(2999,false,false)==companion_lyrics::Navigation::None);
  assert(!nav.session);
  assert(nav.tick(3000,true,false)==companion_lyrics::Navigation::Open);
  nav.tick(3001,false,true);
  assert(nav.tick(7000,true,true)==companion_lyrics::Navigation::None);
  assert(nav.session); // Brief pause/resume is still one session.
  char line[32], next[32];
  companion_lyrics::Timing timing;
  const char* enhanced="[offset:500]\n[00:01]<00:01>Hold <00:02>the <00:03>light\n[00:04]Next";
  assert(lyricLines(enhanced,true,1600,line,next,&timing));
  assert(!strcmp(line,"Hold the light") && timing.wordTimed && timing.word==1);
  assert(timing.start==500 && timing.next==3500);
  lyricLines(enhanced,true,500,line,next,&timing);
  assert(timing.word==0); // Exact timing after backwards seek.
  lyricLines("[00:00]Ordinary line",true,2000,line,next,&timing);
  assert(!timing.wordTimed && timing.word==-1);
  const char* timed = "[ar:Test]\n[00:01.20]First\r\n[00:03.456]Second\n[00:07.00]\n[00:09.0][00:12.0]Refrain";
  assert(lyricLines(timed, true, 0, line, next) && !line[0] && !strcmp(next, "First"));
  assert(lyricLines(timed, true, 1200, line, next) && !strcmp(line, "First") && !strcmp(next, "Second"));
  assert(lyricLines(timed, true, 3456, line, next) && !strcmp(line, "Second") && !next[0]);
  assert(lyricLines(timed, true, 7000, line, next) && !line[0] && !strcmp(next, "Refrain"));
  assert(lyricLines(timed, true, 12000, line, next) && !strcmp(line, "Refrain") && !next[0]);
  assert(lyricLines(timed, true, 1200, line, next) && !strcmp(line, "First")); // Backward seek.
  assert(lyricLines("[00:10]Later\n[00:02]Earlier", true, 3000, line, next) && !strcmp(line, "Earlier"));
  assert(lyricLines("[00:02.00]Offset\n[offset:+500]", true, 1500, line, next) && !strcmp(line, "Offset"));
  assert(lyricLines("[offset:-500]\n[00:02.00]Offset", true, 2000, line, next) && !line[0]);
  assert(!lyricLines("[ar:test]\n[00:99]bad\n[xxx]bad", true, 0, line, next));
  assert(lyricLines("Plain first\nPlain next", false, 0, line, next) && !strcmp(line, "Plain first"));
  assert(lyricLines("Plain first\nPlain next", false, 6000, line, next) && !strcmp(line, "Plain next"));
  char tiny[5], tinyNext[5];
  assert(lyricLines("[00:00]abc\xC3\xA9 long", true, 0, tiny, tinyNext) && !strcmp(tiny, "abc"));
  char twoByte[3], twoByteNext[3], threeByte[4], threeByteNext[4];
  assert(lyricLines("[00:00]\xC3\xA9", true, 0, twoByte, twoByteNext) && !strcmp(twoByte, "\xC3\xA9"));
  assert(lyricLines("[00:01]\xE4\xB8\x80", true, 0, threeByte, threeByteNext) &&
         !strcmp(threeByteNext, "\xE4\xB8\x80"));
  assert(lyricLines("[00:00]\xF0\x9F\x8E\xB5", true, 0, tiny, tinyNext) &&
         !strcmp(tiny, "\xF0\x9F\x8E\xB5"));
  // Timing validity must not depend on a preview fitting in the output buffer.
  assert(lyricLines("[00:01]\xF0\x9F\x8E\xB5", true, 0, threeByte, threeByteNext) && !threeByteNext[0]);
  assert(lyricLines("[00:01]\n[00:02]Verse", true, 0, line, next) && !line[0] && !next[0]);
  assert(lyricLines("[00:01]\n[00:02]Verse", true, 2000, line, next) && !strcmp(line, "Verse"));
  Gate gate;
  assert(gate.ready(0));
  gate.defer(UINT32_MAX - 15, 32);
  assert(!gate.ready(15));
  assert(gate.ready(16));
  assert(gate.remaining(0) == 16);
  assert(gate.remaining(16) == 0);
  AuthRequestLimiter limiter;
  assert(limiter.ready(0, false)); limiter.attempted(0);
  assert(!limiter.ready(14999, true) && limiter.ready(15000, true));
  assert(!limiter.ready(299999, false) && limiter.ready(300000, false));
  CredentialRecord record;
  assert(!validCredentialRecord(record));
  strcpy(record.refresh, "dummy-refresh"); strcpy(record.clientId, "dummy-client");
  record.checksum = credentialChecksum(record); assert(validCredentialRecord(record));
  record.refresh[1] ^= 1; assert(!validCredentialRecord(record)); record.refresh[1] ^= 1;
  record.secret[0] = '\n'; record.checksum = credentialChecksum(record); assert(!validCredentialRecord(record));
  eraseCredentialRecord(record); assert(record.version == 0 && !record.refresh[0]);
  uint8_t guarded[] = {99, 0, 0, 0, 0, 98};
  BoundedBody body(guarded + 1, 4, 0);
  const uint8_t bytes[] = {1, 2, 3, 4, 5};
  assert(body.append(bytes, 4, 1) == 4);
  assert(body.append(bytes, 1, 2) == 0 && body.error == TooLarge);
  assert(body.append(bytes, 0, 3) == 0 && body.error == TooLarge);
  assert(guarded[0] == 99 && guarded[5] == 98 && body.size == 4);
  BoundedBody deadline(guarded + 1, 4, UINT32_MAX - 100);
  assert(deadline.append(bytes, 1, 14900) == 0 && deadline.error == TimedOut);
  assert(!configured("", "", ""));
  assert(!configured("", "refresh", ""));
  assert(configured("access", "", ""));
  assert(configured("", "refresh", "client"));
  std::string longText(600, 'x'), changed = longText;
  changed[500] = 'y';
  assert(textFingerprint(longText.c_str()) != textFingerprint(changed.c_str()));
  char display[16]; copyDisplay(display, longText.c_str()); assert(strlen(display) == 15);
  char utf8[4]; copyDisplay(utf8, "ab\xe2\x82\xac"); assert(!strcmp(utf8, "ab"));
  char redirect[128];
  assert(redirectTarget(redirect, "https://api.test/v1/player", "/v2/player", true));
  assert(!strcmp(redirect, "https://api.test/v2/player"));
  assert(redirectTarget(redirect, "https://api.test/v1/player", "next", true));
  assert(!redirectTarget(redirect, "https://api.test/", "https://other.test/image", true));
  assert(redirectTarget(redirect, "https://api.test/", "https://other.test/image", false));
  assert(!redirectTarget(redirect, "https://api.test/", "http://other.test/", false));
  assert(!redirectTarget(redirect, "https://api.test/", "//other.test/", false));
  assert(!redirectTarget(redirect, "https://api.test/", "/image\r\nAuthorization:bad", false));
  assert(!httpsOriginLength("https://user@api.test/image"));
  assert(!httpsOriginLength("https://api.test:80/image"));
  assert(sameHttpsOrigin("https://API.test/a", "https://api.test/b"));
  assert(strcmp(failureName(AuthInvalidClient), failureName(AuthSignInRequired)));
  assert(retryAfterMs(nullptr) == 60000);
  assert(retryAfterMs("") == 60000);
  assert(retryAfterMs("bad") == 60000);
  assert(retryAfterMs("0") == 1000);
  assert(retryAfterMs("120") == 120000);
  assert(retryAfterMs("2147483") == 2147483000);
  assert(retryAfterMs("99999999999999999999") == 2147483647);
  assert(failureDelay(0) == 2000);
  assert(failureDelay(255) == 60000);
  assert(commandFor("PLAY_PAUSE") == PlayPause);
  assert(commandFor("PREV") == Previous);
  assert(commandFor("NEXT") == Next);
  assert(commandFor("SHUFFLE:TOGGLE") == Shuffle);
  assert(commandFor("SPOTIFY:NEXT") == None);
  char text[4];
  assert(copy(text, "abc"));
  assert(!copy(text, "abcd") && text[0] == 0);
  assert(copy(text, nullptr) && text[0] == 0);
  assert(progressAt(900, 1000, true, 200) == 1000);
  assert(progressAt(900, 1000, false, 200) == 900);
  assert(progressAt(-1, 1000, true, 200) == 200);
  assert(progressAt(INT_MAX, INT_MAX, true, UINT32_MAX) == INT_MAX);
  assert(progressAt(5, 0, true, 200) == 0);
  int code = 0;
  assert(!strcmp(weatherName(71, code), "Snow") && code == 600);
  assert(!strcmp(weatherName(95, code), "Thunderstorm") && code == 200);
  assert(!strcmp(weatherName(999, code), "Clouds") && code == 803);
  imageTest(64, 64); imageTest(75, 75); imageTest(80, 37);
  imageTest(20, 20); imageTest(1, 1); imageTest(128, 128);
  puts("standalone detail tests passed");
}
