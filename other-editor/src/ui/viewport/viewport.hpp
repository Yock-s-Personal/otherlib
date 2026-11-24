/**
 * \file ui/viewport/viewport.hpp
 **/
#ifndef OTHER_EDITOR_UI_VIEWPORT_VIEWPORT_HPP
#define OTHER_EDITOR_UI_VIEWPORT_VIEWPORT_HPP

#include "gpu_resource/renderer_resource.hpp"
#include "renderer/renderer.hpp"

#include "ui/ui_window.hpp"

#include "editor_context.hpp"

namespace other {

  class driver;

  namespace ui {

    class viewport : public ui_window {
     public:
      viewport(editor_context& ctx, event_system& events, renderer& renderer_instance, driver* driver_ptr);
      ~viewport() override = default;

      bool override_render() override { return true; }
      void custom_render() override;

     private:
      renderer& renderer_instance;
      driver* driver_ptr = nullptr;
      editor_context& editor_ctx;

      /// the Scene View's tools drew in a way ImGui objected to: they are not run again
      ///  until asked, and the view says why
      bool tools_stopped = false;
      std::string tools_stopped_reason;

      void draw_viewport(other::viewport& vp);
      /// what is the Scene View's alone, once its picture is drawn: what its tools ask
      ///  about it, and the tools
      void draw_scene_view(const other::viewport& vp, const ImVec2& position, const ImVec2& size);
      /// an icon for each light and camera of the scene, over the picture
      void draw_icons(const ImVec2& position, const ImVec2& size);
      /// a model dragged from the asset browser and let go on the picture is put in the scene
      void take_dropped_model();

      ostd::vector<editor_services::view_icon> icons;
    };

  }  // namespace ui
}  // namespace other

#endif  // OTHER_EDITOR_UI_VIEWPORT_VIEWPORT_HPP