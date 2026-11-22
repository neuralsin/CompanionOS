#pragma once
#include <vector>
#include <cstring>
namespace mock_nvs {
static std::vector<uint8_t> blob;
static unsigned writes = 0;
static bool failWrite = false;
}
class Preferences {
 public:
  bool begin(const char*, bool readonly) { return !readonly || !mock_nvs::blob.empty(); }
  void end() {}
  size_t getBytesLength(const char*) { return mock_nvs::blob.size(); }
  size_t getBytes(const char*, void* dest, size_t size) {
    if (size != mock_nvs::blob.size()) return 0;
    memcpy(dest, mock_nvs::blob.data(), size); return size;
  }
  size_t putBytes(const char*, const void* data, size_t size) {
    if (mock_nvs::failWrite) return 0;
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    mock_nvs::blob.assign(bytes, bytes + size); ++mock_nvs::writes; return size;
  }
};
