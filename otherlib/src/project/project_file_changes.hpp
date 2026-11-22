/**
 * \file project/project_file_changes.hpp
 * what changed in a project file on disk, measured against the text a project was loaded
 * from: what an open project can take as it is, and what it cannot take without being
 * opened again
 **/
#ifndef OTHERLIB_PROJECT_PROJECT_FILE_CHANGES_HPP
#define OTHERLIB_PROJECT_PROJECT_FILE_CHANGES_HPP

#include "core/defines.hpp"
#include "data-structures/std_container.hpp"

namespace other {

  struct project_file_changes {
    /// why the text on disk cannot be used; nothing else is set when this is
    opt<std::string> error;
    /// the metadata rows differ; an open project re-reads them
    bool metadata_changed = false;
    /// the starting scene the file names now, when it is not the one it named before
    opt<std::string> starting_scene;
    /// the parts that differ and that a loaded project only reads when it is opened, in words
    ///  for a person: "the scene list", "the plugins"
    ostd::vector<std::string> needs_reopen;

    bool any() const { return error.has_value() || metadata_changed || starting_scene.has_value() || !needs_reopen.empty(); }
  };

  /// loaded is the text the project was opened from. a part of the file that only a compile
  ///  reads ([vm], [rendering], [package], the public name) is not reported: a compile reads
  ///  the file each time
  project_file_changes compare_project_files(std::string_view loaded, std::string_view on_disk);

}  // namespace other

#endif  // OTHERLIB_PROJECT_PROJECT_FILE_CHANGES_HPP
