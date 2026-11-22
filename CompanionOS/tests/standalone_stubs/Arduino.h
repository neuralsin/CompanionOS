#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cstdarg>
#include <cstdio>
#include <type_traits>

class String : public std::string {
 public:
  using std::string::string;
  using std::string::operator=;
  String() = default;
  String(const std::string& s) : std::string(s) {}
  String substring(unsigned first) const { return substr(std::min(size_t(first), size())); }
  template <class T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
  String(T n) : std::string(std::to_string(n)) {}
  String(double value, unsigned digits) {
    std::ostringstream s; s << std::fixed << std::setprecision(digits) << value; assign(s.str());
  }
  void trim() {
    size_t first = find_first_not_of(" \r\n\t");
    if (first == npos) clear(); else assign(substr(first, find_last_not_of(" \r\n\t") - first + 1));
  }
  void toLowerCase() { std::transform(begin(), end(), begin(), [](unsigned char c) { return char(std::tolower(c)); }); }
};
extern uint32_t fakeNow;
inline unsigned long millis() { return fakeNow; }
template <class T> inline T constrain(T value, T low, T high) { return std::max(low, std::min(high, value)); }
class Stream {
  int error = 0;
 public:
  virtual ~Stream() = default;
  virtual size_t write(uint8_t) = 0;
  virtual size_t write(const uint8_t*, size_t) = 0;
  virtual int available() = 0;
  virtual int read() = 0;
  virtual int peek() = 0;
  virtual void flush() = 0;
  void setWriteError(int code = 1) { error = code; }
  int getWriteError() const { return error; }
  void clearWriteError() { error = 0; }
};
struct MockSerial {
  std::string output;
  void printf(const char* format, ...) {
    char text[2048]; va_list args; va_start(args, format);
    vsnprintf(text, sizeof(text), format, args); va_end(args); output += text;
  }
};
extern MockSerial Serial;
