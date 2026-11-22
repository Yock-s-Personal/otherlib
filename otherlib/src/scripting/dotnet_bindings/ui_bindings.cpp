/**
 * \file scripting/dotnet_bindings/ui_bindings.cpp
 **/
#include "scripting/dotnet_bindings/ui_bindings.hpp"

#include <string>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include "dotnet/native_string.hpp"

#include "scripting/main_thread_only.hpp"
#include "theme/font.hpp"
#include "theme/theme.hpp"
#include "ui/inspector_widgets.hpp"

namespace other {
  namespace bindings {

    nbool32 native_begin_window(const char* title, int32_t flags) {
      return ImGui::Begin(title, nullptr, static_cast<ImGuiWindowFlags>(flags));
    }

    void native_end_window() {
      ImGui::End();
    }

    nbool32 native_begin_child(const char* str_id, float width, float height, int32_t child_flags, int32_t window_flags) {
      return ImGui::BeginChild(str_id, ImVec2(width, height), static_cast<ImGuiChildFlags>(child_flags), static_cast<ImGuiWindowFlags>(window_flags));
    }

    void native_end_child() {
      ImGui::EndChild();
    }

    void native_ui_text(const char* text) {
      ImGui::TextUnformatted(text);
    }

    void native_ui_text_colored(float r, float g, float b, float a, const char* text) {
      ImGui::TextColored(ImVec4(r, g, b, a), "%s", text);
    }

    void native_ui_text_wrapped(const char* text) {
      ImGui::TextWrapped("%s", text);
    }

    void native_ui_label_text(const char* label, const char* text) {
      ImGui::LabelText(label, "%s", text);
    }

    nbool32 native_ui_button(const char* label, float width, float height) {
      return ImGui::Button(label, ImVec2(width, height));
    }

    nbool32 native_ui_small_button(const char* label) {
      return ImGui::SmallButton(label);
    }

    nbool32 native_ui_invisible_button(const char* str_id, float width, float height) {
      return ImGui::InvisibleButton(str_id, ImVec2(width, height));
    }

    nbool32 native_ui_checkbox(const char* label, nbool32* value) {
      bool v = *value;
      bool changed = ImGui::Checkbox(label, &v);
      *value = v;
      return changed;
    }

    nbool32 native_ui_radio_button(const char* label, nbool32 active) {
      return ImGui::RadioButton(label, (bool)active);
    }

    void native_ui_progress_bar(float fraction, float width, float height, const char* overlay) {
      std::string overlay_str = overlay;
      const char* overlay_ptr = overlay_str.empty() ? nullptr : overlay_str.c_str();
      ImGui::ProgressBar(fraction, ImVec2(width, height), overlay_ptr);
    }

    void native_ui_separator() {
      ImGui::Separator();
    }

    void native_ui_same_line(float offset, float spacing) {
      ImGui::SameLine(offset, spacing);
    }

    void native_ui_spacing() {
      ImGui::Spacing();
    }

    void native_ui_indent(float width) {
      ImGui::Indent(width);
    }

    void native_ui_unindent(float width) {
      ImGui::Unindent(width);
    }

    void native_ui_new_line() {
      ImGui::NewLine();
    }

    void native_ui_dummy(float width, float height) {
      ImGui::Dummy(ImVec2(width, height));
    }

    nbool32 native_ui_input_text(const char* label, char* buffer, int32_t buffer_size, int32_t flags) {
      return ImGui::InputText(label, buffer, static_cast<size_t>(buffer_size), static_cast<ImGuiInputTextFlags>(flags));
    }

    nbool32 native_ui_input_text_multiline(const char* label, char* buffer, int32_t buffer_size, float width, float height, int32_t flags) {
      return ImGui::InputTextMultiline(label, buffer, static_cast<size_t>(buffer_size), ImVec2(width, height), static_cast<ImGuiInputTextFlags>(flags));
    }

    nbool32 native_ui_input_float(const char* label, float* value, float step, float step_fast, int32_t decimal_precision) {
      std::string format = std::format("%.{}f", decimal_precision);
      return ImGui::InputFloat(label, value, step, step_fast, format.c_str());
    }

    nbool32 native_ui_input_float2(const char* label, float* values) {
      return ImGui::InputFloat2(label, values);
    }

    nbool32 native_ui_input_float3(const char* label, float* values) {
      return ImGui::InputFloat3(label, values);
    }

    nbool32 native_ui_input_float4(const char* label, float* values) {
      return ImGui::InputFloat4(label, values);
    }

    nbool32 native_ui_input_int(const char* label, int32_t* value, int32_t step, int32_t step_fast) {
      return ImGui::InputInt(label, value, step, step_fast);
    }

    nbool32 native_ui_drag_float(const char* label, float* value, float speed, float min_val, float max_val) {
      return ImGui::DragFloat(label, value, speed, min_val, max_val);
    }

    nbool32 native_ui_drag_float3(const char* label, float* values, float speed, float min_val, float max_val) {
      return ImGui::DragFloat3(label, values, speed, min_val, max_val);
    }

    nbool32 native_ui_slider_float(const char* label, float* value, float min_val, float max_val) {
      return ImGui::SliderFloat(label, value, min_val, max_val);
    }

    nbool32 native_ui_slider_int(const char* label, int32_t* value, int32_t min_val, int32_t max_val) {
      return ImGui::SliderInt(label, value, min_val, max_val);
    }

    nbool32 native_ui_color_edit3(const char* label, float* col) {
      return ImGui::ColorEdit3(label, col);
    }

    nbool32 native_ui_color_edit4(const char* label, float* col) {
      return ImGui::ColorEdit4(label, col);
    }

    nbool32 native_ui_tree_node(const char* label) {
      return ImGui::TreeNode(label);
    }

    nbool32 native_ui_tree_node_ex(const char* label, int32_t flags) {
      return ImGui::TreeNodeEx(label, static_cast<ImGuiTreeNodeFlags>(flags));
    }

    void native_ui_tree_pop() {
      ImGui::TreePop();
    }

    nbool32 native_ui_collapsing_header(const char* label, int32_t flags) {
      return ImGui::CollapsingHeader(label, static_cast<ImGuiTreeNodeFlags>(flags));
    }

    nbool32 native_ui_selectable(const char* label, nbool32 selected, int32_t flags) {
      return ImGui::Selectable(label, (bool)selected, static_cast<ImGuiSelectableFlags>(flags));
    }

    nbool32 native_ui_begin_combo(const char* label, const char* preview_value, int32_t flags) {
      return ImGui::BeginCombo(label, preview_value, static_cast<ImGuiComboFlags>(flags));
    }

    void native_ui_end_combo() {
      ImGui::EndCombo();
    }

    nbool32 native_ui_begin_listbox(const char* label, float width, float height) {
      return ImGui::BeginListBox(label, ImVec2(width, height));
    }

    void native_ui_end_listbox() {
      ImGui::EndListBox();
    }

    nbool32 native_ui_begin_tab_bar(const char* str_id, int32_t flags) {
      return ImGui::BeginTabBar(str_id, static_cast<ImGuiTabBarFlags>(flags));
    }

    void native_ui_end_tab_bar() {
      ImGui::EndTabBar();
    }

    nbool32 native_ui_begin_tab_item(const char* label, int32_t flags) {
      return ImGui::BeginTabItem(label, nullptr, static_cast<ImGuiTabItemFlags>(flags));
    }

    void native_ui_end_tab_item() {
      ImGui::EndTabItem();
    }

    nbool32 native_ui_begin_menu_bar() {
      return ImGui::BeginMenuBar();
    }

    void native_ui_end_menu_bar() {
      ImGui::EndMenuBar();
    }

    nbool32 native_ui_begin_main_menu_bar() {
      return ImGui::BeginMainMenuBar();
    }

    void native_ui_end_main_menu_bar() {
      ImGui::EndMainMenuBar();
    }

    nbool32 native_ui_begin_menu(const char* label, nbool32 enabled) {
      return ImGui::BeginMenu(label, (bool)enabled);
    }

    void native_ui_end_menu() {
      ImGui::EndMenu();
    }

    nbool32 native_ui_menu_item(const char* label, const char* shortcut, nbool32 selected, nbool32 enabled) {
      std::string shortcut_str = shortcut;
      const char* shortcut_ptr = shortcut_str.empty() ? nullptr : shortcut_str.c_str();
      return ImGui::MenuItem(label, shortcut_ptr, (bool)selected, (bool)enabled);
    }

    void native_ui_open_popup(const char* str_id) {
      ImGui::OpenPopup(str_id);
    }

    nbool32 native_ui_begin_popup(const char* str_id, int32_t flags) {
      return ImGui::BeginPopup(str_id, static_cast<ImGuiWindowFlags>(flags));
    }

    nbool32 native_ui_begin_popup_modal(const char* name, int32_t flags) {
      return ImGui::BeginPopupModal(name, nullptr, static_cast<ImGuiWindowFlags>(flags));
    }

    void native_ui_end_popup() {
      ImGui::EndPopup();
    }

    void native_ui_close_current_popup() {
      ImGui::CloseCurrentPopup();
    }

    nbool32 native_ui_begin_table(const char* str_id, int32_t columns, int32_t flags) {
      return ImGui::BeginTable(str_id, columns, static_cast<ImGuiTableFlags>(flags));
    }

    void native_ui_end_table() {
      ImGui::EndTable();
    }

    void native_ui_table_next_row(int32_t flags, float min_row_height) {
      ImGui::TableNextRow(static_cast<ImGuiTableRowFlags>(flags), min_row_height);
    }

    nbool32 native_ui_table_next_column() {
      return ImGui::TableNextColumn();
    }

    nbool32 native_ui_table_set_column_index(int32_t column_n) {
      return ImGui::TableSetColumnIndex(column_n);
    }

    void native_ui_table_setup_column(const char* label, int32_t flags, float init_width_or_weight) {
      ImGui::TableSetupColumn(label, static_cast<ImGuiTableColumnFlags>(flags), init_width_or_weight);
    }

    void native_ui_table_headers_row() {
      ImGui::TableHeadersRow();
    }

    void native_ui_get_content_region_avail(float* out_x, float* out_y) {
      ImVec2 avail = ImGui::GetContentRegionAvail();
      *out_x = avail.x;
      *out_y = avail.y;
    }

    void native_ui_get_window_size(float* out_width, float* out_height) {
      ImVec2 size = ImGui::GetWindowSize();
      *out_width = size.x;
      *out_height = size.y;
    }

    void native_ui_get_window_pos(float* out_x, float* out_y) {
      ImVec2 pos = ImGui::GetWindowPos();
      *out_x = pos.x;
      *out_y = pos.y;
    }

    void native_ui_set_next_window_size(float width, float height, int32_t cond) {
      ImGui::SetNextWindowSize(ImVec2(width, height), cond);
    }

    void native_ui_set_next_window_pos(float x, float y, int32_t cond) {
      ImGui::SetNextWindowPos(ImVec2(x, y), cond);
    }

    nbool32 native_ui_is_item_hovered() {
      return ImGui::IsItemHovered();
    }

    nbool32 native_ui_is_item_clicked(int32_t mouse_button) {
      return ImGui::IsItemClicked(mouse_button);
    }

    nbool32 native_ui_is_item_active() {
      return ImGui::IsItemActive();
    }

    nbool32 native_ui_is_window_focused(int32_t flags) {
      return ImGui::IsWindowFocused(static_cast<ImGuiFocusedFlags>(flags));
    }

    nbool32 native_ui_is_window_hovered(int32_t flags) {
      return ImGui::IsWindowHovered(static_cast<ImGuiHoveredFlags>(flags));
    }

    void native_ui_push_style_color(int32_t idx, float r, float g, float b, float a) {
      ImGui::PushStyleColor(idx, ImVec4(r, g, b, a));
    }

    void native_ui_pop_style_color(int32_t count) {
      ImGui::PopStyleColor(count);
    }

    void native_ui_push_style_var_float(int32_t idx, float val) {
      ImGui::PushStyleVar(idx, val);
    }

    void native_ui_push_style_var_vec2(int32_t idx, float x, float y) {
      ImGui::PushStyleVar(idx, ImVec2(x, y));
    }

    void native_ui_pop_style_var(int32_t count) {
      ImGui::PopStyleVar(count);
    }

    void native_ui_push_item_width(float width) {
      ImGui::PushItemWidth(width);
    }

    void native_ui_pop_item_width() {
      ImGui::PopItemWidth();
    }

    void native_ui_push_id_str(const char* str_id) {
      ImGui::PushID(str_id);
    }

    void native_ui_push_id_int(int32_t int_id) {
      ImGui::PushID(int_id);
    }

    void native_ui_pop_id() {
      ImGui::PopID();
    }

    void native_ui_set_tooltip(const char* text) {
      ImGui::SetTooltip("%s", text);
    }

    nbool32 native_ui_begin_tooltip() {
      return ImGui::BeginTooltip();
    }

    void native_ui_end_tooltip() {
      ImGui::EndTooltip();
    }

    nbool32 native_ui_begin_drag_drop_source(int32_t flags) {
      return ImGui::BeginDragDropSource(static_cast<ImGuiDragDropFlags>(flags));
    }

    nbool32 native_ui_set_drag_drop_payload(const char* type, void* data, int32_t size) {
      return ImGui::SetDragDropPayload(type, data, size);
    }

    void native_ui_end_drag_drop_source() {
      ImGui::EndDragDropSource();
    }

    nbool32 native_ui_begin_drag_drop_target() {
      return ImGui::BeginDragDropTarget();
    }

    void native_ui_end_drag_drop_target() {
      ImGui::EndDragDropTarget();
    }

    void native_ui_separator_text(const char* label) {
      ImGui::SeparatorText(label);
    }

    void native_ui_text_disabled(const char* text) {
      ImGui::TextDisabled("%s", text);
    }

    void native_ui_bullet() {
      ImGui::Bullet();
    }

    void native_ui_begin_disabled(nbool32 disabled) {
      ImGui::BeginDisabled(disabled != 0);
    }

    void native_ui_end_disabled() {
      ImGui::EndDisabled();
    }

    void native_ui_set_clipboard_text(const char* text) {
      ImGui::SetClipboardText(text);
    }

    const char* native_ui_get_clipboard_text() {
      const char* text = ImGui::GetClipboardText();
      return returned_string(text != nullptr ? text : "");
    }

    void native_ui_begin_property_row(const char* label, float label_width) {
      ui::inspector::begin_property_row(label, label_width > 0.f ? label_width : ui::inspector::kPropertyLabelWidth);
    }

    void native_ui_end_property_row() {
      ui::inspector::end_property_row();
    }

    void native_ui_property_display(const char* label, const char* text, float r, float g, float b, float a) {
      ui::inspector::property_display(label, text, glm::vec4{ r, g, b, a });
    }

    nbool32 native_ui_theme_color(const char* token, float* out_rgba) {
      const color_token* found = find_color_token(token);
      if (found == nullptr) {
        return false;
      }
      const glm::vec4& color = *found->ptr;
      out_rgba[0] = color.r;
      out_rgba[1] = color.g;
      out_rgba[2] = color.b;
      out_rgba[3] = color.a;
      return true;
    }

    void native_ui_push_font(int32_t role) {
      const bool is_role = role >= 0 && role < static_cast<int32_t>(font_role::NUM_ROLES);
      ImGui::PushFont(is_role ? ui_fonts().get(static_cast<font_role>(role)) : nullptr);
    }

    void native_ui_pop_font() {
      ImGui::PopFont();
    }

    void native_ui_begin_group() {
      ImGui::BeginGroup();
    }

    void native_ui_end_group() {
      ImGui::EndGroup();
    }

    void native_ui_set_next_item_width(float width) {
      ImGui::SetNextItemWidth(width);
    }

    void native_ui_align_text_to_frame_padding() {
      ImGui::AlignTextToFramePadding();
    }

    void native_ui_get_cursor_pos(float* out_x, float* out_y) {
      const ImVec2 pos = ImGui::GetCursorPos();
      *out_x = pos.x;
      *out_y = pos.y;
    }

    void native_ui_set_cursor_pos(float x, float y) {
      ImGui::SetCursorPos(ImVec2(x, y));
    }

    void native_ui_get_cursor_screen_pos(float* out_x, float* out_y) {
      const ImVec2 pos = ImGui::GetCursorScreenPos();
      *out_x = pos.x;
      *out_y = pos.y;
    }

    void native_ui_set_cursor_screen_pos(float x, float y) {
      ImGui::SetCursorScreenPos(ImVec2(x, y));
    }

    float native_ui_get_scroll_x() {
      return ImGui::GetScrollX();
    }

    float native_ui_get_scroll_y() {
      return ImGui::GetScrollY();
    }

    void native_ui_set_scroll_x(float scroll) {
      ImGui::SetScrollX(scroll);
    }

    void native_ui_set_scroll_y(float scroll) {
      ImGui::SetScrollY(scroll);
    }

    float native_ui_get_scroll_max_x() {
      return ImGui::GetScrollMaxX();
    }

    float native_ui_get_scroll_max_y() {
      return ImGui::GetScrollMaxY();
    }

    void native_ui_set_scroll_here_y(float center_ratio) {
      ImGui::SetScrollHereY(center_ratio);
    }

    void native_ui_set_next_item_open(nbool32 open, int32_t cond) {
      ImGui::SetNextItemOpen(open != 0, static_cast<ImGuiCond>(cond));
    }

    void native_ui_set_keyboard_focus_here(int32_t offset) {
      ImGui::SetKeyboardFocusHere(offset);
    }

    nbool32 native_ui_is_item_deactivated_after_edit() {
      return ImGui::IsItemDeactivatedAfterEdit();
    }

    nbool32 native_ui_is_window_appearing() {
      return ImGui::IsWindowAppearing();
    }

    namespace {

      /// ImGui asserts on a button or a key it does not have
      bool is_mouse_button(int32_t button) {
        return button >= 0 && button < ImGuiMouseButton_COUNT;
      }

      bool is_key(int32_t key) {
        return key >= ImGuiKey_NamedKey_BEGIN && key < ImGuiKey_NamedKey_END;
      }

    }  // namespace

    void native_ui_get_mouse_pos(float* out_x, float* out_y) {
      const ImVec2 pos = ImGui::GetMousePos();
      *out_x = pos.x;
      *out_y = pos.y;
    }

    nbool32 native_ui_is_mouse_down(int32_t button) {
      return is_mouse_button(button) && ImGui::IsMouseDown(button);
    }

    nbool32 native_ui_is_mouse_clicked(int32_t button) {
      return is_mouse_button(button) && ImGui::IsMouseClicked(button);
    }

    nbool32 native_ui_is_mouse_double_clicked(int32_t button) {
      return is_mouse_button(button) && ImGui::IsMouseDoubleClicked(button);
    }

    nbool32 native_ui_is_mouse_released(int32_t button) {
      return is_mouse_button(button) && ImGui::IsMouseReleased(button);
    }

    void native_ui_get_mouse_drag_delta(int32_t button, float lock_threshold, float* out_x, float* out_y) {
      const ImVec2 delta = is_mouse_button(button) ? ImGui::GetMouseDragDelta(button, lock_threshold) : ImVec2(0.f, 0.f);
      *out_x = delta.x;
      *out_y = delta.y;
    }

    void native_ui_get_mouse_wheel(float* out_x, float* out_y) {
      const ImGuiIO& io = ImGui::GetIO();
      *out_x = io.MouseWheelH;
      *out_y = io.MouseWheel;
    }

    nbool32 native_ui_is_mouse_hovering_rect(float min_x, float min_y, float max_x, float max_y, nbool32 clip) {
      return ImGui::IsMouseHoveringRect(ImVec2(min_x, min_y), ImVec2(max_x, max_y), clip != 0);
    }

    nbool32 native_ui_is_key_down(int32_t key) {
      return is_key(key) && ImGui::IsKeyDown(static_cast<ImGuiKey>(key));
    }

    nbool32 native_ui_is_key_pressed(int32_t key, nbool32 repeat) {
      return is_key(key) && ImGui::IsKeyPressed(static_cast<ImGuiKey>(key), repeat != 0);
    }

    int32_t native_ui_get_key_mods() {
      return static_cast<int32_t>(ImGui::GetIO().KeyMods);
    }

    void native_ui_get_item_rect_min(float* out_x, float* out_y) {
      const ImVec2 min = ImGui::GetItemRectMin();
      *out_x = min.x;
      *out_y = min.y;
    }

    void native_ui_get_item_rect_max(float* out_x, float* out_y) {
      const ImVec2 max = ImGui::GetItemRectMax();
      *out_x = max.x;
      *out_y = max.y;
    }

    void native_ui_get_item_rect_size(float* out_width, float* out_height) {
      const ImVec2 size = ImGui::GetItemRectSize();
      *out_width = size.x;
      *out_height = size.y;
    }

    nbool32 native_ui_drag_int(const char* label, int32_t* value, float speed, int32_t min_val, int32_t max_val) {
      return ImGui::DragInt(label, value, speed, min_val, max_val);
    }

    nbool32 native_ui_drag_float2(const char* label, float* values, float speed, float min_val, float max_val) {
      return ImGui::DragFloat2(label, values, speed, min_val, max_val);
    }

    nbool32 native_ui_drag_float4(const char* label, float* values, float speed, float min_val, float max_val) {
      return ImGui::DragFloat4(label, values, speed, min_val, max_val);
    }

    nbool32 native_ui_slider_float2(const char* label, float* values, float min_val, float max_val) {
      return ImGui::SliderFloat2(label, values, min_val, max_val);
    }

    nbool32 native_ui_slider_float3(const char* label, float* values, float min_val, float max_val) {
      return ImGui::SliderFloat3(label, values, min_val, max_val);
    }

    nbool32 native_ui_slider_float4(const char* label, float* values, float min_val, float max_val) {
      return ImGui::SliderFloat4(label, values, min_val, max_val);
    }

    const void* native_ui_accept_drag_drop_payload(const char* type, int32_t flags, int32_t* out_size) {
      *out_size = 0;
      const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(type, static_cast<ImGuiDragDropFlags>(flags));
      if (payload == nullptr) {
        return nullptr;
      }
      *out_size = payload->DataSize;
      return payload->Data;
    }

  }  // namespace bindings
}  // namespace other