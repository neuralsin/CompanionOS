#ifndef STANDALONE_SERVICES_H
#define STANDALONE_SERVICES_H

// Include once, in companion_net.h after the public OAuth globals and encodeBase64.
// Both public entry points must be called only from the Arduino/main task.
#include <Arduino.h>

// Public, main-task-only status for the parent's lyrics/status fallback.
String standaloneSpotifyStatus;
String standaloneWeatherStatus;

#ifdef ESP32
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <memory>
#include <new>
#include <math.h>
#include <stdlib.h>
#include "standalone_services_detail.h"
#if ENABLE_SPOTIFY
#include <TJpg_Decoder.h>
#include <Preferences.h>
#include <utility>
#if JD_FORMAT != 1
#error Standalone Spotify requires TJpg_Decoder RGB565 output (JD_FORMAT=1)
#endif
extern unsigned long spotifyProgressUpdatedAt;
extern bool t2_shuffle;
extern uint8_t t2_volume;
extern uint8_t t2_repeat;
extern char t2_device[24];
extern void t2_redrawSpotifyPartial();
extern void t2_resetSpotifyDrawState();
extern void resetSpotifyDrawState();
#endif

namespace standalone_services {
using namespace standalone_detail;

enum Kind : uint8_t { Weather, Playback, Control, Cover };
struct Job {
  Kind kind = Weather;
  Command command = None;
  uint32_t epoch = 0;
  uint32_t mediaEpoch = 0;
  uint32_t artRevision = 0;
  bool hostActive = false;
  bool hostAndroid = false;
  bool spotifyPage = false;
  bool playing = false;
  char access[2048] = {};
  char refresh[1024] = {};
  char clientId[128] = {};
  char secret[256] = {};
  char track[256] = {};
  char artist[256] = {};
  uint64_t trackFingerprint = 0, artistFingerprint = 0;
  char itemId[256] = {};
  char url[512] = {};
};
struct Result {
  int status = -1;
  uint32_t retryMs = 0;
  uint32_t sampledAt = 0;
  bool valid = false;
  bool tokenChanged = false;
  char access[2048] = {};
  char refresh[1024] = {};
  bool playing = false;
  bool shuffle = false;
  int progress = 0;
  int duration = 0;
  uint8_t volume = 100;
  uint8_t repeat = 0;
  char device[24] = {};
  char track[256] = {};
  char artist[256] = {};
  char itemId[256] = {};
  char url[512] = {};
  int temp = 0, feels = 0, humidity = 0, wind = 0, high = 0, low = 0, wmo = 0;
  char sunrise[6] = {}, sunset[6] = {};
#if ENABLE_SPOTIFY
  uint16_t pixels[64 * 64] = {};
#endif
};

// Queue operations transfer exclusive ownership of this slot. Neither side touches
// it after publishing until the other side returns it. No String crosses tasks.
static Job job;
static Result result;
static QueueHandle_t jobs = nullptr, results = nullptr;
static TaskHandle_t task = nullptr;
static bool busy = false;
static Gate initGate, spotifyGate, weatherGate, coverGate;
static uint8_t spotifyFailures = 0, weatherFailures = 0, coverFailures = 0;
static uint32_t epoch = 0, mediaEpoch = 0;
static uint8_t owner = 255;
static bool spotifyDue = true;
#if ENABLE_SPOTIFY
static uint32_t lastPoll = 0;
#endif
static int spotifyError = 0, weatherError = 0;
#if ENABLE_SPOTIFY
static AuthRequestLimiter authRequests;
static bool credentialsLoaded = false;
static bool savedCredentialsKnown = false;
static bool credentialsSaveFailed = false;
static uint32_t savedCredentialChecksum = 0;

// Main-only NVS access. One versioned/checksummed blob avoids mixed credential
// fields after an interrupted update. NVS confidentiality requires flash encryption.
static void loadCredentialsOnce() {
  if (credentialsLoaded) return;
  credentialsLoaded = true;
  std::unique_ptr<CredentialRecord> record(new (std::nothrow) CredentialRecord());
  if (!record) return;
  Preferences prefs;
  if (prefs.begin("spotify-svc", true)) {
    bool valid = prefs.getBytesLength("creds-v1") == sizeof(*record) &&
                 prefs.getBytes("creds-v1", record.get(), sizeof(*record)) == sizeof(*record) &&
                 validCredentialRecord(*record);
    prefs.end();
    if (valid) {
      String access(record->access), refresh(record->refresh), clientId(record->clientId), secret(record->secret);
      if (access.length() == strlen(record->access) && refresh.length() == strlen(record->refresh) &&
          clientId.length() == strlen(record->clientId) && secret.length() == strlen(record->secret)) {
        spotifyAccessToken = std::move(access); spotifyRefreshToken = std::move(refresh);
        spotifyClientId = std::move(clientId); spotifyClientSecret = std::move(secret);
        savedCredentialChecksum = record->checksum;
        savedCredentialsKnown = true;
      }
    }
  }
  eraseCredentialRecord(*record);
}
static bool persistCredentials() {
  std::unique_ptr<CredentialRecord> record(new (std::nothrow) CredentialRecord());
  if (!record) return false;
  bool complete = copy(record->access, spotifyAccessToken.c_str()) && copy(record->refresh, spotifyRefreshToken.c_str()) &&
                  copy(record->clientId, spotifyClientId.c_str()) && copy(record->secret, spotifyClientSecret.c_str());
  record->checksum = credentialChecksum(*record);
  bool saved = false;
  if (complete && validCredentialRecord(*record)) {
    if (savedCredentialsKnown && savedCredentialChecksum == record->checksum) saved = true;
    else {
      Preferences prefs;
      if (prefs.begin("spotify-svc", false)) {
        saved = prefs.putBytes("creds-v1", record.get(), sizeof(*record)) == sizeof(*record);
        prefs.end();
        if (saved) { savedCredentialChecksum = record->checksum; savedCredentialsKnown = true; }
      }
    }
  }
  eraseCredentialRecord(*record);
  return saved;
}

static bool authFailure(int code) {
  return code == AuthInvalidClient || code == AuthSignInRequired || code == 401 || code == NotConfigured;
}
static bool requestAuthorization(bool userAction) {
  uint32_t now = millis();
  if (!wifiConnected || dhNetPaused || !authRequests.ready(now, userAction)) return false;
  IPAddress controller;
  if (!controller.fromString(pcIPStr) || !controller[0] || controller[0] == 127 ||
      controller[0] >= 224 || controller[3] == 255) return false;
  authRequests.attempted(now);  // Bound attempts too, including UDP send failure.
  static const char packet[] = "SPOTIFY_AUTH_REQUEST";
  if (!udp.beginPacket(controller, UDP_PORT_TX)) return false;
  size_t written = udp.write(reinterpret_cast<const uint8_t*>(packet), sizeof(packet) - 1);
  bool sent = udp.endPacket() == 1 && written == sizeof(packet) - 1;
  if (sent) standaloneSpotifyStatus = "Spotify sign-in requested";
  return sent;
}
#endif
static String itemId, coverUrl, nativeTrack, nativeArtist, coveredId;
struct Pending { Command command = None; uint32_t at = 0; };
static Pending pending[4];
static uint8_t pendingHead = 0, pendingCount = 0;

struct Response {
  int status = -1;
  uint32_t retryMs = 0;
  size_t size = 0;
  std::unique_ptr<uint8_t[]> body;
};

#if ARDUINOJSON_VERSION_MAJOR >= 7
// v7 ignores DynamicJsonDocument(capacity). Enforce a real allocation budget.
class JsonBudget : public ArduinoJson::Allocator {
  struct alignas(max_align_t) Block { size_t size; };
  size_t used = 0;
  static constexpr size_t limit = 8192;
 public:
  void* allocate(size_t size) override {
    if (size > limit - used) return nullptr;
    Block* block = static_cast<Block*>(malloc(sizeof(Block) + size));
    if (!block) return nullptr;
    block->size = size; used += size;
    return block + 1;
  }
  void deallocate(void* pointer) override {
    if (!pointer) return;
    Block* block = static_cast<Block*>(pointer) - 1;
    used -= block->size; free(block);
  }
  void* reallocate(void* pointer, size_t size) override {
    if (!pointer) return allocate(size);
    if (!size) { deallocate(pointer); return nullptr; }
    Block* block = static_cast<Block*>(pointer) - 1;
    size_t old = block->size;
    if (size > limit - (used - old)) return nullptr;
    Block* resized = static_cast<Block*>(realloc(block, sizeof(Block) + size));
    if (!resized) return nullptr;
    resized->size = size; used = used - old + size;
    return resized + 1;
  }
};
struct Document {
  JsonBudget allocator;
  JsonDocument value;
  Document() : value(&allocator) {}
};
#else
struct Document { DynamicJsonDocument value; Document() : value(8192) {} };
#endif

class BodySink : public Stream {
 public:
  BoundedBody buffer;
  BodySink(uint8_t* bytes, size_t cap) : buffer(bytes, cap, millis()) {}
  size_t write(uint8_t byte) override { return write(&byte, 1); }
  size_t write(const uint8_t* bytes, size_t size) override {
    size_t written = buffer.append(bytes, size, millis());
    if (buffer.error) setWriteError();
    return written;
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}
};

// HTTPClient's close-delimited read loop has no inactivity timeout on this core.
// Stop it from the same worker (no cross-task TLS teardown), even without writes.
class DeadlineClient : public WiFiClientSecure {
  uint32_t started = 0;
  bool armed = false;
  bool expired() {
    if (armed && uint32_t(millis() - started) >= 25000) {
      timedOut = true; armed = false; WiFiClientSecure::stop();
    }
    return timedOut;
  }
 public:
  bool timedOut = false;
  void arm() { started = millis(); armed = true; }
  int available() override { return expired() ? 0 : WiFiClientSecure::available(); }
  uint8_t connected() override { return expired() ? 0 : WiFiClientSecure::connected(); }
  int read() override { return expired() ? -1 : WiFiClientSecure::read(); }
  int read(uint8_t* bytes, size_t size) override {
    return expired() ? -1 : WiFiClientSecure::read(bytes, size);
  }
};

static const char* serviceName() {
  return job.kind == Weather ? "weather" : (job.kind == Cover ? "cover" : "spotify");
}
static void logHttpFailure(HTTPClient& http, int code, int tlsError = 0, const char* tlsDesc = nullptr) {
  // Only constant labels, numeric codes, and native error descriptions.
  if (code < 0 && code > -1000) {
    if (tlsError && tlsDesc && *tlsDesc) {
      Serial.printf("[standalone:%s] transport %d: %s (TLS %d: %s)\n", serviceName(), code,
                    http.errorToString(code).c_str(), tlsError, tlsDesc);
    } else {
      Serial.printf("[standalone:%s] transport %d: %s (TLS %d)\n", serviceName(), code,
                    http.errorToString(code).c_str(), tlsError);
    }
  } else if (code >= 300) {
    Serial.printf("[standalone:%s] HTTP %d\n", serviceName(), code);
  } else {
    Serial.printf("[standalone:%s] %s\n", serviceName(), failureName(code));
  }
}

// Worker-only: HTTPClient removes chunk framing before writing to the capped
// sink. Redirects are explicit, GET-only, HTTPS-only, and bounded to three hops;
// an authenticated GET may never carry its bearer token to a different origin.
static Response request(const String& url, const char* method, const String& auth,
                        const String& body, size_t cap, bool form = false) {
  Response response;
  String target = url;
  for (unsigned hop = 0; hop <= 3; ++hop) {
    DeadlineClient client;
    client.setInsecure();  // Existing debt: server certificates are NOT verified.
    client.setHandshakeTimeout(15);
    HTTPClient http;
    http.useHTTP10(false);
    http.setReuse(false);
    http.setConnectTimeout(8000);
    http.setTimeout(8000);
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    if (!httpsOriginLength(target.c_str())) { response.status = UnsafeRedirect; break; }
    int hostEnd = target.indexOf('/', 8);
    String host = hostEnd > 0 ? target.substring(8, hostEnd) : target.substring(8);
    int colonIdx = host.indexOf(':');
    if (colonIdx > 0) host = host.substring(0, colonIdx);
    IPAddress resolvedIP;
    if (!WiFi.hostByName(host.c_str(), resolvedIP)) {
      Serial.printf("[standalone:%s] DNS failed for %s\n", serviceName(), host.c_str());
    }
    if (!http.begin(client, target)) { response.status = -1; logHttpFailure(http, response.status); break; }
    const char* headers[] = {"Retry-After", "Content-Encoding", "Location"};
    http.collectHeaders(headers, 3);
    if (auth.length()) http.addHeader("Authorization", auth);
    http.addHeader("Accept-Encoding", "identity");
    if (form) http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    client.arm();
    response.status = http.sendRequest(method, body);
    bool redirect = response.status == 301 || response.status == 302 || response.status == 303 ||
                    response.status == 307 || response.status == 308;
    if (redirect && !strcmp(method, "GET")) {
      char next[1024];
      if (hop == 3 || !redirectTarget(next, target.c_str(), http.header("Location").c_str(), auth.length() != 0)) {
        response.status = UnsafeRedirect;
      } else {
        target = next; http.end(); continue;
      }
    }
    if (response.status == 429) response.retryMs = retryAfterMs(http.header("Retry-After").c_str());
    int nativeError = response.status < 0 ? response.status : 0;
    if (cap && (response.status == 200 || (form && (response.status == 400 || response.status == 401)))) {
      String encoding = http.header("Content-Encoding");
      encoding.trim(); encoding.toLowerCase();
      if (http.getSize() > int(cap)) response.status = TooLarge;
      else if (encoding.length() && encoding != "identity") response.status = Encoding;
      else {
        response.body.reset(new (std::nothrow) uint8_t[cap + 1]);
        if (!response.body) response.status = NoMemory;
        else {
          BodySink sink(response.body.get(), cap);
          int written = http.writeToStream(&sink);
          response.size = sink.buffer.size;
          response.body[response.size] = 0;
          if (sink.buffer.error) response.status = sink.buffer.error;
          else if (written < 0) { response.status = written; nativeError = written; }
          else if (size_t(written) != response.size ||
                   (http.getSize() >= 0 && size_t(http.getSize()) != response.size)) response.status = InvalidJson;
        }
      }
    }
    char tlsText[80] = {0};
    int tlsError = nativeError ? client.lastError(tlsText, sizeof(tlsText)) : 0;
    if (nativeError) {
      logHttpFailure(http, nativeError, tlsError, tlsText);
      if (nativeError == HTTPC_ERROR_TOO_LESS_RAM) response.status = NoMemory;
      else if (nativeError == HTTPC_ERROR_READ_TIMEOUT) response.status = TimedOut;
      else if (tlsError) response.status = TlsFailure;
    }
    if (client.timedOut) response.status = TimedOut;
    if (response.status < -1000 || response.status >= 300) logHttpFailure(http, response.status, tlsError, tlsText);
    http.end();
    return response;
  }
  return response;
}

static void noteResponse(const Response& response) {
  result.status = response.status;
  if (response.retryMs > result.retryMs) result.retryMs = response.retryMs;
}

#if ENABLE_SPOTIFY
static String formEncode(const char* text) {
  String encoded;
  static const char hex[] = "0123456789ABCDEF";
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p; ++p) {
    unsigned char c = *p;
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') encoded += char(c);
    else { encoded += '%'; encoded += hex[c >> 4]; encoded += hex[c & 15]; }
  }
  return encoded;
}

static bool refresh(String& access) {
  if (!job.refresh[0] || !job.clientId[0]) { result.status = 401; return false; }
  String body = "grant_type=refresh_token&refresh_token=" + formEncode(job.refresh);
  String auth;
  if (job.secret[0]) auth = "Basic " + encodeBase64(String(job.clientId) + ":" + job.secret);
  else body += "&client_id=" + formEncode(job.clientId);  // PKCE/public client.
  Response response = request("https://accounts.spotify.com/api/token", "POST", auth, body, 8192, true);
  noteResponse(response);
  if (!response.body) return false;
  Document document;
  auto& doc = document.value;
  DeserializationError error = deserializeJson(doc, response.body.get(), response.size);
  if (response.status != 200) {
    if (!error) {
      const char* category = doc["error"] | "";
      if (!strcmp(category, "invalid_client")) result.status = AuthInvalidClient;
      else if (!strcmp(category, "invalid_grant")) result.status = AuthSignInRequired;
    }
    return false;
  }
  if (error) { result.status = error == DeserializationError::NoMemory ? NoMemory : InvalidJson; return false; }
  const char* token = doc["access_token"] | "";
  const char* rotated = doc["refresh_token"] | job.refresh;
  if (!*token || !copy(result.access, token) || !copy(result.refresh, rotated)) {
    result.status = InvalidJson; return false;
  }
  access = result.access;
  result.tokenChanged = true;
  return true;
}

static Response spotifyRequest(const String& path, const char* method, String& access,
                               bool& refreshed, size_t cap) {
  Response response;
  if (!access.length()) {
    if (refreshed) { result.status = 401; return response; }
    refreshed = true;
    if (!refresh(access)) return response;
  }
  response = request("https://api.spotify.com/v1/me/player" + path, method, "Bearer " + access, "", cap);
  if (response.status == 401 && !refreshed) {
    refreshed = true;
    response.body.reset();
    if (!refresh(access)) return Response();
    response = request("https://api.spotify.com/v1/me/player" + path, method, "Bearer " + access, "", cap);
  }
  noteResponse(response);
  return response;
}

static bool readPlayback(String& access, bool& refreshed) {
  Response response = spotifyRequest("", "GET", access, refreshed, 24576);
  if (response.status == 204) {
    result.valid = true;
    result.sampledAt = millis();
    return true;
  }
  if (response.status != 200 || !response.body) return false;
  // Filter out market lists and other large unused metadata before allocating it.
  StaticJsonDocument<1024> filter;
  filter["is_playing"] = true;
  filter["shuffle_state"] = true;
  if (job.kind != Control) {
  filter["progress_ms"] = true;
  filter["repeat_state"] = true;
  filter["device"]["name"] = true; filter["device"]["volume_percent"] = true;
  filter["item"]["name"] = true; filter["item"]["uri"] = true;
  filter["item"]["duration_ms"] = true;
  filter["item"]["artists"][0]["name"] = true;
  filter["item"]["album"]["images"][0]["url"] = true;
  filter["item"]["album"]["images"][0]["width"] = true;
  filter["item"]["images"][0]["url"] = true;
  filter["item"]["images"][0]["width"] = true;
  filter["item"]["show"]["publisher"] = true;
  }
  Document document;
  auto& doc = document.value;
  DeserializationError error = deserializeJson(doc, response.body.get(), response.size, DeserializationOption::Filter(filter));
  if (error || !doc["is_playing"].is<bool>()) {
    result.status = error == DeserializationError::NoMemory ? NoMemory : InvalidJson; return false;
  }
  result.sampledAt = millis();
  result.shuffle = doc["shuffle_state"] | false;
  result.playing = doc["is_playing"] | false;
  if (job.kind == Control) { result.valid = true; return true; }
  result.volume = constrain(doc["device"]["volume_percent"] | 100, 0, 100);
  const char* repeat = doc["repeat_state"] | "off";
  result.repeat = !strcmp(repeat, "track") ? 2 : (!strcmp(repeat, "context") ? 1 : 0);
  const char* device = doc["device"]["name"] | "Unknown";
  strncpy(result.device, device, sizeof(result.device) - 1);
  JsonVariant entry = doc["item"];
  if (!entry.isNull()) {
    const char* artist = entry["artists"][0]["name"] | (entry["show"]["publisher"] | "");
    copyDisplay(result.track, entry["name"] | "");
    copyDisplay(result.artist, artist);
    if (!copy(result.itemId, entry["uri"] | "")) { result.status = InvalidJson; return false; }
    result.duration = entry["duration_ms"] | 0;
    result.playing = doc["is_playing"] | false;
    result.progress = progressAt(doc["progress_ms"] | 0, result.duration, false, 0);
    JsonArray images = entry["album"]["images"].as<JsonArray>();
    if (images.isNull()) images = entry["images"].as<JsonArray>();
    int smallest = 0x7fffffff;
    for (JsonObject image : images) {
      int width = image["width"] | 0;
      const char* url = image["url"] | "";
      if (width > 0 && width < smallest && !strncmp(url, "https://", 8) && strlen(url) < sizeof(result.url)) {
        smallest = width; copy(result.url, url);
      }
    }
  } else result.playing = false;
  result.valid = true;
  return true;
}

static void control() {
  String access(job.access);
  bool refreshed = false;
  String path;
  const char* method = "PUT";
  // Query actual state for toggle commands. A stale UI snapshot must not invert
  // the user's intent. This GET shares the same one-refresh budget as the write.
  if (job.command == PlayPause || job.command == Shuffle) {
    if (!readPlayback(access, refreshed)) return;
    if (result.status == 204) { result.valid = false; result.status = 404; return; }
    path = job.command == PlayPause ? (result.playing ? "/pause" : "/play") :
           (result.shuffle ? "/shuffle?state=false" : "/shuffle?state=true");
    result.valid = false;  // Never apply the pre-command sample as the new state.
  } else if (job.command == Next) { path = "/next"; method = "POST"; }
  else if (job.command == Previous) { path = "/previous"; method = "POST"; }
  else return;
  spotifyRequest(path, method, access, refreshed, 0);
  result.valid = result.status >= 200 && result.status < 300;
}

struct DecodeContext {
  const uint8_t* bytes;
  size_t length, offset;
  uint16_t width, height;
  uint16_t* pixels;
  uint32_t started;
};
static size_t jpegInput(JDEC* decoder, uint8_t* dest, size_t count) {
  DecodeContext* c = static_cast<DecodeContext*>(decoder->device);
  if (count > c->length - c->offset) count = c->length - c->offset;
  if (dest) memcpy(dest, c->bytes + c->offset, count);
  c->offset += count;
  return count;
}
static int jpegOutput(JDEC* decoder, void* pixels, JRECT* rect) {
  DecodeContext* c = static_cast<DecodeContext*>(decoder->device);
  if (uint32_t(millis() - c->started) > 3000) return 0;
  sampleBlock(c->pixels, c->width, c->height, rect->left, rect->top,
              rect->right - rect->left + 1, rect->bottom - rect->top + 1,
              static_cast<uint16_t*>(pixels));
  vTaskDelay(1);
  return 1;
}
static void cover() {
  if (strncmp(job.url, "https://", 8)) return;
  Response response = request(job.url, "GET", "", "", 49152);
  noteResponse(response);
  if (response.status != 200 || !response.body) return;
  std::unique_ptr<uint8_t[]> workspace(new (std::nothrow) uint8_t[TJPGD_WORKSPACE_SIZE]);
  if (!workspace) { result.status = NoMemory; return; }
  DecodeContext context = {response.body.get(), response.size, 0, 0, 0, result.pixels, millis()};
  JDEC decoder = {};
  // Native RGB565, matching host art and the existing setSwapBytes(true) draws.
  decoder.swap = 0;
  if (jd_prepare(&decoder, jpegInput, workspace.get(), TJPGD_WORKSPACE_SIZE, &context) != JDR_OK ||
      !decoder.width || !decoder.height || decoder.width > 1024 || decoder.height > 1024) {
    result.status = InvalidJpeg; return;
  }
  uint8_t scale = 0;
  while (scale < 3 && (decoder.width >> (scale + 1)) >= 64 && (decoder.height >> (scale + 1)) >= 64) ++scale;
  context.width = decoder.width >> scale;
  context.height = decoder.height >> scale;
  result.valid = jd_decomp(&decoder, jpegOutput, scale) == JDR_OK;
  if (!result.valid) result.status = InvalidJpeg;
}
#endif  // ENABLE_SPOTIFY

static bool number(JsonVariant value, int& dest, float low, float high) {
  if (!value.is<float>() && !value.is<int>()) return false;
  float n = value.as<float>();
  if (!isfinite(n) || n < low || n > high) return false;
  dest = int(lroundf(n));
  return true;
}
static void sunTime(char (&dest)[6], const char* value) {
  if (!value) return;
  const char* t = strchr(value, 'T');
  if (t && strlen(t + 1) >= 5) { memcpy(dest, t + 1, 5); dest[5] = 0; }
}
static void weather() {
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(WEATHER_LATITUDE, 4) +
      "&longitude=" + String(WEATHER_LONGITUDE, 4) +
      "&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,wind_speed_10m" +
      "&daily=temperature_2m_max,temperature_2m_min,sunrise,sunset&forecast_days=1&timezone=auto";
  Response response = request(url, "GET", "", "", 8192);
  noteResponse(response);
  if (response.status != 200 || !response.body) return;
  Document document;
  auto& doc = document.value;
  DeserializationError error = deserializeJson(doc, response.body.get(), response.size);
  if (error) { result.status = error == DeserializationError::NoMemory ? NoMemory : InvalidJson; return; }
  JsonVariant current = doc["current"];
  JsonVariant daily = doc["daily"];
  result.valid = number(current["temperature_2m"], result.temp, -100, 100) &&
      number(current["apparent_temperature"], result.feels, -150, 150) &&
      number(current["relative_humidity_2m"], result.humidity, 0, 100) &&
      number(current["wind_speed_10m"], result.wind, 0, 500) &&
      number(current["weather_code"], result.wmo, 0, 99) &&
      number(daily["temperature_2m_max"][0], result.high, -100, 100) &&
      number(daily["temperature_2m_min"][0], result.low, -100, 100);
  sunTime(result.sunrise, daily["sunrise"][0] | "");
  sunTime(result.sunset, daily["sunset"][0] | "");
  if (!result.valid) result.status = InvalidJson;
}

static void worker(void*) {
  uint8_t ticket;
  for (;;) {
    if (xQueueReceive(jobs, &ticket, portMAX_DELAY) != pdTRUE) continue;
    // Value initialization is done in-place, not as a large task-stack temporary.
    result.~Result();
    new (&result) Result();
    if (job.kind == Weather) weather();
#if ENABLE_SPOTIFY
    else if (job.kind == Playback) { String access(job.access); bool refreshed = false; readPlayback(access, refreshed); }
    else if (job.kind == Control) control();
    else if (job.kind == Cover) cover();
#endif
    if (!result.valid && result.status <= -1000)
      Serial.printf("[standalone:%s] %s\n", serviceName(), failureName(result.status));
    xQueueSend(results, &ticket, portMAX_DELAY);
  }
}

// Everything below this boundary runs only on main.
static bool start(uint32_t now) {
  if (task) return true;
  if (!initGate.ready(now)) return false;
  jobs = xQueueCreate(1, sizeof(uint8_t));
  results = xQueueCreate(1, sizeof(uint8_t));
  if (jobs && results && xTaskCreate(worker, "standalone", 10240, nullptr, 1, &task) == pdPASS) return true;
  if (jobs) vQueueDelete(jobs);
  if (results) vQueueDelete(results);
  jobs = results = nullptr; task = nullptr;
  initGate.defer(now, 30000);
  return false;
}

static bool spotifyAllowed() {
  return ENABLE_SPOTIFY && wifiConnected && !dhNetPaused && (!isHostPlaybackActive() || isHostAndroid);
}
static bool spotifyConfigured() {
#if ENABLE_SPOTIFY
  return configured(spotifyAccessToken.c_str(), spotifyRefreshToken.c_str(), spotifyClientId.c_str());
#else
  return false;
#endif
}
static bool weatherAllowed() { return wifiConnected && !dhNetPaused && !isHostWeatherActive(); }
static void observeOwner() {
  uint8_t current = (wifiConnected ? 1 : 0) | (isHostActive() ? 2 : 0) |
                    (isHostAndroid ? 4 : 0) | (dhNetPaused ? 8 : 0) | (isHostPlaybackActive() ? 16 : 0) |
                    (isHostWeatherActive() ? 32 : 0);
  if (current != owner) {
    owner = current; ++epoch; ++mediaEpoch;
    pendingCount = pendingHead = 0;
    spotifyDue = true;
    itemId = coverUrl = nativeTrack = nativeArtist = coveredId = "";
    // Ownership/reconnect is a fresh scheduling epoch, not an old failed attempt.
    initGate = Gate(); spotifyGate = Gate(); weatherGate = Gate(); coverGate = Gate();
    spotifyFailures = weatherFailures = coverFailures = 0;
    spotifyError = weatherError = 0;
    standaloneSpotifyStatus = standaloneWeatherStatus = "";
  }
}

static void backoff(Gate& gate, uint8_t& failures, uint32_t now) {
  if (result.valid) { failures = 0; gate.defer(now, 0); }
  else {
    if (failures < 6) ++failures;
    uint32_t delay = result.retryMs ? result.retryMs : failureDelay(failures);
    if (!result.retryMs && (result.status == 400 || result.status == 401 || result.status == 403 ||
                           result.status == AuthInvalidClient || result.status == AuthSignInRequired)) delay = 60000;
    gate.defer(now, delay);
  }
}

#if ENABLE_SPOTIFY
static bool credentialsMatch() {
  return spotifyAccessToken == job.access && spotifyRefreshToken == job.refresh &&
         spotifyClientId == job.clientId && spotifyClientSecret == job.secret;
}
static bool mediaMatches() {
  return job.mediaEpoch == mediaEpoch && job.trackFingerprint == textFingerprint(currentTrack.c_str()) &&
         job.artistFingerprint == textFingerprint(currentArtist.c_str());
}
static void updateStatus() {
  if (result.valid) {
    if (job.kind != Cover) standaloneSpotifyStatus = (job.kind == Playback && !result.itemId[0]) ? "No active Spotify device" : "";
  } else if (result.status == 429) standaloneSpotifyStatus = "Spotify rate limited";
  else if (result.status == 401) standaloneSpotifyStatus = "Spotify sign-in required";
  else if (result.status == 403) standaloneSpotifyStatus = "Spotify access denied";
  else if (result.status == 404) standaloneSpotifyStatus = "No active Spotify device";
  else if (result.status >= 300) standaloneSpotifyStatus = "Spotify HTTP " + String(result.status);
  else standaloneSpotifyStatus = failureName(result.status);
}
static void redrawPlayback() {
  if (currentState == STATE_SPOTIFY) {
    if (activeTheme == 1) t2_redrawSpotifyPartial(); else redrawSpotifyPartial();
  } else if (currentState == STATE_CLOCK_DASHBOARD) redrawClockDashboardPartial();
  else if (currentState == STATE_RETRO_WATCH) redrawRetroWatchPartial();
}
static void resetArt() {
  albumArtReady = false; receivingArt = false; artDrawn = false;
  albumArtStartedAt = 0;
  albumArtRowsComplete = 0;
  memset(albumArtRowsReceived, 0, sizeof(albumArtRowsReceived));
  resetSpotifyDrawState();
  t2_resetSpotifyDrawState();
}
#endif

static void consume(uint32_t now) {
  uint8_t ticket;
  if (!busy || xQueueReceive(results, &ticket, 0) != pdTRUE) return;
  busy = false;
  bool owned = job.epoch == epoch;
  if (job.kind == Weather) {
    if (owned && weatherAllowed()) {
      backoff(weatherGate, weatherFailures, now);
      weatherError = result.valid ? 0 : result.status;
      standaloneWeatherStatus = result.valid ? "" : failureName(result.status);
      if (result.valid) {
        weatherGate.defer(now, WEATHER_POLL_INTERVAL_MS);
        weatherTemp = result.temp; weatherFeels = result.feels;
        weatherHumidity = result.humidity; weatherWind = result.wind;
        weatherHigh = result.high; weatherLow = result.low;
        weatherCondition = weatherName(result.wmo, weatherCode);
        weatherCity = WEATHER_CITY_NAME;
        weatherSunrise = result.sunrise; weatherSunset = result.sunset;
        if (currentState == STATE_WEATHER) redrawWeatherPartial();
        else if (currentState == STATE_CLOCK_DASHBOARD) redrawClockDashboardPartial();
        else if (currentState == STATE_RETRO_WATCH) redrawRetroWatchPartial();
      }
    }
  }
#if ENABLE_SPOTIFY
  else if (owned && spotifyAllowed() && credentialsMatch()) {
    updateStatus();
    if (result.tokenChanged) {
      spotifyAccessToken = result.access;
      spotifyRefreshToken = result.refresh;
      credentialsSaveFailed = !persistCredentials();  // Rotated refresh tokens; main task only.
    }
    if (job.kind == Cover) {
      backoff(coverGate, coverFailures, now);
      if (result.valid && mediaMatches() && job.artRevision == albumArtRevision && itemId == job.itemId &&
          !receivingArt && albumArt) {
        if (albumArt) memcpy(albumArt, result.pixels, sizeof(result.pixels));
        memset(albumArtRowsReceived, 1, sizeof(albumArtRowsReceived));
        albumArtRowsComplete = ALBUM_ART_H;
        albumArtReady = true; artDrawn = false;
        albumArtStartedAt = 0;
        ++albumArtRevision;
        resetSpotifyDrawState(); t2_resetSpotifyDrawState();
        coveredId = itemId;
        redrawPlayback();
      }
    } else {
      backoff(spotifyGate, spotifyFailures, now);
      spotifyError = result.valid ? 0 : result.status;
      if (job.kind == Control) {
        // Success or uncertain transport outcome: reconcile via GET, never replay.
        spotifyDue = true;
        if (result.valid) spotifyGate.defer(now, 300);
      } else if (result.valid && mediaMatches()) {
        bool changed = itemId != result.itemId || currentTrack != result.track || currentArtist != result.artist;
        if (changed) {
          currentLyrics = currentLyricsLine2 = prevLyricsLine = "";
          resetArt(); coveredId = ""; ++mediaEpoch;
        }
        currentTrack = result.track; currentArtist = result.artist;
        nativeTrack = currentTrack; nativeArtist = currentArtist;
        itemId = result.itemId; coverUrl = result.url;
        isPlaying = musicPlaying = result.playing;
        playDuration = result.duration;
        playProgress = progressAt(result.progress, playDuration, isPlaying, uint32_t(now - result.sampledAt));
        spotifyProgressUpdatedAt = now;
        t2_shuffle = result.shuffle; t2_volume = result.volume; t2_repeat = result.repeat;
        memcpy(t2_device, result.device, sizeof(result.device));
        spotifyDue = false; lastPoll = now;
        redrawPlayback();
      }
    }
  }
#endif
  // Erase credential copies as soon as the ownership round trip finishes.
  memset(job.access, 0, sizeof(job.access)); memset(job.refresh, 0, sizeof(job.refresh));
  memset(job.secret, 0, sizeof(job.secret));
  memset(result.access, 0, sizeof(result.access)); memset(result.refresh, 0, sizeof(result.refresh));
}

static bool dispatch(Kind kind, Command command, uint32_t now) {
  if (busy) return false;
#if ENABLE_SPOTIFY
  loadCredentialsOnce();
#endif
  if (kind != Weather && !spotifyConfigured()) {
    standaloneSpotifyStatus = failureName(NotConfigured); return false;
  }
  if (!start(now)) {
    if (kind != Weather) {
      spotifyError = NoMemory; spotifyGate.defer(now, 30000);
      standaloneSpotifyStatus = "Spotify worker unavailable";
    } else {
      weatherError = NoMemory; weatherGate.defer(now, 30000);
      standaloneWeatherStatus = "Weather worker unavailable";
    }
    return false;
  }
  job.~Job(); new (&job) Job();
  job.kind = kind; job.command = command;
  job.epoch = epoch; job.mediaEpoch = mediaEpoch;
  job.artRevision = albumArtRevision;
  job.trackFingerprint = textFingerprint(currentTrack.c_str());
  job.artistFingerprint = textFingerprint(currentArtist.c_str());
  job.hostActive = isHostActive(); job.hostAndroid = isHostAndroid;
  job.spotifyPage = currentState == STATE_SPOTIFY; job.playing = isPlaying;
#if ENABLE_SPOTIFY
  if (kind != Weather) {
    if (!copy(job.access, spotifyAccessToken.c_str()) || !copy(job.refresh, spotifyRefreshToken.c_str()) ||
        !copy(job.clientId, spotifyClientId.c_str()) || !copy(job.secret, spotifyClientSecret.c_str()) ||
        (kind == Cover && (!copy(job.itemId, itemId.c_str()) || !copy(job.url, coverUrl.c_str())))) {
      memset(job.access, 0, sizeof(job.access)); memset(job.refresh, 0, sizeof(job.refresh));
      memset(job.secret, 0, sizeof(job.secret));
      standaloneSpotifyStatus = "Spotify fields too long";
      spotifyGate.defer(now, 60000); return false;
    }
  }
#endif
  uint8_t ticket = 1;
  busy = xQueueSend(jobs, &ticket, 0) == pdTRUE;
  if (busy && kind == Weather) standaloneWeatherStatus = "Updating";
  else if (busy && kind != Cover) standaloneSpotifyStatus = "Updating";
  return busy;
}

static String retryStatus(int error, const Gate& gate, uint32_t now) {
  String reason;
  if (error == 401) reason = "Sign-in required";
  else if (error == 403) reason = "Access denied";
  else if (error == 404) reason = "No active device";
  else if (error == 429) reason = "Rate limited";
  else if (error >= 300) reason = "HTTP " + String(error);
  else reason = failureName(error);
  uint32_t seconds = (uint64_t(gate.remaining(now)) + 999) / 1000;
  return reason + "; retry " + String(seconds) + "s";
}
static void publishStatuses(uint32_t now) {
  static Gate refresh;
  static String shownSpotify, shownWeather;
  if (!refresh.ready(now)) return;
  refresh.defer(now, 1000);
  if (!wifiConnected) standaloneWeatherStatus = standaloneSpotifyStatus = "Offline";
  else {
    if (dhNetPaused) standaloneWeatherStatus = standaloneSpotifyStatus = "Paused";
    else {
      if (!weatherAllowed()) standaloneWeatherStatus = "";
      else if (!(busy && job.kind == Weather) && weatherError) standaloneWeatherStatus = retryStatus(weatherError, weatherGate, now);
      if (!spotifyAllowed()) standaloneSpotifyStatus = "";
      else if (!spotifyConfigured()) standaloneSpotifyStatus = failureName(NotConfigured);
      else if (!(busy && job.kind != Weather && job.kind != Cover) && spotifyError)
        standaloneSpotifyStatus = retryStatus(spotifyError, spotifyGate, now);
#if ENABLE_SPOTIFY
      if (spotifyAllowed() && spotifyConfigured() && credentialsSaveFailed && !standaloneSpotifyStatus.length())
        standaloneSpotifyStatus = "Spotify credentials not saved";
#endif
    }
  }
  if (shownWeather != standaloneWeatherStatus) {
    shownWeather = standaloneWeatherStatus;
    if (currentState == STATE_WEATHER) redrawWeatherPartial();
  }
#if ENABLE_SPOTIFY
  if (shownSpotify != standaloneSpotifyStatus) {
    shownSpotify = standaloneSpotifyStatus;
    if (currentState == STATE_SPOTIFY) redrawPlayback();
  }
#endif
}
}  // namespace standalone_services

// Main-only: call after a validated SPOTIFY_AUTH packet has updated all globals.
// It also prevents a late lazy NVS load from overwriting newly received tokens.
inline void notifyStandaloneCredentialsChanged() {
  using namespace standalone_services;
#if ENABLE_SPOTIFY
  credentialsLoaded = true;
  credentialsSaveFailed = !persistCredentials();
#endif
  ++epoch; ++mediaEpoch;
  spotifyGate = Gate(); coverGate = Gate(); initGate = Gate();
  spotifyFailures = coverFailures = 0; spotifyError = 0;
  spotifyDue = true; pendingHead = pendingCount = 0;
  itemId = coverUrl = nativeTrack = nativeArtist = coveredId = "";
  standaloneSpotifyStatus = "Spotify credentials updated";
#if ENABLE_SPOTIFY
  if (credentialsSaveFailed) standaloneSpotifyStatus += "; save failed";
#endif
}

inline void setupStandaloneServices() {
#if ENABLE_SPOTIFY
  standalone_services::loadCredentialsOnce();
#endif
}

inline bool requestStandaloneSpotifyAuthorization() {
#if ENABLE_SPOTIFY
  return standalone_services::requestAuthorization(true);
#else
  return false;
#endif
}

inline bool queueStandaloneCommand(const String& text) {
  using namespace standalone_services;
#if ENABLE_SPOTIFY
  loadCredentialsOnce();
#endif
  Command command = commandFor(text.c_str());
  observeOwner();
  if (command == None || !spotifyAllowed()) return false;
  // true acknowledges local ownership even on a drop. The caller must NOT also
  // send the command to the host (a second toggle could undo the first).
  if (!spotifyConfigured()) {
    standaloneSpotifyStatus = failureName(NotConfigured);
#if ENABLE_SPOTIFY
    requestAuthorization(command == PlayPause);
#endif
    return true;
  }
#if ENABLE_SPOTIFY
  if (command == PlayPause && authFailure(spotifyError)) { requestAuthorization(true); return true; }
#endif
  uint32_t now = millis();
  if (!spotifyGate.ready(now)) { standaloneSpotifyStatus = "Spotify cooling down"; return true; }
  if (pendingCount == 4) { standaloneSpotifyStatus = "Spotify queue busy"; return true; }
  pending[(pendingHead + pendingCount) % 4].command = command;
  pending[(pendingHead + pendingCount) % 4].at = now;
  ++pendingCount;
  standaloneSpotifyStatus = "Spotify command queued";
  if (command == Next || command == Previous) ++mediaEpoch;
  return true;
}

inline void tickStandaloneServices() {
  using namespace standalone_services;
#if ENABLE_SPOTIFY
  loadCredentialsOnce();
#endif
  uint32_t now = millis();
  observeOwner();
  consume(now);
#if ENABLE_SPOTIFY
  if (spotifyAllowed() && (!spotifyConfigured() || authFailure(spotifyError))) requestAuthorization(false);
#endif
  publishStatuses(now);
  if (busy) return;
#if ENABLE_SPOTIFY
  if (itemId.length() && (currentTrack != nativeTrack || currentArtist != nativeArtist)) {
    itemId = coverUrl = coveredId = ""; ++mediaEpoch; spotifyDue = true;
  }
  while (pendingCount) {
    Pending p = pending[pendingHead];
    if (uint32_t(now - p.at) > 3000 || !spotifyAllowed()) {
      standaloneSpotifyStatus = "Spotify command expired";
      pendingHead = (pendingHead + 1) % 4; --pendingCount;
      continue;
    }
    if (!spotifyGate.ready(now)) break;
    pendingHead = (pendingHead + 1) % 4; --pendingCount;
    if (dispatch(Control, p.command, now)) return;
  }
#endif
  if (weatherAllowed() && weatherGate.ready(now)) {
    if (dispatch(Weather, None, now)) return;
  }
#if ENABLE_SPOTIFY
  if (!spotifyAllowed() || !spotifyConfigured()) return;
  uint32_t interval = (currentState == STATE_SPOTIFY || isPlaying) ?
      SPOTIFY_POLL_INTERVAL_ACTIVE_MS : SPOTIFY_POLL_INTERVAL_IDLE_MS;
  if (spotifyGate.ready(now) && (spotifyDue || uint32_t(now - lastPoll) >= interval)) {
    if (dispatch(Playback, None, now)) return;
  }
  if (itemId.length() && coverUrl.length() && coveredId != itemId && coverGate.ready(now) && !receivingArt)
    dispatch(Cover, None, now);
#endif
}

#else  // Not ESP32: deliberately do not fall back to blocking network calls.
inline void tickStandaloneServices() {}
inline bool queueStandaloneCommand(const String&) { return false; }
inline void notifyStandaloneCredentialsChanged() {}
inline void setupStandaloneServices() {}
inline bool requestStandaloneSpotifyAuthorization() { return false; }
#endif
#endif
