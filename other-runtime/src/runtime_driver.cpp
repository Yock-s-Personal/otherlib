/**
 * \file runtime_driver.cpp
 **/
#include "runtime_driver.hpp"

#include <filesystem>
#include <format>

#include "core/profiler.hpp"
#include "file/path_helpers.hpp"

#include "renderer/render_pipeline.hpp"

#include "driver/systems/project_system.hpp"
#include "driver/systems/vm_system.hpp"
#include "project/project.hpp"

namespace other {

  runtime_driver::runtime_driver(const command_line& cmd, const config_table& config)
      : driver(cmd, config),
        main_pipeline_name(config.get_value<std::string>("rendering.main-pipeline", "default-instancing")),
        pipelines_outstanding(config.get_value<ostd::vector<std::string>>("rendering.pipelines", {}).size()) {
  }

  void runtime_driver::on_initialize() {
    PROFILE_SECTION("runtime_driver::on_initialize");
    const bool has_boot_file = !get_config_value<std::string>("driver.boot-file").empty();
    if (!has_boot_file && !configuration().project_file.has_value()) {
      fail("the config has neither driver.boot-file nor application.project-file, so no project can load");
    }

    /// the asset loader makes the same test and raises no event for a path it rejects, so a
    ///  missing file would otherwise never be counted as settled
    const ostd::vector<std::string> pipeline_paths = get_config_value<ostd::vector<std::string>>("rendering.pipelines");
    if (pipeline_paths.empty()) {
      fail("rendering.pipelines is empty, so there is nothing to draw with");
    }

    /// found the way the rendering system finds them, so engine://... is a path here too
    for (const std::string& path : pipeline_paths) {
      if (!resolve_asset_path(path).found()) {
        fail(std::format("rendering pipeline '{}' does not exist", path));
      }
    }

    auto& events = *get_event_system();
    /// the driver's own project.loaded listener was added first, so the starting scene is
    ///  already active when this one runs
    events.add_listener("project.loaded", [this](const value&) {
      const project& loaded = core_system<project_system>().get_project();
      if (const auto errors = loaded.get_load_errors(); !errors.empty()) {
        fail(std::format("the project loaded with {} error(s), the first being '{}': {}", errors.size(), errors.front().subject, errors.front().message));
      } else if (loaded.get_starting_scene_id() == 0) {
        fail("the project has no starting scene");
      }
    });
    events.add_listener("project.load-failed", [this](const value&) {
      fail("the project file could not be read");
    });
    events.add_listener("rendering-pipeline.asset-load-failed", [this](const value&) {
      fail("a pipeline listed in rendering.pipelines failed to load");
    });
  }

  void runtime_driver::on_rendering_pipeline_loaded(natural_t asset_id, render_pipeline* pipeline) {
    PROFILE_SECTION("runtime_driver::on_rendering_pipeline_loaded");
    if (pipeline->get_name() == main_pipeline_name) {
      /// asked once, here: get_screen_texture asserts on a pipeline with no display texture
      if (pipeline->get_final_output_texture().has_value()) {
        main_pipeline = pipeline;
      } else {
        fail(std::format("rendering pipeline '{}' has no display texture to present", main_pipeline_name));
      }
    }

    if (pipelines_outstanding > 0 && --pipelines_outstanding == 0 && main_pipeline == nullptr) {
      fail(std::format("rendering.main-pipeline '{}' is not one of the pipelines in rendering.pipelines", main_pipeline_name));
    }
  }

  void runtime_driver::on_rendering_pipeline_unloaded(natural_t asset_id, render_pipeline* pipeline) {
    /// the renderer frees the pipeline straight after this hook
    if (pipeline == main_pipeline) {
      main_pipeline = nullptr;
    }
  }

  void runtime_driver::on_scene_activated(natural_t scene_id) {
    warned_no_camera = false;
  }

  void runtime_driver::update_running() {
    PROFILE_SECTION("runtime_driver::update_running");
    if (!failure.has_value()) {
      check_boot();
    }

    /// acted on here and not where it was found: listeners run inside a system tick, and a
    ///  shutdown started from on_initialize would meet the READY event that follows it
    if (failure.has_value()) {
      request_failure_shutdown(*failure);
      failure.reset();
    }
  }

  void runtime_driver::on_begin_frame(render_data* frame_data) {
    PROFILE_SECTION("runtime_driver::on_begin_frame");
    frame_presentable = false;
    /// null until a scene is active
    if (frame_data == nullptr) {
      return;
    }

    if (frame_data->primary_camera == nullptr) {
      if (!warned_no_camera) {
        CORE_LOG_ERROR("The active scene has no object tagged 'main-camera'; nothing will be drawn until it has one.");
        warned_no_camera = true;
      }
      return;
    }

    /// zero while minimised
    const glm::ivec2 window_size = get_renderer().get_window_size();
    if (window_size.x <= 0 || window_size.y <= 0) {
      return;
    }

    /// no viewport does this for the runtime. the pipeline keeps its startup size and the
    ///  present stretches it over the window, so the projection takes the window's aspect
    frame_data->primary_camera->set_viewport_size(glm::vec2(window_size));
    frame_data->primary_camera->calculate_matrices(window_size);
    frame_presentable = true;
  }

  void runtime_driver::on_render() {
    PROFILE_SECTION("runtime_driver::on_render");
    if (!frame_presentable || main_pipeline == nullptr || !main_pipeline->is_valid()) {
      return;
    }
    get_renderer().present_texture(main_pipeline->get_screen_texture());
  }

  void runtime_driver::on_shutdown() {
    PROFILE_SECTION("runtime_driver::on_shutdown");
    main_pipeline = nullptr;
  }

  void runtime_driver::fail(std::string reason) {
    /// the first reason is the cause; later ones are usually its consequences
    if (!failure.has_value()) {
      failure = std::move(reason);
    }
  }

  void runtime_driver::check_boot() {
    if (project_seen) {
      return;
    }

    if (!core_system<project_system>().project_empty()) {
      project_seen = true;
      return;
    }

    /// the vm ticks before this in the frame, so a boot program that loads a project has
    ///  done so by now. idle with nothing loaded covers a missing boot file, one that does
    ///  not assemble, a failed project.load and a folder with no project file
    auto& vm = core_system<vm_system>();
    if (vm.boot_attempted() && vm.idle()) {
      fail("nothing loaded a project: the boot program ended, or never ran, and the project is still empty");
    }
  }

}  // namespace other

OTHER_DRIVER(other::runtime_driver);