#ifndef STANDALONE_SERVICES_DETAIL_H
#define STANDALONE_SERVICES_DETAIL_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace standalone_detail {

struct Gate {
  uint32_t since = 0;
  uint32_t wait = 0;
  bool ready(uint32_t now) const { return uint32_t(now - since) >= wait; }
  void defer(uint32_t now, uint32_t delay) { since = now; wait = delay; }
  uint32_t remaining(uint32_t now) const {
    uint32_t elapsed = uint32_t(now - since);
    return elapsed >= wait ? 0 : wait - elapsed;
  }
};

struct AuthRequestLimiter {
  Gate automatic, explicitAction;
  bool ready(uint32_t now, bool userAction) const {
    return userAction ? explicitAction.ready(now) : automatic.ready(now);
  }
  void attempted(uint32_t now) {
    automatic.defer(now, 5UL * 60UL * 1000UL);
    explicitAction.defer(now, 15000);
  }
};

struct CredentialRecord {
  uint32_t version = 1, checksum = 0;
  char access[2048] = {}, refresh[1024] = {}, clientId[128] = {}, secret[256] = {};
};
inline uint32_t credentialChecksum(const CredentialRecord& record) {
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&record) + offsetof(CredentialRecord, access);
  uint32_t crc = UINT32_MAX;
  for (size_t i = 0; i < sizeof(record) - offsetof(CredentialRecord, access); ++i) {
    crc ^= bytes[i];
    for (unsigned bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320UL & (0U - (crc & 1)));
  }
  return ~crc;
}
template <size_t N> inline bool credentialText(const char (&text)[N]) {
  const char* end = static_cast<const char*>(memchr(text, 0, N));
  if (!end) return false;
  for (const char* p = text; p != end; ++p) if (uint8_t(*p) <= 32 || uint8_t(*p) >= 127) return false;
  return true;
}
inline bool validCredentialRecord(const CredentialRecord& record) {
  // Persist only complete refresh-capable tuples, never truncated partial data.
  return record.version == 1 && credentialText(record.access) && credentialText(record.refresh) &&
         credentialText(record.clientId) && credentialText(record.secret) &&
         record.refresh[0] && record.clientId[0] && record.checksum == credentialChecksum(record);
}
inline void eraseCredentialRecord(CredentialRecord& record) {
  volatile uint8_t* bytes = reinterpret_cast<volatile uint8_t*>(&record);
  for (size_t i = 0; i < sizeof(record); ++i) bytes[i] = 0;
}

enum Failure : int {
  NoMemory = -1001, TooLarge = -1002, TimedOut = -1003, Encoding = -1004,
  UnsafeRedirect = -1005, InvalidJson = -1006, InvalidJpeg = -1007,
  NotConfigured = -1008, TlsFailure = -1009, AuthInvalidClient = -1010, AuthSignInRequired = -1011
};
inline const char* failureName(int status) {
  switch (status) {
    case NoMemory: return "Out of memory";
    case TooLarge: return "Response too large";
    case TimedOut: return "Request timed out";
    case Encoding: return "Unsupported encoding";
    case UnsafeRedirect: return "Redirect rejected";
    case InvalidJson: return "Invalid JSON response";
    case InvalidJpeg: return "Invalid cover image";
    case NotConfigured: return "Spotify not configured";
    case TlsFailure: return "TLS connection failed";
    case AuthInvalidClient: return "Spotify app credentials invalid";
    case AuthSignInRequired: return "Spotify sign-in required";
    default: return "Network request failed";
  }
}

// Append-only destination for already decoded HTTP bytes. Failure is latched:
// HTTPClient's short-write retry must never turn overflow into a partial success.
struct BoundedBody {
  uint8_t* bytes;
  size_t capacity, size = 0;
  uint32_t started;
  int error = 0;
  BoundedBody(uint8_t* dest, size_t cap, uint32_t at) : bytes(dest), capacity(cap), started(at) {}
  size_t append(const uint8_t* data, size_t count, uint32_t now) {
    if (error) return 0;
    if (uint32_t(now - started) >= 15000) { error = TimedOut; return 0; }
    if (count > capacity - size) { error = TooLarge; return 0; }
    if (count) memcpy(bytes + size, data, count);
    size += count;
    return count;
  }
};

inline bool configured(const char* access, const char* refresh, const char* clientId) {
  return (access && *access) || (refresh && *refresh && clientId && *clientId);
}

inline uint64_t textFingerprint(const char* text) {
  uint64_t hash = UINT64_C(14695981039346656037);
  for (; *text; ++text) { hash ^= uint8_t(*text); hash *= UINT64_C(1099511628211); }
  return hash;
}

template <size_t N> inline void copyDisplay(char (&dest)[N], const char* src) {
  if (!src) src = "";
  size_t size = strlen(src);
  if (size >= N) {
    size = N - 1;
    // Never leave half a UTF-8 character at the truncation boundary.
    while (size && (uint8_t(src[size]) & 0xc0) == 0x80) --size;
  }
  memcpy(dest, src, size); dest[size] = 0;
}

inline bool urlCharacters(const char* url) {
  for (; *url; ++url) if (uint8_t(*url) <= 32 || *url == 127 || *url == '\\') return false;
  return true;
}
inline size_t httpsOriginLength(const char* url) {
  if (strncmp(url, "https://", 8) || !urlCharacters(url)) return 0;
  size_t end = 8;
  while (url[end] && url[end] != '/' && url[end] != '?' && url[end] != '#') ++end;
  if (end == 8) return 0;
  for (size_t i = 8; i < end; ++i) {
    char c = url[i];
    if (c == ':') return (end - i == 4 && !strncmp(url + i, ":443", 4) && i > 8) ? end : 0;
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '-' || c == '.')) return 0;
  }
  return end;
}
inline bool sameHttpsOrigin(const char* a, const char* b) {
  size_t an = httpsOriginLength(a), bn = httpsOriginLength(b);
  if (!an || an != bn) return false;
  for (size_t i = 0; i < an; ++i) {
    char ac = a[i], bc = b[i];
    if (ac >= 'A' && ac <= 'Z') ac += 'a' - 'A';
    if (bc >= 'A' && bc <= 'Z') bc += 'a' - 'A';
    if (ac != bc) return false;
  }
  return true;
}
template <size_t N> inline bool redirectTarget(char (&dest)[N], const char* base,
                                               const char* location, bool authenticated) {
  size_t origin = httpsOriginLength(base), n = strlen(location), prefix = 0;
  if (!origin || !n || !urlCharacters(location)) return false;
  if (!strncmp(location, "https://", 8)) {
    if (n >= N) return false;
  } else {
    if (strchr(location, ':') || location[0] == '#' || location[0] == '?' ||
        (location[0] == '/' && location[1] == '/')) return false;
    prefix = origin;
    if (location[0] != '/') {
      size_t pathEnd = strcspn(base, "?#");
      for (size_t i = origin; i < pathEnd; ++i) if (base[i] == '/') prefix = i + 1;
      if (prefix == origin) return false;
    }
    if (prefix + n >= N) return false;
  }
  if (prefix) memcpy(dest, base, prefix);
  memcpy(dest + prefix, location, n + 1);
  return httpsOriginLength(dest) && (!authenticated || sameHttpsOrigin(base, dest));
}

enum Command : uint8_t { None, PlayPause, Previous, Next, Shuffle };
inline Command commandFor(const char* s) {
  if (!strcmp(s, "PLAY_PAUSE")) return PlayPause;
  if (!strcmp(s, "PREV")) return Previous;
  if (!strcmp(s, "NEXT")) return Next;
  if (!strcmp(s, "SHUFFLE:TOGGLE")) return Shuffle;
  return None;
}

inline uint32_t retryAfterMs(const char* value) {
  // Retry-After delta seconds. Malformed/missing values get a conservative minute.
  if (!value || !*value) return 60000;
  uint32_t seconds = 0;
  for (; *value; ++value) {
    if (*value < '0' || *value > '9') return 60000;
    if (seconds > 214748 || (seconds == 214748 && *value > '3')) return 2147483647;
    seconds = seconds * 10 + unsigned(*value - '0');
  }
  return seconds ? seconds * 1000 : 1000;
}

inline uint32_t failureDelay(uint8_t failures) {
  unsigned shift = failures > 5 ? 5 : failures;
  uint32_t delay = 2000UL << shift;
  return delay > 60000 ? 60000 : delay;
}

template <size_t N> inline bool copy(char (&dest)[N], const char* src) {
  if (!src) src = "";
  size_t n = strlen(src);
  if (n >= N) { dest[0] = 0; return false; }
  memcpy(dest, src, n + 1);
  return true;
}

inline int progressAt(int progress, int duration, bool playing, uint32_t age) {
  if (duration <= 0) return 0;
  uint64_t p = progress > 0 ? uint64_t(progress) : 0;
  if (playing) p += age;
  return p > uint64_t(duration) ? duration : int(p);
}

// Each destination pixel samples one source pixel, including upscaling and MCU edges.
inline void sampleBlock(uint16_t* dest, uint16_t sourceW, uint16_t sourceH,
                        uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                        const uint16_t* pixels) {
  if (!sourceW || !sourceH || !w || !h) return;
  for (unsigned dy = 0; dy < 64; ++dy) {
    unsigned sy = dy * sourceH / 64;
    if (sy < y || sy >= unsigned(y) + h) continue;
    for (unsigned dx = 0; dx < 64; ++dx) {
      unsigned sx = dx * sourceW / 64;
      if (sx >= x && sx < unsigned(x) + w)
        dest[dy * 64 + dx] = pixels[(sy - y) * w + sx - x];
    }
  }
}

inline const char* weatherName(int wmo, int& code) {
  switch (wmo) {
    case 0: code = 800; return "Clear";
    case 1: case 2: case 3: code = 802; return "Partly Cloudy";
    case 45: case 48: code = 741; return "Foggy";
    case 51: case 53: case 55: case 56: case 57: code = 300; return "Drizzle";
    case 61: case 63: case 65: case 66: case 67: code = 500; return "Rain";
    case 71: case 73: case 75: case 77: code = 600; return "Snow";
    case 80: case 81: case 82: code = 521; return "Showers";
    case 85: case 86: code = 621; return "Snow Showers";
    case 95: case 96: case 99: code = 200; return "Thunderstorm";
    default: code = 803; return "Clouds";
  }
}
}  // namespace standalone_detail
#endif
