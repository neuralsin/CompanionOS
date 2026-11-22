#define ESP32 1
#define STANDALONE_HOST_TEST 1
#include "standalone_stubs/HTTPClient.h"
#include <cassert>
#include <iostream>
uint32_t fakeNow = 1;
int fakeTlsError = 0;
size_t fakeFreeHeap = 120 * 1024, fakeLargestBlock = 90 * 1024;
MockSerial Serial;
std::deque<MockReply> replies;
std::vector<MockCall> calls;
unsigned bodyDecodes = 0;
bool hostActive = false, playbackActive = false;
bool isHostActive() { return hostActive; }
bool isHostPlaybackActive() { return playbackActive; }
bool weatherActive = false;
uint32_t weatherLeaseAt = 0;
bool isHostWeatherActive();
#include "standalone_compile/standalone_compile.ino"
bool isHostWeatherActive() { return weatherActive && uint32_t(fakeNow - weatherLeaseAt) < WEATHER_POLL_INTERVAL_MS; }
using namespace standalone_services;

static std::string chunks(const std::string& bytes) {
  std::string wire;
  for (size_t at = 0; at < bytes.size(); at += 13) {
    size_t size = std::min(size_t(13), bytes.size() - at);
    char hex[32]; snprintf(hex, sizeof(hex), "%x", unsigned(size));
    wire += std::string(hex) + "\r\n" + bytes.substr(at, size) + "\r\n";
  }
  return wire + "0\r\n\r\n";
}
static MockReply reply(int status, const std::string& body = "", bool chunked = false) {
  MockReply r; r.status = status; r.body = chunked ? chunks(body) : body;
  r.chunked = chunked; r.size = chunked ? -1 : int(body.size()); return r;
}
static void resetResult() { result.~Result(); new (&result) Result(); }
static void cleanHttp() { assert(replies.empty()); calls.clear(); fakeTlsError = 0; resetResult(); }
static const char* weatherJson = R"({"current":{"temperature_2m":-1.5,"apparent_temperature":-3,"relative_humidity_2m":42,"weather_code":71,"wind_speed_10m":8},"daily":{"temperature_2m_max":[2],"temperature_2m_min":[-4],"sunrise":["2026-10-16T06:00"],"sunset":["2026-10-16T18:00"]}})";
static void finishWeather() {
  uint8_t ticket; assert(xQueueReceive(jobs, &ticket, 0));
  resetResult(); weather(); assert(xQueueSend(results, &ticket, 0));
}

static void testDemandAndPriority() {
  assert(!busy && jobs->bytes.empty() && results->bytes.empty());
  wifiConnected = true;
  hostActive = playbackActive = weatherActive = dhNetPaused = isHostAndroid = false;
  currentState = STATE_EYES;
  observeOwner();
  spotifyAccessToken = spotifyRefreshToken = spotifyClientId = "";
  assert(!weatherAllowed());
  tickStandaloneServices(); assert(!busy); // No off-page weather job.
  companion_memory::radioWorkspace() = true;
  currentState = STATE_WEATHER;
  spotifyAccessToken = "test-priority";
  assert(!spotifyAllowed() && !weatherAllowed());
  tickStandaloneServices(); assert(!busy); // Radio workspace never competes for TLS.
  companion_memory::radioWorkspace() = false;
#if ENABLE_SPOTIFY
  currentState = STATE_SPOTIFY; // Both jobs eligible in the compact host enum.
  spotifyGate = weatherGate = Gate(); spotifyDue = true;
  tickStandaloneServices();
  assert(busy && job.kind == Playback && companion_memory::networkBusy.load());
  uint8_t ticket;
  assert(xQueueReceive(jobs, &ticket, 0));
  resetResult(); result.status = 204; result.valid = true;
  assert(xQueueSend(results, &ticket, 0));
  consume(fakeNow);
  assert(!busy && !companion_memory::networkBusy.load());
  currentState = STATE_LYRICS;
  assert(!weatherAllowed());
  spotifyDue = false; lastPoll = fakeNow;
  spotifyGate = Gate(); itemId = "";
  fakeNow += 999; tickStandaloneServices(); assert(!busy);
  ++fakeNow; tickStandaloneServices();
  assert(busy && job.kind == Playback && job.spotifyPage);
  assert(xQueueReceive(jobs, &ticket, 0));
  resetResult(); result.status = 429; result.retryMs = 60000;
  assert(xQueueSend(results, &ticket, 0)); consume(fakeNow);
  fakeNow += 1000; tickStandaloneServices(); assert(!busy);
  assert(!spotifyGate.ready(fakeNow)); // Fast polling never bypasses Retry-After.
#endif
  spotifyAccessToken = "";
}

static void testWeatherOwnership() {
  currentState = STATE_WEATHER;
  // PC dashboard/playback traffic is not evidence of a working host weather API.
  assert(!busy);
  wifiConnected = hostActive = playbackActive = true;
  dhNetPaused = isHostAndroid = weatherActive = false;
  observeOwner();
  size_t before = calls.size();
  assert(weatherAllowed()); tickStandaloneServices();
  assert(busy && job.kind == Weather);

  // A valid host weather update acquires bit 32 and invalidates native work
  // already in flight. Its cached data must survive the full lease.
  uint32_t oldEpoch = epoch;
  weatherActive = true; weatherLeaseAt = fakeNow; weatherTemp = 30;
  observeOwner();
  assert(epoch == oldEpoch + 1 && (owner & 32) && !weatherAllowed());
  replies.push_back(reply(200, weatherJson, true)); finishWeather();
  fakeNow += 1001; tickStandaloneServices();
  assert(!busy && weatherTemp == 30 && calls.size() == before + 1);
  for (unsigned i = 0; i < 3; ++i) {
    fakeNow += 1000; tickStandaloneServices();
    assert(!busy && weatherTemp == 30 && !weatherAllowed());
  }
  assert(calls.size() == before + 1);

  // At lease expiry, PC traffic can continue while one native fetch resumes.
  fakeNow = weatherLeaseAt + WEATHER_POLL_INTERVAL_MS;
  tickStandaloneServices();
  assert(!(owner & 32) && busy && job.kind == Weather && hostActive);
  tickStandaloneServices(); assert(jobs->bytes.size() == 1);
  replies.push_back(reply(200, weatherJson, true)); finishWeather();
  fakeNow += 1001; tickStandaloneServices();
  assert(!busy && weatherTemp == -2 && calls.size() == before + 2);
  weatherActive = hostActive = playbackActive = false;
  observeOwner();
}

static void testControlRouting() {
  // Emulate the parent's routing contract, not its transport implementation.
  // A true acknowledgement is final: no host packet, even on local rejection.
  unsigned hostForwards = 0;
  auto route = [&](const String& command) {
    bool owned = queueStandaloneCommand(command);
    if (!owned) ++hostForwards;
    return owned;
  };
  struct OwnerCase { bool wifi, host, playback, android, paused, local; };
  const OwnerCase cases[] = {
    {true, true, true, false, false, false},   // PC playback lease owns controls.
    {true, true, true, true, false, true},     // Android uses native Spotify.
    {true, false, false, false, false, true},  // No host: standalone.
    {true, true, false, false, false, true},   // Non-playback host lease is separate.
    {false, false, false, false, false, false},
    {true, false, false, false, true, false}
  };
  spotifyAccessToken = "dummy-access";
  size_t networkCalls = calls.size();
  for (const OwnerCase& ownerCase : cases) {
    wifiConnected = ownerCase.wifi; hostActive = ownerCase.host;
    playbackActive = ownerCase.playback; isHostAndroid = ownerCase.android;
    dhNetPaused = ownerCase.paused;
    observeOwner(); pendingCount = pendingHead = 0; spotifyGate = Gate();
    unsigned before = hostForwards;
    bool expectedLocal = ENABLE_SPOTIFY && ownerCase.local;
    assert(route("PLAY_PAUSE") == expectedLocal);
    assert(hostForwards == before + (expectedLocal ? 0 : 1));
    assert(pendingCount == (expectedLocal ? 1 : 0));
    if (expectedLocal) assert(standaloneSpotifyStatus == "Spotify command queued");
  }
  assert(calls.size() == networkCalls);  // Routing never performs HTTPS on main.

#if ENABLE_SPOTIFY
  wifiConnected = hostActive = playbackActive = isHostAndroid = true;
  dhNetPaused = false; observeOwner(); spotifyGate = Gate();
  pendingCount = pendingHead = 0;
  busy = true;  // An in-flight job allows only the four bounded pending intents.
  unsigned before = hostForwards;
  assert(route("PLAY_PAUSE")); assert(route("PREV"));
  assert(route("NEXT")); assert(route("SHUFFLE:TOGGLE"));
  assert(pendingCount == 4);
  assert(route("PLAY_PAUSE") && pendingCount == 4);
  assert(standaloneSpotifyStatus == "Spotify queue busy");
  assert(hostForwards == before);  // Queue-full rejection must not double-toggle.

  pendingCount = pendingHead = 0;
  spotifyGate.defer(fakeNow, 60000);
  assert(route("PLAY_PAUSE") && pendingCount == 0);
  assert(standaloneSpotifyStatus == "Spotify cooling down" && hostForwards == before);

  spotifyGate = Gate();
  spotifyAccessToken = spotifyRefreshToken = spotifyClientId = "";
  assert(route("PLAY_PAUSE") && pendingCount == 0);
  assert(standaloneSpotifyStatus == "Spotify not configured" && hostForwards == before);

  spotifyAccessToken = "dummy-access";
  assert(route("PLAY_PAUSE") && pendingCount == 1);
  isHostAndroid = false; observeOwner();  // PC takeover cancels pending local intent.
  assert(pendingCount == 0 && hostForwards == before);
  assert(!route("PLAY_PAUSE") && hostForwards == before + 1);
  busy = false;
#endif
  assert(calls.size() == networkCalls);
  unsigned beforeUnsupported = hostForwards;
  assert(!route("POMO:START") && hostForwards == beforeUnsupported + 1);  // Not this service's command.
  pendingCount = pendingHead = 0;
  wifiConnected = true; hostActive = playbackActive = isHostAndroid = dhNetPaused = false;
  spotifyAccessToken = spotifyRefreshToken = spotifyClientId = "";
  observeOwner();
}

#if ENABLE_SPOTIFY
static void testAuthorizationAndPersistence() {
  wifiConnected = true; dhNetPaused = hostActive = playbackActive = isHostAndroid = false;
  observeOwner(); pcIPStr = "192.168.1.20"; authRequests = AuthRequestLimiter();
  size_t initial = udp.sent.size();
  assert(requestAuthorization(false));
  assert(udp.sent.size() == initial + 1 && udp.sent.back().body == "SPOTIFY_AUTH_REQUEST");
  assert(udp.sent.back().address[0] == 192 && udp.sent.back().address[3] == 20 && udp.sent.back().port == UDP_PORT_TX);
  assert(!requestAuthorization(false) && !requestAuthorization(true));
  fakeNow += 15000;
  assert(requestStandaloneSpotifyAuthorization());  // Explicit SELECT bypasses automatic five-minute cooldown.
  assert(!requestStandaloneSpotifyAuthorization());
  fakeNow += 300000; assert(requestAuthorization(false));
  authRequests = AuthRequestLimiter(); pcIPStr = "255.255.255.255"; assert(!requestAuthorization(true));
  pcIPStr = "224.0.0.1"; assert(!requestAuthorization(true));
  pcIPStr = ""; assert(!requestAuthorization(true));
  pcIPStr = "192.168.1.20"; wifiConnected = false; assert(!requestAuthorization(true)); wifiConnected = true;

  // Auth-required results and an unconfigured tick request once, not every loop.
  authRequests = AuthRequestLimiter();
  spotifyAccessToken = spotifyRefreshToken = spotifyClientId = spotifyClientSecret = "";
  busy = true;  // Tick must not enqueue unrelated jobs in this fixture.
  initial = udp.sent.size(); tickStandaloneServices(); tickStandaloneServices();
  assert(udp.sent.size() == initial + 1);
  fakeNow += 15000;
  assert(queueStandaloneCommand("PLAY_PAUSE") && udp.sent.size() == initial + 2 && pendingCount == 0);
  busy = false;

  // Only complete, bounded refresh-capable tuples persist. Never overwrite a
  // good stored record with an incomplete/corrupt update or a failed NVS write.
  mock_nvs::blob.clear(); mock_nvs::writes = 0; savedCredentialsKnown = false;
  assert(!persistCredentials() && mock_nvs::writes == 0);
  spotifyAccessToken = "dummy-access"; spotifyRefreshToken = "dummy-refresh"; spotifyClientId = "dummy-client";
  uint32_t oldEpoch = epoch;
  spotifyGate.defer(fakeNow, 60000); coverGate.defer(fakeNow, 60000);
  notifyStandaloneCredentialsChanged();
  assert(epoch == oldEpoch + 1 && spotifyGate.ready(fakeNow) && coverGate.ready(fakeNow));
  assert(spotifyDue && mock_nvs::writes == 1);
  notifyStandaloneCredentialsChanged(); assert(mock_nvs::writes == 1);
  std::vector<uint8_t> goodBlob = mock_nvs::blob;
  spotifyRefreshToken = "bad\nrefresh"; assert(!persistCredentials() && mock_nvs::blob == goodBlob);
  spotifyRefreshToken = "rotated-dummy"; mock_nvs::failWrite = true;
  assert(!persistCredentials() && mock_nvs::blob == goodBlob); mock_nvs::failWrite = false;

  // A successful token result stores its rotated refresh token on main.
  spotifyRefreshToken = "dummy-refresh";
  assert(dispatch(Playback, None, fakeNow));
  uint8_t ticket; assert(xQueueReceive(jobs, &ticket, 0));
  resetResult(); result.status = 204; result.valid = result.tokenChanged = true;
  copy(result.access, "rotated-access"); copy(result.refresh, "rotated-dummy");
  assert(xQueueSend(results, &ticket, 0)); consume(fakeNow);
  assert(mock_nvs::writes == 2 && spotifyRefreshToken == "rotated-dummy");
  spotifyAccessToken = spotifyRefreshToken = spotifyClientId = "";
  credentialsLoaded = false; savedCredentialsKnown = false; setupStandaloneServices();
  assert(spotifyAccessToken == "rotated-access" && spotifyRefreshToken == "rotated-dummy" && spotifyClientId == "dummy-client");
  spotifyAccessToken = "live-newer"; setupStandaloneServices(); assert(spotifyAccessToken == "live-newer");
  mock_nvs::blob.back() ^= 1; credentialsLoaded = false; setupStandaloneServices();
  assert(spotifyAccessToken == "live-newer");  // Corrupt record never replaces live globals.
  pcIPStr = "";
}

static void testCoverMemoryOwnership() {
  wifiConnected = true; dhNetPaused = hostActive = playbackActive = isHostAndroid = false;
  spotifyAccessToken = "dummy-access";
  currentState = STATE_SPOTIFY; observeOwner();
  currentTrack = nativeTrack = "test"; currentArtist = nativeArtist = "artist";
  itemId = "spotify:track:memory"; coverUrl = "https://example.test/cover";
  receivingArt = false; albumArtReady = false;
  auto complete = [&]() {
    assert(dispatch(Cover, None, fakeNow));
    uint8_t ticket; assert(xQueueReceive(jobs, &ticket, 0));
    resetResult(); result.status = 200; result.valid = true;
    result.pixels.reset(static_cast<uint16_t*>(calloc(4096, sizeof(uint16_t))));
    result.pixels.get()[0] = 0x1234;
    assert(xQueueSend(results, &ticket, 0));
  };
  complete();
  uint16_t* handedOff = result.pixels.get();
  consume(fakeNow);
  assert(albumArt == handedOff && galleryImage == handedOff && albumArtReady);
  assert(!result.pixels && albumArt[0] == 0x1234); // No second copy/allocation.
  complete(); currentState = STATE_CLOCK_DASHBOARD;
  consume(fakeNow);
  assert(!result.pixels && albumArt == handedOff); // Off-screen result discarded.
  free(albumArt); albumArt = galleryImage = nullptr;
  albumArtReady = false; currentState = STATE_EYES;
}

static void testNativeLyrics() {
  wifiConnected = true; dhNetPaused = hostActive = playbackActive = isHostAndroid = false;
  currentState = STATE_SPOTIFY; observeOwner();
  spotifyAccessToken = "dummy-access";
  currentTrack = nativeTrack = "A & B"; currentArtist = nativeArtist = "Artist / Name";
  itemId = "spotify:track:lyrics"; albumName = "Album + edition";
  playDuration = 12000; playProgress = 0; isPlaying = true; spotifyProgressUpdatedAt = fakeNow;
  standaloneSpotifyStatus = "";
  releaseLyrics();
  auto complete = [&](MockReply response) {
    assert(dispatch(Lyrics, None, fakeNow));
    assert(standaloneSpotifyStatus.empty()); // No polling/loading label.
    uint8_t ticket; assert(xQueueReceive(jobs, &ticket, 0));
    resetResult(); replies.push_back(response); lyrics();
    assert(calls.back().auth.empty()); // Spotify credentials never leave Spotify.
    assert(calls.back().url.find("track_name=A%20%26%20B") != std::string::npos);
    assert(calls.back().url.find("artist_name=Artist%20%2F%20Name") != std::string::npos);
    assert(xQueueSend(results, &ticket, 0));
  };
  const char* synced = R"({"instrumental":false,"syncedLyrics":"[00:01.00]First verse\n[00:04.00]Second verse\n[00:08.00]Third verse","plainLyrics":"Unused plain text"})";
  complete(reply(200, synced, true));
  assert(result.valid && result.lyrics && result.lyricsSynced);
  char* owned = result.lyrics.get(); consume(fakeNow);
  assert(lyricCache.get() == owned && !result.lyrics && standaloneLyricsSynced);
  assert(currentLyrics.empty() && currentLyricsLine2 == "First verse");
  tickLyrics(fakeNow + 2000); assert(currentLyrics == "First verse" && currentLyricsLine2 == "Second verse");
  isPlaying = false; playProgress = 2000; tickLyrics(fakeNow + 10000); assert(currentLyrics == "First verse");
  playProgress = 9000; tickLyrics(fakeNow); assert(currentLyrics == "Third verse");
  playProgress = 0; tickLyrics(fakeNow); assert(currentLyrics.empty() && currentLyricsLine2 == "First verse");
  for (unsigned i = 0; i < 1000; ++i) tickLyrics(fakeNow);
  assert(lyricCache.get() == owned);
  currentState = STATE_CLOCK_DASHBOARD; tickLyrics(fakeNow);
  assert(!lyricCache && lyricCompletedId.empty() && currentLyricsLine2.empty());
  currentState = STATE_SPOTIFY;

  complete(reply(200, synced)); ++mediaEpoch; consume(fakeNow);
  assert(!lyricCache && !result.lyrics && lyricCompletedId.empty()); // Late track result discarded.
  complete(reply(200, "{\"syncedLyrics\":\"[00:01]\\u4e00\\n[00:02]Verse\",\"plainLyrics\":\"Plain fallback\"}"));
  assert(result.valid && result.lyricsSynced); consume(fakeNow);
  assert(standaloneLyricsSynced && currentLyricsLine2 == "\xE4\xB8\x80");
  releaseLyrics();
  complete(reply(200, "{\"syncedLyrics\":\"[00:01]\\n[00:02]Verse\",\"plainLyrics\":\"Plain fallback\"}"));
  assert(result.valid && result.lyricsSynced); consume(fakeNow);
  assert(standaloneLyricsSynced && currentLyrics.empty() && currentLyricsLine2.empty());
  playProgress = 2000; tickLyrics(fakeNow, true); assert(currentLyrics == "Verse");
  playProgress = 0; releaseLyrics();
  complete(reply(404)); consume(fakeNow);
  assert(lyricWithoutAlbum && lyricCompletedId.empty() && standaloneLyricsStatus.empty());
  complete(reply(404));
  assert(calls.back().url.find("album_name") == std::string::npos);
  consume(fakeNow);
  assert(lyricCompletedId == itemId && standaloneLyricsStatus == "Lyrics unavailable");
  assert(standaloneSpotifyStatus.empty());

  releaseLyrics();
  complete(reply(200, "{\"instrumental\":true}")); consume(fakeNow);
  assert(!lyricCache && standaloneLyricsStatus == "Instrumental");
  releaseLyrics();
  complete(reply(200, "{\"syncedLyrics\":null,\"plainLyrics\":\"Plain first\\nPlain second\"}")); consume(fakeNow);
  assert(lyricCache && !standaloneLyricsSynced && currentLyrics == "Plain first");
  tickLyrics(fakeNow + 6000); assert(currentLyrics == "Plain second");
  releaseLyrics();
  complete(reply(503)); consume(fakeNow);
  assert(!lyricCache && lyricGate.wait >= 30000 && standaloneSpotifyStatus.empty());
  releaseLyrics();
  complete(reply(200, "{\"plainLyrics\":\"" + std::string(8193, 'x') + "\"}"));
  assert(!result.valid && (result.status == TooLarge || result.status == NoMemory) && !result.lyrics); consume(fakeNow);
  releaseLyrics();
  complete(reply(200, "{broken")); assert(!result.valid && result.status == InvalidJson); consume(fakeNow);
  releaseLyrics();
  cleanHttp(); currentState = STATE_EYES;
  hostActive = playbackActive = true;
  currentLyrics = "Host line"; standaloneLyricsSynced = false;
  standaloneLyricsStatus = "Host status";
  companion_lyrics::timing().next = 3200;
  tickLyrics(fakeNow);
  assert(currentLyrics == "Host line" && !standaloneLyricsSynced);
  assert(standaloneLyricsStatus == "Host status" && companion_lyrics::timing().next == 3200);
  hostActive = playbackActive = false;
}
#endif

int main() {
  job.kind = Weather;
  replies.push_back(reply(200, weatherJson, true));
  weather();
  assert(result.valid && result.temp == -2 && result.wmo == 71);
  assert(calls.back().accept == "identity" && bodyDecodes == 1);
  cleanHttp();

  fakeTlsError = -32512;
  replies.push_back(reply(-1));
  assert(request("https://example.test/data", "GET", "", "", 8).status == NoMemory);
  assert(Serial.output.find("RAM before=") != std::string::npos);
  cleanHttp();
  // Reproduce the device's exact heap: this must attempt TLS, not self-defer.
  fakeFreeHeap = 65920; fakeLargestBlock = 38900;
  replies.push_back(reply(200, "{}"));
  assert(request("https://example.test/data", "GET", "", "", 8).status == 200);
  assert(calls.size() == 1);
  cleanHttp();
  fakeFreeHeap = 63 * 1024; fakeLargestBlock = 39 * 1024;
  replies.push_back(reply(200, "{}"));
  assert(request("https://example.test/data", "GET", "", "", 8).status == 200);
  assert(calls.size() == 1);
  cleanHttp();
  fakeFreeHeap = 120 * 1024; fakeLargestBlock = 90 * 1024;

  replies.push_back(reply(200, "123456789", true));
  Response response = request("https://example.test/data", "GET", "", "", 8);
  assert(response.status == TooLarge && response.size <= 8);
  cleanHttp();
  MockReply oversized = reply(200, "{}"), compressed = oversized;
  oversized.size = 99; compressed.headers["Content-Encoding"] = "deflate";
  replies.push_back(oversized);
  assert(request("https://example.test/data", "GET", "", "", 8).status == TooLarge);
  replies.push_back(compressed);
  assert(request("https://example.test/data", "GET", "", "", 8).status == Encoding);
  MockReply stalled = reply(200); stalled.stall = true;
  replies.push_back(stalled);
  assert(request("https://example.test/data", "GET", "", "", 8).status == TimedOut);
  MockReply truncated = reply(200, "5\r\nxy", true); truncated.body = "5\r\nxy";
  replies.push_back(truncated);
  assert(request("https://example.test/data", "GET", "", "", 8).status < 0);
  cleanHttp();

  MockReply moved = reply(302); moved.headers["Location"] = "https://cdn.test/image";
  replies.push_back(moved); replies.push_back(reply(200, "jpeg", true));
  assert(request("https://example.test/data", "GET", "", "", 8).status == 200);
  assert(calls.size() == 2 && calls[1].url == "https://cdn.test/image"); cleanHttp();
  replies.push_back(moved);
  assert(request("https://example.test/data", "GET", "Bearer dummy-access", "", 8).status == UnsafeRedirect);
  assert(calls.size() == 1); cleanHttp();
  moved.headers["Location"] = "http://example.test/data"; replies.push_back(moved);
  assert(request("https://example.test/data", "GET", "", "", 8).status == UnsafeRedirect); cleanHttp();
  replies.push_back(moved);
  assert(request("https://example.test/data", "POST", "", "", 8).status == 302);
  assert(calls.size() == 1); cleanHttp();
  fakeTlsError = -123;
  replies.push_back(reply(-1));
  assert(request("https://example.test/data", "GET", "Bearer dummy-access", "", 8).status == TlsFailure);
  cleanHttp();
  fakeTlsError = -1;
  replies.push_back(reply(-1));
  assert(request("https://example.test/data", "GET", "", "", 8).status == -1);
  cleanHttp();

#if ENABLE_SPOTIFY
  job.kind = Playback;
  copy(job.access, "dummy-access"); copy(job.refresh, "dummy+refresh&"); copy(job.clientId, "dummy-id");
  replies.push_back(reply(400, R"({"error":"invalid_client","error_description":"DO-NOT-LOG-THIS"})", true));
  String refreshedAccess;
  assert(!refresh(refreshedAccess) && result.status == AuthInvalidClient);
  assert(calls.size() == 1 && calls[0].auth.empty() && calls[0].body.find("client_id=dummy-id") != std::string::npos);
  cleanHttp();
  replies.push_back(reply(400, R"({"error":"invalid_grant"})", true));
  assert(!refresh(refreshedAccess) && result.status == AuthSignInRequired); cleanHttp();
  replies.push_back(reply(401));
  replies.push_back(reply(200, R"({"access_token":"new-dummy","refresh_token":"rotated-dummy"})", true));
  replies.push_back(reply(200, R"({"is_playing":false,"shuffle_state":false,"item":null})", true));
  String access = job.access; bool refreshed = false;
  assert(readPlayback(access, refreshed) && refreshed && result.tokenChanged && access == "new-dummy");
  assert(calls.size() == 3 && calls[1].method == "POST");
  assert(calls[1].body.find("dummy%2Brefresh%26") != std::string::npos);
  assert(calls[2].auth == "Bearer new-dummy"); cleanHttp();

  replies.push_back(reply(401)); replies.push_back(reply(200, R"({"access_token":"new-dummy"})", true));
  replies.push_back(reply(401)); access = job.access; refreshed = false;
  assert(!readPlayback(access, refreshed) && result.status == 401 && calls.size() == 3); cleanHttp();

  job.kind = Control; job.command = PlayPause;
  std::string hugeMetadata = "{\"is_playing\":false,\"shuffle_state\":true,\"item\":{\"name\":\"" + std::string(10000, 'x') + "\"}}";
  replies.push_back(reply(200, hugeMetadata, true)); replies.push_back(reply(204));
  control(); assert(result.valid && calls.size() == 2 && calls.back().url.find("/play") != std::string::npos); cleanHttp();
  replies.push_back(reply(200, "{\"is_playing\":true,\"shuffle_state\":false}", true));
  replies.push_back(reply(-11)); control(); assert(!result.valid && calls.size() == 2); cleanHttp();

  job.kind = Playback;
  std::string longPlayback = "{\"is_playing\":true,\"item\":{\"uri\":\"spotify:track:test\",\"name\":\"" + std::string(600, 'x') + "\",\"artists\":[{\"name\":\"test\"}],\"duration_ms\":5000}}";
  replies.push_back(reply(200, longPlayback, true)); access = job.access; refreshed = false;
  assert(readPlayback(access, refreshed) && strlen(result.track) == 255); cleanHttp();

  wifiConnected = true; observeOwner();
  spotifyAccessToken = "dummy-access";
  currentTrack = std::string(1200, 'a'); currentArtist = std::string(600, 'b');
  assert(dispatch(Playback, None, fakeNow));
  assert(mediaMatches());
  currentTrack[1100] = 'c'; assert(!mediaMatches());
  uint8_t ticket; assert(xQueueReceive(jobs, &ticket, 0)); busy = false;
  spotifyAccessToken = spotifyRefreshToken = spotifyClientId = "";
  unsigned count = calls.size();
  assert(queueStandaloneCommand("PLAY_PAUSE") && !pendingCount && calls.size() == count);
  assert(standaloneSpotifyStatus == "Spotify not configured");
#endif

  // Main result application, timed errors, reconnect reset, then successful weather.
  currentState = STATE_WEATHER;
  wifiConnected = true; observeOwner();
  if (!task) assert(start(fakeNow));
  job.kind = Weather; job.epoch = epoch; busy = true;
  resetResult(); result.status = 429; result.retryMs = 60000;
  uint8_t done = 1; assert(xQueueSend(results, &done, 0));
  fakeNow += 1001; tickStandaloneServices();
  assert(!busy && standaloneWeatherStatus.find("retry 60s") != std::string::npos);
  wifiConnected = false; fakeNow += 1001; tickStandaloneServices();
  assert(standaloneWeatherStatus == "Offline");
  wifiConnected = true; fakeNow += 1001; tickStandaloneServices();
  assert(busy && job.kind == Weather && weatherGate.ready(fakeNow));
  replies.push_back(reply(200, weatherJson, true)); finishWeather();
  fakeNow += 1001; tickStandaloneServices();
  assert(weatherTemp == -2 && weatherCode == 600 && standaloneWeatherStatus.empty());
  assert(!busy && calls.back().accept == "identity");
  testWeatherOwnership();
  testControlRouting();
  testDemandAndPriority();
#if ENABLE_SPOTIFY
  testAuthorizationAndPersistence();
  testCoverMemoryOwnership();
  testNativeLyrics();
#endif
  assert(Serial.output.find("dummy-access") == std::string::npos);
  assert(Serial.output.find("dummy-refresh") == std::string::npos);
  assert(Serial.output.find("https://") == std::string::npos);
  assert(Serial.output.find("DO-NOT-LOG-THIS") == std::string::npos);
  assert(replies.empty());
  std::cout << "standalone request/state tests passed (Spotify=" << ENABLE_SPOTIFY << ")\n";
}
