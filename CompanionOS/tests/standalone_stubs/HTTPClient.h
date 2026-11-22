#pragma once
#include "Arduino.h"
#include "WiFiClientSecure.h"
#include <deque>
#include <map>
#include <vector>
#include <cassert>
#include <cstdlib>
constexpr int HTTPC_DISABLE_FOLLOW_REDIRECTS = 0;
constexpr int HTTPC_ERROR_TOO_LESS_RAM = -8, HTTPC_ERROR_READ_TIMEOUT = -11;
struct MockReply {
  int status = 200;
  std::string body;
  std::map<std::string, std::string> headers;
  int size = -1;
  bool chunked = false, stall = false;
};
struct MockCall { std::string url, method, auth, accept, body; };
extern std::deque<MockReply> replies;
extern std::vector<MockCall> calls;
extern unsigned bodyDecodes;
class HTTPClient {
  MockReply reply;
  MockCall call;
  WiFiClientSecure* client = nullptr;
 public:
  void useHTTP10(bool value) { assert(!value); }
  void setReuse(bool) {}
  void setConnectTimeout(int) {}
  void setTimeout(int) {}
  void setFollowRedirects(int value) { assert(value == HTTPC_DISABLE_FOLLOW_REDIRECTS); }
  bool begin(WiFiClientSecure& c, const String& url) { client = &c; call.url = url; return true; }
  void collectHeaders(const char**, size_t) {}
  void addHeader(const String& key, const String& value) {
    if (key == "Authorization") call.auth = value;
    if (key == "Accept-Encoding") call.accept = value;
  }
  int sendRequest(const char* method, const String& body) {
    assert(!replies.empty());
    call.method = method; call.body = body; calls.push_back(call);
    reply = replies.front(); replies.pop_front(); return reply.status;
  }
  String header(const char* key) { return reply.headers[key]; }
  int getSize() { return reply.size; }
  String errorToString(int code) { return code == -11 ? "read timeout" : "connection failed"; }
  void end() {}
  int writeToStream(Stream* sink) {
    ++bodyDecodes;
    if (reply.stall) { fakeNow += 15001; client->available(); return -11; }
    auto put = [&](const char* bytes, size_t size) {
      // Same short-write retry contract as native HTTPClient.
      size_t n = sink->write(reinterpret_cast<const uint8_t*>(bytes), size);
      if (n != size) n += sink->write(reinterpret_cast<const uint8_t*>(bytes) + n, size - n);
      return n == size;
    };
    if (!reply.chunked) {
      if (!put(reply.body.data(), reply.body.size())) return -10;
      if (reply.size >= 0 && reply.size != int(reply.body.size())) return -10;
      return int(reply.body.size());
    }
    // Mock native chunk decoder, not production code. Exercises adapter boundaries.
    size_t at = 0, total = 0;
    for (;;) {
      size_t end = reply.body.find("\r\n", at);
      if (end == std::string::npos) return -11;
      size_t size = std::strtoul(reply.body.substr(at, end - at).c_str(), nullptr, 16);
      at = end + 2;
      if (!size) { reply.size = int(total); return int(total); }
      if (at + size + 2 > reply.body.size() || reply.body.substr(at + size, 2) != "\r\n") return -11;
      if (!put(reply.body.data() + at, size)) return -10;
      total += size; at += size + 2;
    }
  }
};
