/**
 * \file scripting/dotnet_bindings/ui_bindings.cpp
 **/
#include "scripting/dotnet_bindings/ui_bindings.hpp"

#include <string>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include "dotnet/native_string.hpp"

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

  }  // namespace bindings
}  // namespace other