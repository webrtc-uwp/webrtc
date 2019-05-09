#ifndef HOLOLIGHT_UTILS_FUTURE_UTILS_H
#define HOLOLIGHT_UTILS_FUTURE_UTILS_H

namespace hololight::util::future_utils {

template <typename R>
inline bool is_ready(std::future<R> const& f) {
  return std::future_status::ready == f.wait_for(std::chrono::seconds(0));
}

template <typename R>
inline bool is_ready(std::shared_future<R> const& f) {
  return std::future_status::ready == f.wait_for(std::chrono::seconds(0));
}
}  // namespace hololight::util::future_utils

#endif  // HOLOLIGHT_UTILS_FUTURE_UTILS_H