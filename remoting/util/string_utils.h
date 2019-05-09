#ifndef HOLOLIGHT_UTIL_STRING_UTILS_H
#define HOLOLIGHT_UTIL_STRING_UTILS_H

#include <codecvt>
#include <locale>
#include <sstream>
#include <string>
#include <vector>

namespace hololight::util::string_utils {
inline std::wstring string_to_wstring(std::string& str) {
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

  return converter.from_bytes(str);
}

inline std::string wstring_to_string(std::wstring& str) {
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

  return converter.to_bytes(str);
}

inline std::vector<std::string> split(std::string& s, char delimiter) {
  std::stringstream ss(s);
  std::vector<std::string> result;

  while (ss.good()) {
    std::string substr;
    getline(ss, substr, ',');
    result.push_back(substr);
  }

  return result;
}
}  // namespace hololight::util::string_utils

#endif  // HOLOLIGHT_UTIL_STRING_UTILS_H