#ifndef COMPANION_TEST_HOST_ARDUINO_H
#define COMPANION_TEST_HOST_ARDUINO_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <string>
#include <sstream>
#include <iomanip>

using std::min;
using std::max;
template<class T, class L, class H> T constrain(T x, L low, H high) {
  return x < low ? static_cast<T>(low) : x > high ? static_cast<T>(high) : x;
}
inline long map(long x, long a, long b, long c, long d) {
  return (x - a) * (d - c) / (b - a) + c;
}

// Only the Arduino String surface used by these headers. Indices are byte-based.
class String {
  std::string value_;
 public:
  String() = default;
  String(const char* s) : value_(s ? s : "") {}
  String(const std::string& s) : value_(s) {}
  String(char c) : value_(1, c) {}
  String(int n) : value_(std::to_string(n)) {}
  String(unsigned n) : value_(std::to_string(n)) {}
  String(long n) : value_(std::to_string(n)) {}
  String(unsigned long n) : value_(std::to_string(n)) {}
  String(double n, unsigned decimals = 2) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(decimals) << n;
    value_ = out.str();
  }
  const char* c_str() const { return value_.c_str(); }
  unsigned length() const { return static_cast<unsigned>(value_.size()); }
  bool isEmpty() const { return value_.empty(); }
  bool equalsIgnoreCase(const String& other) const {
    if (length() != other.length()) return false;
    for (unsigned i = 0; i < length(); ++i)
      if (std::tolower(static_cast<unsigned char>((*this)[i])) !=
          std::tolower(static_cast<unsigned char>(other[i]))) return false;
    return true;
  }
  bool reserve(unsigned n) { value_.reserve(n); return true; }
  char operator[](unsigned i) const { return i < value_.size() ? value_[i] : '\0'; }
  char charAt(unsigned i) const { return (*this)[i]; }
  String substring(unsigned first, unsigned last) const {
    if (first > last) std::swap(first, last);
    first = min(first, length());
    last = min(last, length());
    return value_.substr(first, last - first);
  }
  String substring(unsigned first) const { return substring(first, length()); }
  void remove(unsigned first, unsigned count = static_cast<unsigned>(-1)) {
    if (first < length()) value_.erase(first, count);
  }
  void replace(char a, char b) { std::replace(value_.begin(), value_.end(), a, b); }
  void replace(const String& from, const String& to) {
    if (!from.length()) return;
    std::size_t p = 0;
    while ((p = value_.find(from.value_, p)) != std::string::npos) {
      value_.replace(p, from.length(), to.value_);
      p += to.length();
    }
  }
  void trim() {
    const auto a = value_.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) { value_.clear(); return; }
    value_ = value_.substr(a, value_.find_last_not_of(" \t\r\n") - a + 1);
  }
  int indexOf(char c, unsigned from = 0) const {
    const auto p = value_.find(c, from);
    return p == std::string::npos ? -1 : static_cast<int>(p);
  }
  int indexOf(const String& s, unsigned from = 0) const {
    const auto p = value_.find(s.value_, from);
    return p == std::string::npos ? -1 : static_cast<int>(p);
  }
  long toInt() const { return std::strtol(c_str(), nullptr, 10); }
  void toCharArray(char* dest, unsigned n) const {
    if (n) { std::strncpy(dest, c_str(), n - 1); dest[n - 1] = 0; }
  }
  String& operator+=(const String& s) { value_ += s.value_; return *this; }
  friend String operator+(String a, const String& b) { return a += b; }
  friend bool operator==(const String& a, const String& b) { return a.value_ == b.value_; }
  friend bool operator!=(const String& a, const String& b) { return !(a == b); }
};

static_assert(sizeof(uint32_t) == 4, "Host must provide 32-bit timestamps");
static uint32_t hostMillis = 0;
inline unsigned long millis() { return hostMillis; }
inline void delay(unsigned long ms) { hostMillis += static_cast<uint32_t>(ms); }
inline void yield() {}
inline long random(long a, long b) { return a + (b > a ? (b - a) / 2 : 0); }
inline long random(long b) { return random(0, b); }
constexpr double DEG_TO_RAD = 0.017453292519943295;
#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif

#endif
