/**
 * \file services/compile_service.cpp
 **/
#include "services/compile_service.hpp"

#include <algorithm>
#include <filesystem>

#include "core/logger.hpp"
#include "core/profiler.hpp"

#include "driver/driver.hpp"
#include "driver/systems/project_system.hpp"
#include "vm/other_device.hpp"
#include "vm/vm.hpp"

namespace other {
  namespace {

    /// the bus the running VM links against, so a package's boot program is linked against
    ///  the same devices it will find; null before the VM is up
    const command_bus* live_bus() {
      const other_command_device* device = vm::get_initialized_device();
      return device == nullptr ? nullptr : device->bus;
    }

  }  // namespace

  compile_service::state compile_service::poll(driver* drvr) {
    if (drvr == nullptr) {
      return state::NO_PROJECT;
    }
    auto& projects = drvr->get_kernel().get_core_system<project_system>();
    const project& current = projects.get_project();
    const opt<filepath>& project_file = projects.get_project_file();
    if (current.is_empty() || !project_file.has_value()) {
      return state::NO_PROJECT;
    }
    if (!current.is_loaded()) {
      return state::LOADING;
    }

    if (session.poll()) {
      ++stamp;
    }
    if (*project_file != shown_project || projects.project_file_revision() != shown_revision) {
      shown_revision = projects.project_file_revision();
      const bool another_project = *project_file != shown_project;
      shown_project = *project_file;
      if (another_project) {
        std::error_code ec;
        const filepath project_folder = std::filesystem::absolute(shown_project, ec).parent_path();
        default_output = (project_folder.parent_path() / "packages").string();
      }
      refresh();
    }
    return state::READY;
  }

  void compile_service::refresh() {
    PROFILE_SECTION("compile_service::refresh");
    ++stamp;
    const command_bus* bus = live_bus();
    if (bus == nullptr || shown_project.empty()) {
      preview = {};
      preview.problems.push_back("the VM is not running, so a boot program cannot be linked");
      return;
    }
    preview = preview_project_compile(*bus, shown_project);
    CORE_LOG_DEBUG("[COMPILE] : '{}' would link {} VM source(s) with {} hook handler(s), draw with {} pipeline(s) and leave out {} file(s)", shown_project.string(),
                   preview.vm_sources.size(), preview.hooks.size(), preview.pipelines.size(), preview.left_out.size());
  }

  std::string compile_service::start(driver* drvr, const std::string_view public_name, const std::string_view output_folder, const std::string_view starting_scene,
                                     const std::string_view main_pipeline, bool debug) {
    PROFILE_SECTION("compile_service::start");
    const command_bus* bus = live_bus();
    if (drvr == nullptr || bus == nullptr || shown_project.empty()) {
      return "The VM is not running, so a boot program cannot be linked.";
    }

    /// what was chosen is the project's choice from now on. a main pipeline is only a choice
    ///  when there are several
    const compile_choices choices = {
      .public_name = std::string{ public_name },
      .starting_scene = std::string{ starting_scene },
      .main_pipeline = preview.pipelines.size() > 1 ? std::string{ main_pipeline } : std::string{},
    };
    if (const opt<std::string> error = save_compile_choices(shown_project, choices); error.has_value()) {
      return *error;
    }

    compile_settings settings = {
      .project_file = shown_project,
      .output_directory = filepath{ output_folder },
      .public_name = std::string{ public_name },
      .starting_scene = std::string{ starting_scene },
      .main_pipeline = std::string{ main_pipeline },
      .config = debug ? package_config::DEBUG : package_config::RELEASE,
    };
    if (!session.start(drvr->get_job_system(), *bus, std::move(settings))) {
      return "A compile is already running.";
    }
    ++stamp;
    return {};
  }

  compile_service::result compile_service::last_result() const {
    const opt<compile_report>& report = session.last_report();
    if (!report.has_value()) {
      return result::NONE;
    }
    if (report->success) {
      return result::PACKAGED;
    }
    return report->cancelled ? result::CANCELLED : result::FAILED;
  }

}  // namespace other
