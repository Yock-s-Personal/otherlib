/**
 * \file ui/scene-graph-editor/scene_graph_editor.hpp
 **/
#ifndef OTHER_EDITOR_UI_SCENE_GRAPH_EDITOR_SCENE_GRAPH_EDITOR_HPP
#define OTHER_EDITOR_UI_SCENE_GRAPH_EDITOR_SCENE_GRAPH_EDITOR_HPP

#include "ui/ui_window.hpp"

#include "editor_context.hpp"

namespace other {

  class driver;

  namespace ui {

    /// lists the scenes in the scene graph and switches the active one
    /// \todo grow into the full graph editor: edit edges, attach transition scripts
    class scene_graph_editor : public ui_window {
     public:
      scene_graph_editor(editor_context& ctx, event_system& events, driver* drvr);
      virtual ~scene_graph_editor() = default;

     protected:
      void on_render_body() override;

     private:
      editor_context& editor_ctx;
      driver* driver_ptr = nullptr;
    };

  }  // namespace ui
}  // namespace other

#endif  // OTHER_EDITOR_UI_SCENE_GRAPH_EDITOR_SCENE_GRAPH_EDITOR_HPP
