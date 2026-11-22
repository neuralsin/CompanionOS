#pragma once
#include "Arduino.h"
extern int fakeTlsError;
class WiFiClientSecure {
 public:
  virtual ~WiFiClientSecure() = default;
  virtual int available() { return 0; }
  virtual uint8_t connected() { return 1; }
  virtual int read() { return -1; }
  virtual int read(uint8_t*, size_t) { return -1; }
  virtual void stop() {}
  void setInsecure() {}
  void setHandshakeTimeout(unsigned long) {}
  int lastError(char*, size_t) { return fakeTlsError; }
};
