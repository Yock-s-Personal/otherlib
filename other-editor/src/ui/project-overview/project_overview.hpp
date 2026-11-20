/**
 * \file ui/project-overview/project_overview.hpp
 **/
#ifndef OTHER_EDITOR_UI_PROJECT_OVERVIEW_PROJECT_OVERVIEW_HPP
#define OTHER_EDITOR_UI_PROJECT_OVERVIEW_PROJECT_OVERVIEW_HPP

#include <string>

#include "data-structures/std_container.hpp"

#include "ui/ui_window.hpp"

namespace other {

  class driver;
  class project;
  class project_system;

  namespace ui {

    /// project metadata, the state of the project's C# scripts and a way to rebuild them
    /// \todo frame and render timings once the profiler exposes data the editor can read
    class project_overview : public ui_window {
     public:
      project_overview(event_system& events, driver* drvr);
      virtual ~project_overview() = default;

     protected:
      void on_prepare_render() override;
      void on_render_body() override;

     private:
      driver* driver_ptr = nullptr;

      /// the failure text split into display lines; rebuilt only when the text changes
      std::string shown_details;
      ostd::vector<std::string> detail_lines;

      void render_metadata(project_system& projects, const project& current);
      void render_scripts(project_system& projects, const project& current);
      void render_load_errors(const project& current);

      void refresh_detail_lines(const std::string& details, const filepath& csproj);
    };

  }  // namespace ui
}  // namespace other

#endif  // OTHER_EDITOR_UI_PROJECT_OVERVIEW_PROJECT_OVERVIEW_HPP