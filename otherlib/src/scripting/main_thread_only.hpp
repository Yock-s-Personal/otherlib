/**
 * \file scripting/main_thread_only.hpp
 * the engine side of 'scripts call the engine from the main thread only': what a script is
 * given in place of a native function checks the thread it is called from
 **/
#ifndef OTHERLIB_SCRIPTING_MAIN_THREAD_ONLY_HPP
#define OTHERLIB_SCRIPTING_MAIN_THREAD_ONLY_HPP

#include <atomic>
#include <string_view>
#include <utility>

#include "core/defines.hpp"
#include "thread/thread_safety.hpp"

namespace other {
  namespace bindings {

    /// says so in the log, once for each function: a script that calls from a thread of its
    ///  own usually does it every frame
    void report_call_off_main_thread(std::string_view name, std::atomic<bool>& reported);

    /// call has Fn's own signature, so it goes wherever Fn would. from the main thread it is
    ///  Fn. from any other thread nothing the engine holds is safe to touch, so Fn is not
    ///  called: the script gets an empty value back and the log gets an error
    template <auto Fn>
    struct main_thread_only;

    template <typename R, typename... Args, R (*Fn)(Args...)>
    struct main_thread_only<Fn> {
      static inline std::string_view name = "";
      static inline std::atomic<bool> reported = false;

      static R call(Args... args) {
        if (!is_on_main_thread()) [[unlikely]] {
          report_call_off_main_thread(name, reported);
          return R();
        }
        return Fn(std::forward<Args>(args)...);
      }
    };

  }  // namespace bindings
}  // namespace other

#endif  // OTHERLIB_SCRIPTING_MAIN_THREAD_ONLY_HPP
