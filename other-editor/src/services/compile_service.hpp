/**
 * \file services/compile_service.hpp
 * what the compile window works with: the open project as a compile would see it, the one
 * compile that can run, and what came of the last. a window asks it once a frame and reads
 * the rest only when its stamp has moved
 **/
#ifndef OTHER_EDITOR_SERVICES_COMPILE_SERVICE_HPP
#define OTHER_EDITOR_SERVICES_COMPILE_SERVICE_HPP

#include <string>
#include <string_view>

#include "core/defines.hpp"

#include "project/compile_session.hpp"
#include "project/project_compiler.hpp"

namespace other {

  class driver;

  struct compile_service {
    /// \note CompileState in C# is the same list in the same order
    enum class state : int32_t {
      NO_PROJECT,
      LOADING,
      READY,
    };

    /// \note CompileResult in C# is the same list in the same order
    enum class result : int32_t {
      NONE,
      PACKAGED,
      CANCELLED,
      FAILED,
    };

    /// the one compile. it is here so that the driver can wait for it at shutdown, before
    ///  the VM whose devices it links against is gone
    compile_session session;

    /// the project the preview was made from, and the revision of its file then: another
    ///  project, or the same one changed on disk, is read again
    filepath shown_project;
    uint64_t shown_revision = 0;
    compile_preview preview;
    /// where a package of this project goes unless another folder is chosen
    std::string default_output = "";

    /// goes up whenever something a window reads from here is not what it was
    uint64_t stamp = 1;

    /// once a frame, by whoever shows it: a compile that finished gives its report, and a
    ///  project that is another, or whose file changed, is read again. with no driver
    ///  there is no project
    state poll(driver* drvr);
    /// reads the open project again
    void refresh();

    /// starts a compile of the shown project with those choices, which are written to the
    ///  project file first. empty when it started, otherwise why it did not
    std::string start(driver* drvr, const std::string_view public_name, const std::string_view output_folder, const std::string_view starting_scene,
                      const std::string_view main_pipeline, bool debug);

    result last_result() const;
  };

}  // namespace other

#endif  // OTHER_EDITOR_SERVICES_COMPILE_SERVICE_HPP
