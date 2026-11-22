/**
 * \file project/project.hpp
 **/
#ifndef OTHERLIB_PROJECT_PROJECT_HPP
#define OTHERLIB_PROJECT_PROJECT_HPP

#include <toml++/toml.hpp>

#include "core/arena_buffer.hpp"
#include "core/defines.hpp"
#include "file/file_handle.hpp"

#include "dotnet/dotnet_assembly.hpp"

#include "tools/project_tool.hpp"

namespace other {

  class driver_kernel;
  class project_system;

  struct project_event_data {
    std::string type;
    std::string project_name;
    std::string project_path;
  };

  struct load_error {
    /// scene name, csproj path or assembly path
    std::string subject;
    std::string message;
  };

  /// why the C# project is not running its current sources; kept until a build gets through
  struct script_failure {
    filepath csproj_path;
    std::string reason;
    /// msbuild errors when a build failed, otherwise whatever else is known
    std::string details;
    /// false when the csproj was never resolved, so no asset plan can rebuild it
    bool can_retry = true;
  };

  class project {
   public:
    enum state {
      EMPTY = 0,
      LOADING,
      LOADED,
      LOAD_FAILED,
      UNLOADING,

      NUM_STATES,
      INVALID_STATE = NUM_STATES,
    };
    struct metadata {
      std::string name;
      std::string description;
      std::string author;
      std::string version;
    };
    struct scene {
      std::string name;
      filepath path;

      bool scene_loaded;
      natural_t scene_id;

      ostd::vector<std::string> incoming_scenes;
      ostd::vector<std::string> outgoing_scenes;
    };
    struct script_data {
      filepath csproject_path;
      filepath cs_script_source;
      ostd::vector<filepath> cs_scripts;
      ostd::vector<filepath> lua_scripts;
    };

    project(project_system* proj_system);
    ~project();

    /// the project file a path stands for: the path itself when it is a file, or the .toml or
    ///  .oproj a folder holds under the folder's own name. empty when there is no such file
    static opt<filepath> find_project_file(const filepath& path);

    void load_from_file(driver_kernel* kernel, const filepath& path);
    void generate_at(driver_kernel* kernel, const filepath& directory);

    void fail_load();
    void unload();
    void set_state(state new_state);

    void add_built_script(const filepath& script_asset_path);
    void add_script_file(const filepath& script_file_path);

    void remove_built_script(const filepath& script_asset_path);
    void remove_script_file(const filepath& script_file_path);

    void begin_assembly_refresh(const filepath& dll_path);
    bool refresh_built_script(const filepath& script_asset_path);

    /// what the project file on disk says that this project, loaded from an earlier text of
    ///  it, has not taken
    struct file_notice {
      /// set when the file on disk cannot be read as a project file
      std::string error;
      /// the parts that differ and are only read when a project is opened
      ostd::vector<std::string> needs_reopen;
    };
    /// re-reads the project file after it changed on disk. the metadata and the starting
    ///  scene are taken as the file has them; whatever else differs is left for the next
    ///  time the project is opened, and noted
    void refresh_from_file(const filepath& path);
    inline const opt<file_notice>& get_file_notice() const { return project_file_notice; }

    void add_loaded_scene(natural_t scene_id);
    void remove_loaded_scene(natural_t scene_id);

    void drop_scene(natural_t scene_id, const std::string_view reason);
    void drop_script_project(const std::string_view reason, const std::string_view details = {}, bool can_retry = true);
    /// a build failed without dropping anything: whatever assembly is mounted stays mounted
    void record_script_failure(const std::string_view reason, const std::string_view details);

    const std::string& get_project_name() const { return project_metadata.name; }
    inline const metadata& get_metadata() const { return project_metadata; }

    /// the csproj a rebuild would build: the mounted one, or the one that failed
    filepath script_project_path() const;
    inline const opt<script_failure>& get_script_failure() const { return script_project_failure; }

    inline const filepath& get_project_rc_path() const { return rc_path; }
    inline const filepath get_csproj_path() const { return project_scripts.csproject_path; }

    inline void set_dotnet_assembly(ref<assembly> a) { project_assembly = a; }
    inline state get_state() const { return current_state; }
    inline bool is_empty() const { return current_state == EMPTY; }
    inline bool is_loading() const { return current_state == LOADING; }
    inline bool is_loaded() const { return current_state == LOADED; }
    inline bool load_failed() const { return current_state == LOAD_FAILED; }
    inline bool is_unloading() const { return current_state == UNLOADING; }

    inline bool expects_script_project() const { return has_script_project; }
    inline bool script_project_mounted() const {
      return project_assembly != nullptr;
    }
    inline bool script_project_unmounted() const {
      return project_scripts.cs_script_source.empty() &&
        project_scripts.cs_scripts.empty() && project_scripts.lua_scripts.empty();
    }
    inline bool scene_graph_loaded() const { return all_scenes_loaded; }
    inline bool scene_graph_unloaded() const { return all_scenes_unloaded; }

    inline natural_t get_starting_scene_id() const { return starting_scene_id; }
    inline std::span<project::scene> get_scenes() { return scenes_in_project; }
    inline const std::span<const project::scene> get_scenes() const { return scenes_in_project; }

    inline std::span<const load_error> get_load_errors() const { return load_errors; }

   private:
    struct project_args {
      std::string name;
      std::string working_directory;
    };

    state current_state = EMPTY;

    project_system* system = nullptr;
    metadata project_metadata;
    arena_buffer file_buffer;

    /// PROJECT-scope mount created at load, removed at unload
    std::string project_mount_name;

    filepath rc_path;
    ref<file_handle> project_file_handle;
    ref<assembly> project_assembly;
    script_data project_scripts;

    natural_t starting_scene_id = 0;
    ostd::vector<scene> scenes_in_project;
    bool all_scenes_loaded = false;
    bool all_scenes_unloaded = true;

    // assume there is one since that is default during project creation
    bool has_script_project = true;

    ostd::vector<load_error> load_errors;
    opt<script_failure> script_project_failure;

    /// [scripting] assembly: a built assembly that is loaded as it is. while it is set nothing
    ///  is built and the project has no csproj
    filepath prebuilt_assembly;

    /// the project file's text as it was when the project was loaded
    std::string loaded_file_text;
    opt<file_notice> project_file_notice;

    void clear_script_failure();

    void attach_project_dll(const filepath& dll_path);
    void attach_project_cs_file(const filepath& cs_file);
    void attach_project_lua_file(const filepath& cs_file);

    void detach_project_dll(const filepath& dll_path);
    void detach_project_cs_file(const filepath& cs_file);
    void detach_project_lua_file(const filepath& lua_file);

    void process_project_plugins(const toml::table& table);
    bool process_scripting_sections(const toml::table& table, driver_kernel* kernel);
    bool process_scene_sections(const toml::table& table);
    bool process_scenes_table(toml::node_view<const toml::node> scenes_node);
    void process_scene_graph(toml::node_view<const toml::node> graph_node);
  };

  // struct project_description {
  //   enum type : uint16_t {
  //     APPLICATION = 0,
  //     MODULE,

  //     NUM_PROJECT_TYPES,
  //     INVALID_PROJECT_TYPE = NUM_PROJECT_TYPES,
  //   };
  //   type project_type = APPLICATION;
  //   std::string project_name = "NewProject";

  //   filepath environment_config = "${project-directory}/${project-name}.toml";
  //   filepath working_directory = "${project-directory}";
  //   filepath output_directory = "${project-directory}/build";
  //   filepath exe_name = "${project-directory}/${project-name}.exe";

  //   ostd::vector<std::string> configurations = { "Debug", "Release" };

  //   size_t active_configuration = 0;
  //   ostd::vector<std::string> cmd_args = {};

  //   std::string version = "0.1.0";
  //   std::string description = "An Other project.";
  //   std::string author = "Author Name";

  //   std::string license = "MIT";
  // };

}  // namespace other

#endif  // OTHERLIB_PROJECT_PROJECT_HPP