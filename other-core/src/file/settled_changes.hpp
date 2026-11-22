/**
 * \file file/settled_changes.hpp
 * changed files, held back until each has stopped changing. a tool that rebuilds a file
 * often does it in steps (delete, write, write again), and whoever reacts to the first step
 * finds the file missing or half written
 **/
#ifndef OTHER_CORE_FILE_SETTLED_CHANGES_HPP
#define OTHER_CORE_FILE_SETTLED_CHANGES_HPP

#include <filesystem>

#include "core/defines.hpp"
#include "data-structures/std_container.hpp"

namespace other {

  class settled_changes {
   public:
    /// how many checks in a row a file may be missing before it is taken to be gone for good
    constexpr static inline uint32_t kChecksUntilGone = 20;

    struct result {
      /// there, readable, and the same as at the check before
      ostd::vector<filepath> settled;
      /// missing for kChecksUntilGone checks
      ostd::vector<filepath> gone;
    };

    /// a file changed, was made or was removed. one that is already waiting starts over
    void note(const filepath& path);

    /// called at a steady interval, which is how long a file has to stay the same. what it
    ///  returns is no longer waiting
    result check();

    bool empty() const { return waiting.empty(); }

   private:
    struct observation {
      uintmax_t size = 0;
      std::filesystem::file_time_type written;

      bool operator==(const observation&) const = default;
    };

    struct pending {
      filepath path;
      opt<observation> seen;
      uint32_t missing_checks = 0;
    };

    ostd::vector<pending> waiting;
  };

}  // namespace other

#endif  // OTHER_CORE_FILE_SETTLED_CHANGES_HPP
