/**
 * \file file/settled_changes.cpp
 **/
#include "file/settled_changes.hpp"

#include <algorithm>
#include <fstream>

#include "core/profiler.hpp"

namespace other {

  void settled_changes::note(const filepath& path) {
    const auto found = std::ranges::find(waiting, path, &pending::path);
    if (found == waiting.end()) {
      waiting.push_back({ .path = path });
      return;
    }
    found->seen.reset();
    found->missing_checks = 0;
  }

  settled_changes::result settled_changes::check() {
    PROFILE_SECTION("settled_changes::check");
    result out;
    for (pending& file : waiting) {
      std::error_code ec;
      if (!std::filesystem::is_regular_file(file.path, ec)) {
        file.seen.reset();
        if (++file.missing_checks >= kChecksUntilGone) {
          out.gone.push_back(file.path);
        }
        continue;
      }
      file.missing_checks = 0;

      const observation now = { .size = std::filesystem::file_size(file.path, ec), .written = std::filesystem::last_write_time(file.path, ec) };
      /// a writer that still holds the file keeps it from being opened
      if (file.seen.has_value() && *file.seen == now && std::ifstream{ file.path, std::ios::binary }.is_open()) {
        out.settled.push_back(file.path);
      } else {
        file.seen = now;
      }
    }

    const auto done = [&out](const pending& file) -> bool {
      return std::ranges::contains(out.settled, file.path) || std::ranges::contains(out.gone, file.path);
    };
    std::erase_if(waiting, done);
    return out;
  }

}  // namespace other
