#pragma once
#include "Arduino.h"
#include <vector>
#include <array>
#include <cstring>
class IPAddress {
  uint8_t bytes[4] = {};
 public:
  bool fromString(const String& text) {
    unsigned a, b, c, d; char tail;
    if (sscanf(text.c_str(), "%u.%u.%u.%u%c", &a, &b, &c, &d, &tail) != 4 ||
        a > 255 || b > 255 || c > 255 || d > 255) return false;
    bytes[0] = uint8_t(a); bytes[1] = uint8_t(b); bytes[2] = uint8_t(c); bytes[3] = uint8_t(d); return true;
  }
  uint8_t operator[](unsigned i) const { return bytes[i]; }
};
struct MockDatagram { std::array<uint8_t, 4> address; uint16_t port; std::string body; };
class WiFiUDP {
  MockDatagram pending;
 public:
  std::vector<MockDatagram> sent;
  bool fail = false;
  int beginPacket(const IPAddress& address, uint16_t port) {
    pending = {}; pending.port = port;
    for (unsigned i = 0; i < 4; ++i) pending.address[i] = address[i];
    return !fail;
  }
  size_t write(const uint8_t* bytes, size_t size) { pending.body.append(reinterpret_cast<const char*>(bytes), size); return size; }
  int endPacket() { if (fail) return 0; sent.push_back(pending); return 1; }
};
