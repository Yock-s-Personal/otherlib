/**
 * \file ui/project-compiler/project_compiler.hpp
 **/
#ifndef OTHER_EDITOR_UI_PROJECT_COMPILER_PROJECT_COMPILER_HPP
#define OTHER_EDITOR_UI_PROJECT_COMPILER_PROJECT_COMPILER_HPP

#include <array>
#include <mutex>
#include <string>

#include "data-structures/std_container.hpp"

#include "project/project_compiler.hpp"
#include "ui/ui_window.hpp"

namespace other {

  class editor_driver;
  struct editor_context;

  namespace ui {

    /// packages the open project: the choices a compile takes, what it will build from, a
    ///  button that runs it on a worker thread, and what came of it. the compile itself is
    ///  editor_context::project_compile
    class project_compiler : public ui_window {
     public:
      project_compiler(editor_context& ctx, event_system& events, editor_driver* drvr);
      virtual ~project_compiler() = default;

     protected:
      void on_prepare_render() override;
      void on_render_body() override;

     private:
      constexpr static inline size_t kPathBufferSize = 512;
      constexpr static inline size_t kNameBufferSize = 128;

      editor_context& editor_ctx;
      editor_driver* driver_ptr = nullptr;

      /// the project the fields below were filled from; a different one refills them
      filepath shown_project;
      compile_preview preview;

      std::array<char, kPathBufferSize> output_buf = {};
      std::array<char, kNameBufferSize> name_buf = {};
      std::string starting_scene;
      std::string main_pipeline;
      bool debug_package = false;

      /// why a compile could not be started; cleared by the next attempt
      std::string start_error;

      /// the folder dialog answers on a thread of its own
      std::mutex picked_mutex;
      opt<std::string> picked_folder;

      void refresh(const filepath& project_file);
      void take_picked_folder();
      /// why Compile is not offered; empty when it is
      std::string blocking_reason() const;
      /// what the Compile button does: asks first when the active scene has unsaved edits
      void request_compile();
      void start_compile();

      void render_choices();
      void render_contents();
      void render_compile();
      /// asks before a compile of a scene with edits that are not on disk: save, or no compile
      void render_unsaved_prompt();
      void render_result();

      static void on_folder_picked(void* userdata, const char* const* filelist, int32_t filter);
    };

  }  // namespace ui
}  // namespace other

#endif  // OTHER_EDITOR_UI_PROJECT_COMPILER_PROJECT_COMPILER_HPP
