/**
 * \file scripting/dotnet_bindings/project_bindings.hpp
 * the open project as a script reads it: a copy taken when the script asks whether anything
 * changed, so the strings it then reads are the ones that answer was about
 **/
#ifndef OTHERLIB_SCRIPTING_DOTNET_BINDINGS_PROJECT_BINDINGS_HPP
#define OTHERLIB_SCRIPTING_DOTNET_BINDINGS_PROJECT_BINDINGS_HPP

#include <string>
#include <vector>

#include "core/defines.hpp"

#include "dotnet/types.hpp"

namespace other {
  namespace bindings {

    /// plain containers: this outlives the arena
    struct script_project {
      struct load_error {
        std::string subject;
        std::string message;
      };

      /// a project::state
      int32_t state = 0;
      std::string name;
      std::string version;
      std::string author;
      std::string description;
      std::string file;
      int32_t scene_count = 0;
      /// empty when there is none
      std::string starting_scene;

      bool file_notice = false;
      std::string file_notice_error;
      std::vector<std::string> file_notice_needs_reopen;

      std::vector<load_error> load_errors;

      std::string script_project_path;
      bool scripts_building = false;
      bool scripts_loaded = false;

      bool script_failure = false;
      std::string script_failure_project_path;
      std::string script_failure_reason;
      std::string script_failure_details;
      bool script_failure_can_retry = true;

      /// the same for two projects only when a script would read the same from both
      natural_t stamp() const;
    };

    /// takes the copy, and says what it holds: a script reads the rest again only when this
    ///  is not what it was
    natural_t native_project_change_stamp();

    int32_t native_project_get_state();
    const char* native_project_get_name();
    const char* native_project_get_version();
    const char* native_project_get_author();
    const char* native_project_get_description();
    const char* native_project_get_file();
    int32_t native_project_get_scene_count();
    const char* native_project_get_starting_scene();

    nbool32 native_project_has_file_notice();
    const char* native_project_get_file_notice_error();
    int32_t native_project_get_file_notice_part_count();
    const char* native_project_get_file_notice_part(int32_t index);

    int32_t native_project_get_load_error_count();
    const char* native_project_get_load_error_subject(int32_t index);
    const char* native_project_get_load_error_message(int32_t index);

    const char* native_project_get_script_project_path();
    nbool32 native_project_scripts_building();
    nbool32 native_project_scripts_loaded();

    nbool32 native_project_has_script_failure();
    const char* native_project_get_script_failure_project_path();
    const char* native_project_get_script_failure_reason();
    const char* native_project_get_script_failure_details();
    nbool32 native_project_script_failure_can_retry();

    void native_project_request_script_rebuild();

    /// a frame has started: the copy is taken again the next time a script asks
    void begin_script_project_frame();

    /// what scripts read in place of the driver's project, and how many times they asked for
    ///  a rebuild of it. null goes back to the driver's
    void set_script_project(const script_project* project);
    natural_t script_project_rebuild_requests();
    /// how many strings scripts have read so far
    natural_t script_project_string_reads();

  }  // namespace bindings
}  // namespace other

#endif  // OTHERLIB_SCRIPTING_DOTNET_BINDINGS_PROJECT_BINDINGS_HPP
