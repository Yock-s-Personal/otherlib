/**
 * \file services/editor_handles_bindings.cpp
 * the Scene View's handles as Other.Editor.Handles asks for them, and what a tool draws
 * for them: shapes of the world drawn over the view's picture, into the window that is
 * being drawn, which while the tools run is the view's
 **/
#include <algorithm>
#include <cmath>
#include <numbers>

#include <imgui/imgui.h>

#include "core/profiler.hpp"

#include "renderer/camera.hpp"

#include "services/editor_bindings.hpp"
#include "services/editor_services.hpp"
#include "services/scene_handles.hpp"
#include "services/scene_pick.hpp"

namespace other {
  namespace bindings {
    namespace {

      constexpr uint32_t kAnyHot = 1u << 0;
      constexpr uint32_t kAnyActive = 1u << 1;
      constexpr int32_t kCircleSegments = 64;
      constexpr float kWholeTurn = 2.f * std::numbers::pi_v<float>;
      /// an arrow's head, in the picture's units: how long, and how wide at its back
      constexpr float kHeadLength = 12.f;
      constexpr float kHeadWidth = 5.f;

      glm::vec3 vec(const float* v) {
        return { v[0], v[1], v[2] };
      }

      /// the services while the view's tools are being run: only then are there handles to
      ///  ask for, and a window of the view's to draw into
      editor_services* asking() {
        editor_services* editor = get_editor_services();
        return editor != nullptr && editor->handles.in_frame() ? editor : nullptr;
      }

      /// what is drawn with, and where the view's picture is on the screen
      struct painter {
        const camera* cam = nullptr;
        glm::vec2 size = { 0.f, 0.f };
        ImVec2 origin = { 0.f, 0.f };
        ImDrawList* list = nullptr;

        ImVec2 at(const glm::vec2& place) const {
          return { origin.x + place.x, origin.y + place.y };
        }
      };

      opt<painter> paint() {
        editor_services* editor = asking();
        if (editor == nullptr || editor->view_camera == nullptr || ImGui::GetCurrentContext() == nullptr) {
          return std::nullopt;
        }
        return painter{
          .cam = editor->view_camera,
          .size = editor->view.size,
          .origin = { editor->view.position.x, editor->view.position.y },
          .list = ImGui::GetWindowDrawList(),
        };
      }

    }  // namespace

    uint32_t native_editor_handles_state() {
      const editor_services* editor = get_editor_services();
      if (editor == nullptr) {
        return 0;
      }
      return (editor->handles.any_hot() ? kAnyHot : 0u) | (editor->handles.any_active() ? kAnyActive : 0u);
    }

    void native_editor_handles_snaps(float* move, float* turn_degrees, float* scale) {
      const editor_services* editor = get_editor_services();
      if (editor == nullptr || move == nullptr || turn_degrees == nullptr || scale == nullptr) {
        return;
      }
      *move = editor->snaps.move;
      *turn_degrees = editor->snaps.turn_degrees;
      *scale = editor->snaps.scale;
    }

    uint32_t native_editor_handles_slide_axis(uint64_t id, float* position, float* axis, float reach, float snap) {
      editor_services* editor = asking();
      if (editor == nullptr || position == nullptr || axis == nullptr) {
        return 0;
      }
      glm::vec3 moved = vec(position);
      const uint32_t flags = editor->handles.slide_axis(id, moved, vec(axis), reach, snap);
      position[0] = moved.x;
      position[1] = moved.y;
      position[2] = moved.z;
      return flags;
    }

    uint32_t native_editor_handles_slide_plane(uint64_t id, float* position, float* u, float* v, float near_edge, float far_edge, float snap) {
      editor_services* editor = asking();
      if (editor == nullptr || position == nullptr || u == nullptr || v == nullptr) {
        return 0;
      }
      glm::vec3 moved = vec(position);
      const uint32_t flags = editor->handles.slide_plane(id, moved, vec(u), vec(v), near_edge, far_edge, snap);
      position[0] = moved.x;
      position[1] = moved.y;
      position[2] = moved.z;
      return flags;
    }

    uint32_t native_editor_handles_turn(uint64_t id, float* centre, float* axis, float radius, float* angle, float snap, uint32_t near_half_only) {
      editor_services* editor = asking();
      if (editor == nullptr || centre == nullptr || axis == nullptr || angle == nullptr) {
        return 0;
      }
      return editor->handles.turn(id, vec(centre), vec(axis), radius, *angle, snap, near_half_only != 0);
    }

    nbool32 native_editor_handles_grab_direction(float* direction) {
      const editor_services* editor = get_editor_services();
      if (editor == nullptr || direction == nullptr || !editor->handles.any_active()) {
        return false;
      }
      const glm::vec3& grabbed = editor->handles.grab_direction();
      direction[0] = grabbed.x;
      direction[1] = grabbed.y;
      direction[2] = grabbed.z;
      return true;
    }

    uint32_t native_editor_handles_stretch_axis(uint64_t id, float* centre, float* axis, float reach, float* factor, float snap) {
      editor_services* editor = asking();
      if (editor == nullptr || centre == nullptr || axis == nullptr || factor == nullptr) {
        return 0;
      }
      return editor->handles.stretch_axis(id, vec(centre), vec(axis), reach, *factor, snap);
    }

    uint32_t native_editor_handles_stretch_all(uint64_t id, float* centre, float radius_pixels, float* factor, float snap) {
      editor_services* editor = asking();
      if (editor == nullptr || centre == nullptr || factor == nullptr) {
        return 0;
      }
      return editor->handles.stretch_all(id, vec(centre), radius_pixels, *factor, snap);
    }

    void native_editor_handles_line(float* a, float* b, uint32_t color, float thickness) {
      const opt<painter> p = paint();
      if (!p.has_value() || a == nullptr || b == nullptr) {
        return;
      }
      const opt<std::pair<glm::vec2, glm::vec2>> shown = view_segment(*p->cam, p->size, vec(a), vec(b));
      if (shown.has_value()) {
        p->list->AddLine(p->at(shown->first), p->at(shown->second), color, thickness);
      }
    }

    void native_editor_handles_arrow(float* from, float* to, uint32_t color, float thickness) {
      const opt<painter> p = paint();
      if (!p.has_value() || from == nullptr || to == nullptr) {
        return;
      }
      const opt<std::pair<glm::vec2, glm::vec2>> shown = view_segment(*p->cam, p->size, vec(from), vec(to));
      if (!shown.has_value()) {
        return;
      }
      p->list->AddLine(p->at(shown->first), p->at(shown->second), color, thickness);

      /// a head only where the far end is in front of the camera, and the arrow is long
      ///  enough on the screen to have a way it points
      const opt<glm::vec2> tip = view_place(*p->cam, p->size, vec(to));
      const glm::vec2 along = shown->second - shown->first;
      const float length = glm::length(along);
      if (!tip.has_value() || length < 1.f) {
        return;
      }
      const glm::vec2 way = along / length;
      const glm::vec2 across{ -way.y, way.x };
      const glm::vec2 back = *tip - way * kHeadLength;
      p->list->AddTriangleFilled(p->at(*tip + way * 2.f), p->at(back + across * kHeadWidth), p->at(back - across * kHeadWidth), color);
    }

    void native_editor_handles_circle(float* centre, float* axis, float radius, uint32_t color, float thickness) {
      PROFILE_SECTION_VERBOSE("bindings::native_editor_handles_circle");
      const opt<painter> p = paint();
      if (!p.has_value() || centre == nullptr || axis == nullptr) {
        return;
      }
      const glm::vec3 c = vec(centre);
      const glm::vec3 n = glm::normalize(vec(axis));
      const glm::vec3 other_way = std::abs(n.y) < 0.99f ? glm::vec3{ 0.f, 1.f, 0.f } : glm::vec3{ 1.f, 0.f, 0.f };
      const glm::vec3 first = glm::normalize(glm::cross(other_way, n));
      const glm::vec3 second = glm::cross(n, first);

      glm::vec3 from = c + first * radius;
      for (int32_t i = 1; i <= kCircleSegments; ++i) {
        const float a = 2.f * std::numbers::pi_v<float> * static_cast<float>(i) / static_cast<float>(kCircleSegments);
        const glm::vec3 to = c + (first * std::cos(a) + second * std::sin(a)) * radius;
        const opt<std::pair<glm::vec2, glm::vec2>> shown = view_segment(*p->cam, p->size, from, to);
        if (shown.has_value()) {
          p->list->AddLine(p->at(shown->first), p->at(shown->second), color, thickness);
        }
        from = to;
      }
    }

    void native_editor_handles_arc(float* centre, float* axis, float* start, float radians, float radius, uint32_t color, float thickness) {
      PROFILE_SECTION_VERBOSE("bindings::native_editor_handles_arc");
      const opt<painter> p = paint();
      if (!p.has_value() || centre == nullptr || axis == nullptr || start == nullptr) {
        return;
      }
      const glm::vec3 c = vec(centre);
      const glm::vec3 first = glm::normalize(vec(start));
      const glm::vec3 second = glm::cross(glm::normalize(vec(axis)), first);
      const float turn = std::clamp(radians, -kWholeTurn, kWholeTurn);
      const int32_t segments = std::max(2, static_cast<int32_t>(std::ceil(std::abs(turn) / kWholeTurn * static_cast<float>(kCircleSegments))));

      glm::vec3 from = c + first * radius;
      for (int32_t i = 1; i <= segments; ++i) {
        const float a = turn * static_cast<float>(i) / static_cast<float>(segments);
        const glm::vec3 to = c + (first * std::cos(a) + second * std::sin(a)) * radius;
        const opt<std::pair<glm::vec2, glm::vec2>> shown = view_segment(*p->cam, p->size, from, to);
        if (shown.has_value()) {
          p->list->AddLine(p->at(shown->first), p->at(shown->second), color, thickness);
        }
        from = to;
      }
    }

    void native_editor_handles_pie(float* centre, float* axis, float* start, float radians, float radius, uint32_t color) {
      PROFILE_SECTION_VERBOSE("bindings::native_editor_handles_pie");
      const opt<painter> p = paint();
      if (!p.has_value() || centre == nullptr || axis == nullptr || start == nullptr) {
        return;
      }
      const glm::vec3 c = vec(centre);
      const opt<glm::vec2> middle = view_place(*p->cam, p->size, c);
      if (!middle.has_value()) {
        return;
      }
      const glm::vec3 first = glm::normalize(vec(start));
      const glm::vec3 second = glm::cross(glm::normalize(vec(axis)), first);
      const float turn = std::clamp(radians, -kWholeTurn, kWholeTurn);
      const int32_t segments = std::max(2, static_cast<int32_t>(std::ceil(std::abs(turn) / kWholeTurn * static_cast<float>(kCircleSegments))));

      /// a fan of triangles, since more than half a turn is no shape ImGui fills in one
      ///  piece. their edges are not softened: softened, each would show where it meets the
      ///  next through a colour that is seen through
      const ImDrawListFlags flags = p->list->Flags;
      p->list->Flags &= ~ImDrawListFlags_AntiAliasedFill;
      opt<glm::vec2> from = view_place(*p->cam, p->size, c + first * radius);
      for (int32_t i = 1; i <= segments; ++i) {
        const float a = turn * static_cast<float>(i) / static_cast<float>(segments);
        const opt<glm::vec2> to = view_place(*p->cam, p->size, c + (first * std::cos(a) + second * std::sin(a)) * radius);
        if (from.has_value() && to.has_value()) {
          p->list->AddTriangleFilled(p->at(*middle), p->at(*from), p->at(*to), color);
        }
        from = to;
      }
      p->list->Flags = flags;
    }

    void native_editor_handles_quad(float* a, float* b, float* c, float* d, uint32_t color) {
      const opt<painter> p = paint();
      if (!p.has_value() || a == nullptr || b == nullptr || c == nullptr || d == nullptr) {
        return;
      }
      /// all of it in front of the camera, or none of it is drawn
      const float* corners[4] = { a, b, c, d };
      ImVec2 shown[4];
      for (int32_t i = 0; i < 4; ++i) {
        const opt<glm::vec2> place = view_place(*p->cam, p->size, vec(corners[i]));
        if (!place.has_value()) {
          return;
        }
        shown[i] = p->at(*place);
      }
      p->list->AddQuadFilled(shown[0], shown[1], shown[2], shown[3], color);
    }

    void native_editor_handles_dot(float* at, float radius_pixels, uint32_t color) {
      const opt<painter> p = paint();
      if (!p.has_value() || at == nullptr) {
        return;
      }
      const opt<glm::vec2> place = view_place(*p->cam, p->size, vec(at));
      if (place.has_value()) {
        p->list->AddCircleFilled(p->at(*place), radius_pixels, color);
      }
    }

    void native_editor_handles_label(float* at, const char* text, uint32_t color) {
      const opt<painter> p = paint();
      if (!p.has_value() || at == nullptr || text == nullptr) {
        return;
      }
      const opt<glm::vec2> place = view_place(*p->cam, p->size, vec(at));
      if (place.has_value()) {
        p->list->AddText(p->at(*place), color, text);
      }
    }

  }  // namespace bindings
}  // namespace other
