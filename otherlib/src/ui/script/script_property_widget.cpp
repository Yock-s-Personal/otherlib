/**
 * \file ui/script/script_property_widget.cpp
 **/
#include "ui/script/script_property_widget.hpp"

#include <cstring>

#include <glm/gtc/type_ptr.hpp>

#include "core/logger.hpp"
#include "core/profiler.hpp"
#include "core/subsystem.hpp"

#include "script/behavior_fields.hpp"
#include "script/scripting_environment.hpp"

#include "theme/colors.hpp"
#include "ui/inspector_widgets.hpp"
#include "ui/ui_helpers.hpp"

#include "imgui.h"

namespace other {
  namespace ui {
    namespace {

      /// text longer than this is shown and not edited, so an edit cannot cut it short
      constexpr size_t kTextEditSize = 256;

      bool behavior_section_open = false;

      bool begin_behavior_section(const std::string& behavior_name, const std::string& name_space, size_t behavior_index) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 cursor = ImGui::GetCursorScreenPos();
        const float avail_w = ImGui::GetContentRegionAvail().x;

        constexpr float kBehaviorHeaderHeight = 24.f;
        constexpr float kBehaviorDotRadius = 2.5f;

        ImGui::PushID(static_cast<int>(behavior_index));

        const ImVec2 header_min = cursor;
        const ImVec2 header_max = { cursor.x + avail_w, cursor.y + kBehaviorHeaderHeight };

        const bool hovered = ImGui::IsMouseHoveringRect(header_min, header_max);
        dl->AddRectFilled(header_min, header_max, colors::to_im_col(hovered ? glm::vec4(0.18f, 0.18f, 0.20f, 1.f) : glm::vec4(0.14f, 0.14f, 0.16f, 1.f)));

        const float dot_cx = cursor.x + inspector::kInnerPadding + 4.f;
        const float dot_cy = cursor.y + kBehaviorHeaderHeight * 0.5f;
        dl->AddCircleFilled({ dot_cx, dot_cy }, kBehaviorDotRadius, colors::to_im_col(colors::scene_object::kComponentScript));

        const float text_x = dot_cx + kBehaviorDotRadius + 8.f;
        const float text_y = cursor.y + (kBehaviorHeaderHeight - ImGui::GetFontSize()) * 0.5f;
        dl->AddText({ text_x, text_y }, colors::to_im_col(glm::vec4(0.85f, 0.87f, 0.90f, 1.f)), behavior_name.c_str());

        if (!name_space.empty()) {
          const float ns_w = ImGui::CalcTextSize(name_space.c_str()).x;
          dl->AddText({ cursor.x + avail_w - inspector::kInnerPadding - ns_w, text_y }, colors::to_im_col(glm::vec4(0.45f, 0.45f, 0.50f, 1.f)), name_space.c_str());
        }

        ImGui::SetCursorScreenPos(header_min);
        ImGuiStorage* storage = ImGui::GetStateStorage();
        const ImGuiID state_id = ImGui::GetID("##beh_open");
        if (ImGui::InvisibleButton("##beh_hdr", { avail_w, kBehaviorHeaderHeight })) {
          storage->SetBool(state_id, !storage->GetBool(state_id, true));
        }
        behavior_section_open = storage->GetBool(state_id, true);

        dl->AddLine({ cursor.x, header_max.y }, { cursor.x + avail_w, header_max.y }, colors::to_im_col(glm::vec4(0.22f, 0.22f, 0.24f, 1.f)));
        ImGui::SetCursorScreenPos({ cursor.x, header_max.y + 1.f });
        if (behavior_section_open) {
          shift_cursor_y(2.f);
        }
        return behavior_section_open;
      }

      void end_behavior_section() {
        if (behavior_section_open) {
          shift_cursor_y(4.f);
        }
        ImGui::PopID();
      }

      void draw_no_behaviors_message() {
        scoped_color text_col(ImGuiCol_Text, colors::rgba_to_imvec4(colors::kTextDisabled));
        shift_cursor_y(4.f);
        ImGui::TextWrapped("No behaviors attached.");
        shift_cursor_y(4.f);
      }

      void draw_field_tooltip(const std::string& tooltip) {
        if (!tooltip.empty() && ImGui::IsItemHovered()) {
          ImGui::BeginTooltip();
          ImGui::TextUnformatted(tooltip.c_str());
          ImGui::EndTooltip();
        }
      }

      /// what a field holds, as text, for one that is only shown
      std::string field_text(const behavior_fields::field& f, scene* in_scene) {
        switch (f.kind) {
          case behavior_field_kind::BOOL: {
            uint8_t v = 0;
            return f.value(v) && v != 0 ? "true" : "false";
          }

          case behavior_field_kind::INT32: {
            int32_t v = 0;
            return f.value(v) ? std::format("{}", v) : "";
          }

          case behavior_field_kind::UINT32: {
            uint32_t v = 0;
            return f.value(v) ? std::format("{}", v) : "";
          }

          case behavior_field_kind::INT64: {
            int64_t v = 0;
            return f.value(v) ? std::format("{}", v) : "";
          }

          case behavior_field_kind::UINT64: {
            uint64_t v = 0;
            return f.value(v) ? std::format("{}", v) : "";
          }

          case behavior_field_kind::FLOAT: {
            float v = 0.f;
            return f.value(v) ? std::format("{:.3f}", v) : "";
          }

          case behavior_field_kind::DOUBLE: {
            double v = 0.0;
            return f.value(v) ? std::format("{:.3f}", v) : "";
          }

          case behavior_field_kind::TEXT:
          case behavior_field_kind::ENUM: return std::string{ f.text() };
          case behavior_field_kind::ASSET: return f.body.empty() ? "<none>" : std::string{ f.text() };

          case behavior_field_kind::OBJECT: {
            natural_t id = 0;
            if (!f.value(id) || id == 0) {
              return "<none>";
            }
            const scene_object* named = in_scene != nullptr ? in_scene->find_object(id) : nullptr;
            return named != nullptr ? named->name : "<gone>";
          }

          case behavior_field_kind::VEC2:
          case behavior_field_kind::VEC3:
          case behavior_field_kind::VEC4:
          case behavior_field_kind::QUAT: {
            std::string text = "";
            for (size_t i = 0; i + sizeof(float) <= f.body.size(); i += sizeof(float)) {
              float component = 0.f;
              std::memcpy(&component, f.body.data() + i, sizeof(float));
              text += std::format("{}{:.3f}", i == 0 ? "" : ", ", component);
            }
            return text;
          }

          default: return "";
        }
      }

      /// a row that shows what a field names and lets it be made to name nothing. assigning
      ///  one comes with the hierarchy and the asset browser
      bool draw_reference(const std::string& label, const std::string& text, bool holds_one) {
        inspector::begin_property_row(label);
        ImGui::TextUnformatted(text.c_str());
        bool cleared = false;
        if (holds_one) {
          ImGui::SameLine();
          cleared = ImGui::SmallButton("clear");
        }
        inspector::end_property_row();
        return cleared;
      }

      void draw_fields(const ostd::vector<behavior_field_description>& descriptions, const ostd::vector<behavior_fields::field>& fields, scene* in_scene, behavior_fields::writer& edit);

      /// one field. when it is changed, the field with its new value goes into edit
      void draw_field(const behavior_field_description& d, const behavior_fields::field& f, scene* in_scene, behavior_fields::writer& edit) {
        const std::string& label = d.label.empty() ? d.name : d.label;
        const std::string id = std::format("##{}", d.name);

        if (d.read_only && d.kind != behavior_field_kind::STRUCT) {
          inspector::property_display(label, field_text(f, in_scene), colors::kTextMuted);
          draw_field_tooltip(d.tooltip);
          return;
        }

        switch (d.kind) {
          case behavior_field_kind::BOOL: {
            uint8_t v = 0;
            (void)f.value(v);
            bool on = v != 0;
            if (inspector::property_bool(label, on)) {
              edit.add_value<uint8_t>(d.name, d.kind, on ? 1 : 0);
            }
          } break;

          case behavior_field_kind::INT32: {
            int32_t v = 0;
            (void)f.value(v);
            if (inspector::property_int32(label, v)) {
              edit.add_value(d.name, d.kind, v);
            }
          } break;

          case behavior_field_kind::UINT32: {
            uint32_t v = 0;
            (void)f.value(v);
            if (inspector::property_uint32(label, v)) {
              edit.add_value(d.name, d.kind, v);
            }
          } break;

          case behavior_field_kind::INT64: {
            int64_t v = 0;
            (void)f.value(v);
            if (inspector::property_int64(label, v)) {
              edit.add_value(d.name, d.kind, v);
            }
          } break;

          case behavior_field_kind::UINT64: {
            uint64_t v = 0;
            (void)f.value(v);
            if (inspector::property_uint64(label, v)) {
              edit.add_value(d.name, d.kind, v);
            }
          } break;

          case behavior_field_kind::FLOAT:
          case behavior_field_kind::DOUBLE: {
            /// a double is edited as a float: the widgets are floats
            float v = 0.f;
            double wide = 0.0;
            if (d.kind == behavior_field_kind::DOUBLE && f.value(wide)) {
              v = static_cast<float>(wide);
            } else {
              (void)f.value(v);
            }

            bool changed = false;
            if (d.has_range) {
              inspector::begin_property_row(label);
              changed = ImGui::SliderFloat(id.c_str(), &v, d.range_min, d.range_max);
              inspector::end_property_row();
            } else {
              changed = inspector::property_float(label, v);
            }
            if (changed && d.kind == behavior_field_kind::DOUBLE) {
              edit.add_value(d.name, d.kind, static_cast<double>(v));
            } else if (changed) {
              edit.add_value(d.name, d.kind, v);
            }
          } break;

          case behavior_field_kind::TEXT: {
            if (f.body.size() >= kTextEditSize) {
              inspector::property_display(label, std::string{ f.text() }, colors::kTextMuted);
              break;
            }
            char text[kTextEditSize] = {};
            std::memcpy(text, f.body.data(), f.body.size());
            if (inspector::property_text(label, text, sizeof(text))) {
              edit.add_text(d.name, d.kind, std::string_view{ text, std::strlen(text) });
            }
          } break;

          case behavior_field_kind::ENUM: {
            const std::string current{ f.text() };
            inspector::begin_property_row(label);
            if (ImGui::BeginCombo(id.c_str(), current.c_str())) {
              for (const std::string& member : d.members) {
                if (ImGui::Selectable(member.c_str(), member == current) && member != current) {
                  edit.add_text(d.name, d.kind, member);
                }
              }
              ImGui::EndCombo();
            }
            inspector::end_property_row();
          } break;

          case behavior_field_kind::VEC2: {
            glm::vec2 v = {};
            if (f.value(v) && inspector::property_vec2(label, v)) {
              edit.add_value(d.name, d.kind, v);
            }
          } break;

          case behavior_field_kind::VEC3: {
            glm::vec3 v = {};
            if (!f.value(v)) {
              break;
            }
            bool changed = false;
            if (d.color) {
              inspector::begin_property_row(label);
              changed = ImGui::ColorEdit3(id.c_str(), glm::value_ptr(v), ImGuiColorEditFlags_Float);
              inspector::end_property_row();
            } else {
              changed = inspector::property_vec3(label, v);
            }
            if (changed) {
              edit.add_value(d.name, d.kind, v);
            }
          } break;

          case behavior_field_kind::VEC4:
          case behavior_field_kind::QUAT: {
            /// a rotation is edited as the four numbers it is saved as
            glm::vec4 v = {};
            if (!f.value(v)) {
              break;
            }
            bool changed = false;
            if (d.color) {
              inspector::begin_property_row(label);
              changed = ImGui::ColorEdit4(id.c_str(), glm::value_ptr(v), ImGuiColorEditFlags_Float);
              inspector::end_property_row();
            } else {
              changed = inspector::property_vec4(label, v, 0.1f);
            }
            if (changed) {
              edit.add_value(d.name, d.kind, v);
            }
          } break;

          case behavior_field_kind::ASSET: {
            if (draw_reference(label, field_text(f, in_scene), !f.body.empty())) {
              edit.add_text(d.name, d.kind, "");
            }
          } break;

          case behavior_field_kind::OBJECT: {
            natural_t held = 0;
            (void)f.value(held);
            if (draw_reference(label, field_text(f, in_scene), held != 0)) {
              edit.add_value<natural_t>(d.name, d.kind, 0);
            }
          } break;

          case behavior_field_kind::STRUCT: {
            if (!ImGui::TreeNodeEx(d.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen, "%s", label.c_str())) {
              break;
            }
            ostd::vector<behavior_fields::field> nested = {};
            behavior_fields::writer nested_edit = {};
            if (behavior_fields::read(f.body, nested)) {
              draw_fields(d.fields, nested, in_scene, nested_edit);
            }
            /// only what changed is given back: the struct's other fields are left as they are
            if (nested_edit.count() > 0) {
              edit.add(d.name, d.kind, nested_edit.take());
            }
            ImGui::TreePop();
          } break;

          default: inspector::property_display(label, "<a kind this build cannot draw>", colors::kTextDisabled); break;
        }

        draw_field_tooltip(d.tooltip);
      }

      void draw_fields(const ostd::vector<behavior_field_description>& descriptions, const ostd::vector<behavior_fields::field>& fields, scene* in_scene, behavior_fields::writer& edit) {
        const std::string* group = nullptr;
        bool group_open = true;
        for (const behavior_field_description& d : descriptions) {
          /// the fields that share a group are under one node, which can be folded away
          if (group == nullptr || *group != d.group) {
            if (group != nullptr && !group->empty() && group_open) {
              ImGui::TreePop();
            }
            group = &d.group;
            group_open = d.group.empty() || ImGui::TreeNodeEx(d.group.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
          }
          if (!group_open) {
            continue;
          }

          if (d.separator) {
            ImGui::Separator();
          }
          const auto found = std::ranges::find(fields, std::string_view{ d.name }, &behavior_fields::field::name);
          const std::string& label = d.label.empty() ? d.name : d.label;
          ImGui::PushID(d.name.c_str());
          if (found == fields.end() || found->kind != d.kind) {
            inspector::property_display(label, "<unreadable>", colors::kTextDisabled);
          } else {
            draw_field(d, *found, in_scene, edit);
          }
          ImGui::PopID();
        }
        if (group != nullptr && !group->empty() && group_open) {
          ImGui::TreePop();
        }
      }

      bool draw_behavior(scripting_environment& env, integer_t script_object_id, size_t index, scene* in_scene) {
        script_object* script_obj = env.get_object(script_object_id);
        if (script_obj == nullptr || index >= script_obj->behavior_handles.size()) {
          return false;
        }
        /// copied: a change goes through the environment, which may move the list
        const std::string type_name = script_obj->behavior_handles[index].type_name;
        const bool was_enabled = script_obj->behavior_handles[index].enabled;
        const bool has_instance = script_obj->behavior_handles[index].instance != nullptr;

        const size_t dot = type_name.rfind('.');
        const std::string class_name = dot == std::string::npos ? type_name : type_name.substr(dot + 1);
        const std::string name_space = dot == std::string::npos ? std::string{} : type_name.substr(0, dot);

        bool changed = false;
        if (begin_behavior_section(class_name, name_space, index)) {
          bool enabled = was_enabled;
          if (inspector::property_bool("Enabled", enabled)) {
            env.set_behavior_enabled(script_object_id, type_name, enabled);
            changed = true;
          }

          if (!has_instance) {
            inspector::property_display("Status", "not built; its saved values are kept", colors::kTextDisabled);
          } else {
            const ostd::vector<uint8_t> stream = env.capture_behavior_fields(script_object_id, type_name);
            ostd::vector<behavior_fields::field> fields = {};
            behavior_fields::writer edit = {};
            if (behavior_fields::read(stream, fields)) {
              draw_fields(env.describe_behavior(script_object_id, type_name), fields, in_scene, edit);
            }
            if (edit.count() > 0) {
              env.apply_behavior_fields(script_object_id, type_name, edit.take());
              changed = true;
            }
          }
        }
        end_behavior_section();
        return changed;
      }

    }  // namespace

    bool draw_script_behaviors(script_component& component, scene* in_scene) {
      PROFILE_SECTION("ui::draw_script_behaviors");
      auto* env = subsystem<scripting_environment>::get();
      if (env == nullptr) {
        inspector::property_display("Status", "Scripting environment unavailable", colors::kTextDisabled);
        return false;
      }

      inspector::property_display("Script ID", std::format("{}", component.script_object_id), colors::kTextMuted);
      script_object* script_obj = component.script_object_id >= 0 ? env->get_object(component.script_object_id) : nullptr;
      if (script_obj == nullptr) {
        inspector::property_display("Status", "No Script Object Attached", colors::kTextDisabled);
        return false;
      }
      if (script_obj->behavior_handles.empty()) {
        draw_no_behaviors_message();
        return false;
      }

      bool changed = false;
      for (size_t i = 0; i < script_obj->behavior_handles.size(); ++i) {
        changed |= draw_behavior(*env, component.script_object_id, i, in_scene);
      }
      return changed;
    }

  }  // namespace ui
}  // namespace other
