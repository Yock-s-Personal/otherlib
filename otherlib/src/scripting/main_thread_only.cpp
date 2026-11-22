/**
 * \file scripting/main_thread_only.cpp
 **/
#include "scripting/main_thread_only.hpp"

#include "core/logger.hpp"

namespace other {
  namespace bindings {

    void report_call_off_main_thread(const std::string_view name, std::atomic<bool>& reported) {
      if (reported.exchange(true)) {
        return;
      }
      CORE_LOG_ERROR("[SCRIPT] '{}' was called from a thread that is not the main thread. The engine can only be called from the main thread: the call was not made, and later ones from other threads will not be either.", name);
    }

  }  // namespace bindings
}  // namespace other
