#pragma once
#include <vector>
static int udpInstances = 0, udpStops = 0;
static std::vector<uint8_t> sentPacket;
class WiFiUDP {
 public:
  WiFiUDP() { ++udpInstances; }
  ~WiFiUDP() { --udpInstances; }
  void stop() { ++udpStops; }
  int parsePacket() { return 0; }
  void flush() {}
  int beginPacket(const char*, uint16_t port) { assert(port == 8890); sentPacket.clear(); return 1; }
  void write(const uint8_t* data, size_t size) { sentPacket.assign(data, data + size); }
  int endPacket() { return 1; }
};
