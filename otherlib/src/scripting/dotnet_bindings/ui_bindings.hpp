/**
 * \file scripting/dotnet_bindings/ui_bindings.hpp
 **/
#ifndef OTHERLIB_SCRIPTING_DOTNET_BINDINGS_UI_BINDINGS_HPP
#define OTHERLIB_SCRIPTING_DOTNET_BINDINGS_UI_BINDINGS_HPP

#include <imgui/imgui.h>

#include "dotnet/native_string.hpp"
#include "dotnet/types.hpp"

namespace other {
  namespace bindings {

    nbool32 native_begin_window(const char* title, int32_t flags);
    void native_end_window();
    nbool32 native_begin_child(const char* str_id, float width, float height, int32_t child_flags, int32_t window_flags);
    void native_end_child();

    void native_ui_text(const char* text);
    void native_ui_text_colored(float r, float g, float b, float a, const char* text);
    void native_ui_text_wrapped(const char* text);
    void native_ui_label_text(const char* label, const char* text);

    nbool32 native_ui_button(const char* label, float width, float height);
    nbool32 native_ui_small_button(const char* label);
    nbool32 native_ui_invisible_button(const char* str_id, float width, float height);
    nbool32 native_ui_checkbox(const char* label, nbool32* value);
    nbool32 native_ui_radio_button(const char* label, nbool32 active);

    void native_ui_progress_bar(float fraction, float width, float height, const char* overlay);
    void native_ui_separator();
    void native_ui_same_line(float offset, float spacing);
    void native_ui_spacing();
    void native_ui_indent(float width);
    void native_ui_unindent(float width);
    void native_ui_new_line();
    void native_ui_dummy(float width, float height);

    nbool32 native_ui_input_text(const char* label, char* buffer, int32_t buffer_size, int32_t flags);
    nbool32 native_ui_input_text_with_hint(const char* label, const char* hint, char* buffer, int32_t buffer_size, int32_t flags);
    nbool32 native_ui_input_text_multiline(const char* label, char* buffer, int32_t buffer_size, float width, float height, int32_t flags);

    nbool32 native_ui_input_float(const char* label, float* value, float step, float step_fast, int32_t decimal_precision);
    nbool32 native_ui_input_float2(const char* label, float* values);
    nbool32 native_ui_input_float3(const char* label, float* values);
    nbool32 native_ui_input_float4(const char* label, float* values);

    nbool32 native_ui_input_int(const char* label, int32_t* value, int32_t step, int32_t step_fast);

    nbool32 native_ui_drag_float(const char* label, float* value, float speed, float min_val, float max_val);
    nbool32 native_ui_drag_float3(const char* label, float* values, float speed, float min_val, float max_val);

    nbool32 native_ui_slider_float(const char* label, float* value, float min_val, float max_val);
    nbool32 native_ui_slider_int(const char* label, int32_t* value, int32_t min_val, int32_t max_val);

    nbool32 native_ui_color_edit3(const char* label, float* col);
    nbool32 native_ui_color_edit4(const char* label, float* col);

    nbool32 native_ui_tree_node(const char* label);
    nbool32 native_ui_tree_node_ex(const char* label, int32_t flags);
    void native_ui_tree_pop();

    nbool32 native_ui_collapsing_header(const char* label, int32_t flags);

    nbool32 native_ui_selectable(const char* label, nbool32 selected, int32_t flags);

    nbool32 native_ui_begin_combo(const char* label, const char* preview_value, int32_t flags);
    void native_ui_end_combo();

    nbool32 native_ui_begin_listbox(const char* label, float width, float height);
    void native_ui_end_listbox();

    nbool32 native_ui_begin_tab_bar(const char* str_id, int32_t flags);
    void native_ui_end_tab_bar();

    nbool32 native_ui_begin_tab_item(const char* label, int32_t flags);
    void native_ui_end_tab_item();

    nbool32 native_ui_begin_menu_bar();
    void native_ui_end_menu_bar();

    nbool32 native_ui_begin_main_menu_bar();
    void native_ui_end_main_menu_bar();

    nbool32 native_ui_begin_menu(const char* label, nbool32 enabled);
    void native_ui_end_menu();

    nbool32 native_ui_menu_item(const char* label, const char* shortcut, nbool32 selected, nbool32 enabled);
    void native_ui_open_popup(const char* str_id);

    nbool32 native_ui_begin_popup(const char* str_id, int32_t flags);
    /// a popup the last item, or the window, opens when it is clicked with the button the
    ///  flags name (ImGuiPopupFlags; 1 is the right button). an empty id is the item's own
    nbool32 native_ui_begin_popup_context_item(const char* str_id, int32_t flags);
    nbool32 native_ui_begin_popup_context_window(const char* str_id, int32_t flags);
    nbool32 native_ui_begin_popup_modal(const char* name, int32_t flags);
    void native_ui_end_popup();
    void native_ui_close_current_popup();

    nbool32 native_ui_begin_table(const char* str_id, int32_t columns, int32_t flags);
    void native_ui_end_table();
    void native_ui_table_next_row(int32_t flags, float min_row_height);
    nbool32 native_ui_table_next_column();
    nbool32 native_ui_table_set_column_index(int32_t column_n);
    void native_ui_table_setup_column(const char* label, int32_t flags, float init_width_or_weight);
    void native_ui_table_headers_row();

    void native_ui_get_content_region_avail(float* out_x, float* out_y);
    void native_ui_get_window_size(float* out_width, float* out_height);
    void native_ui_get_window_pos(float* out_x, float* out_y);
    void native_ui_set_next_window_size(float width, float height, int32_t cond);
    void native_ui_set_next_window_pos(float x, float y, int32_t cond);

    nbool32 native_ui_is_item_hovered();
    nbool32 native_ui_is_item_clicked(int32_t mouse_button);
    nbool32 native_ui_is_item_active();
    /// the last item is a tree node whose arrow was just clicked: it opened or closed
    nbool32 native_ui_is_item_toggled_open();
    /// a text field is being typed in: a key is its, and not a shortcut's
    nbool32 native_ui_want_text_input();

    nbool32 native_ui_is_window_focused(int32_t flags);
    nbool32 native_ui_is_window_hovered(int32_t flags);

    void native_ui_push_style_color(int32_t idx, float r, float g, float b, float a);
    void native_ui_pop_style_color(int32_t count);

    void native_ui_push_style_var_float(int32_t idx, float val);
    void native_ui_push_style_var_vec2(int32_t idx, float x, float y);
    void native_ui_pop_style_var(int32_t count);

    void native_ui_push_item_width(float width);
    void native_ui_pop_item_width();

    void native_ui_push_id_str(const char* str_id);
    void native_ui_push_id_int(int32_t int_id);
    void native_ui_pop_id();

    void native_ui_set_tooltip(const char* text);
    nbool32 native_ui_begin_tooltip();
    void native_ui_end_tooltip();

    nbool32 native_ui_begin_drag_drop_source(int32_t flags);
    nbool32 native_ui_set_drag_drop_payload(const char* type, void* data, int32_t size);
    void native_ui_end_drag_drop_source();
    nbool32 native_ui_begin_drag_drop_target();
    void native_ui_end_drag_drop_target();

    void native_ui_separator_text(const char* label);
    void native_ui_text_disabled(const char* text);
    void native_ui_bullet();
    void native_ui_begin_disabled(nbool32 disabled);
    void native_ui_end_disabled();
    void native_ui_set_clipboard_text(const char* text);
    const char* native_ui_get_clipboard_text();

    /// the rows the native inspector is made of. a label width of 0 or less is the default
    void native_ui_begin_property_row(const char* label, float label_width);
    void native_ui_end_property_row();
    /// an alpha of 0 is the text colour of the theme
    void native_ui_property_display(const char* label, const char* text, float r, float g, float b, float a);

    /// the colour a token has now, as four floats. false when no token has the name
    nbool32 native_ui_theme_color(const char* token, float* out_rgba);
    /// role is a font_role. one the theme does not give, or a number that is no role, is
    ///  the default font
    void native_ui_push_font(int32_t role);
    void native_ui_pop_font();

    void native_ui_begin_group();
    void native_ui_end_group();
    void native_ui_set_next_item_width(float width);
    void native_ui_align_text_to_frame_padding();
    /// where the next item goes, in the window and on the screen
    void native_ui_get_cursor_pos(float* out_x, float* out_y);
    void native_ui_set_cursor_pos(float x, float y);
    void native_ui_get_cursor_screen_pos(float* out_x, float* out_y);
    void native_ui_set_cursor_screen_pos(float x, float y);
    float native_ui_get_scroll_x();
    float native_ui_get_scroll_y();
    void native_ui_set_scroll_x(float scroll);
    void native_ui_set_scroll_y(float scroll);
    float native_ui_get_scroll_max_x();
    float native_ui_get_scroll_max_y();
    void native_ui_set_scroll_here_y(float center_ratio);

    void native_ui_set_next_item_open(nbool32 open, int32_t cond);
    void native_ui_set_keyboard_focus_here(int32_t offset);
    /// the last item was being edited and is not any more, and its value changed
    nbool32 native_ui_is_item_deactivated_after_edit();
    nbool32 native_ui_is_window_appearing();

    /// the mouse as the UI has it. a button ImGui does not have reads as not down
    void native_ui_get_mouse_pos(float* out_x, float* out_y);
    nbool32 native_ui_is_mouse_down(int32_t button);
    nbool32 native_ui_is_mouse_clicked(int32_t button);
    nbool32 native_ui_is_mouse_double_clicked(int32_t button);
    nbool32 native_ui_is_mouse_released(int32_t button);
    void native_ui_get_mouse_drag_delta(int32_t button, float lock_threshold, float* out_x, float* out_y);
    void native_ui_get_mouse_wheel(float* out_x, float* out_y);
    nbool32 native_ui_is_mouse_hovering_rect(float min_x, float min_y, float max_x, float max_y, nbool32 clip);

    /// the keyboard. key is an ImGuiKey; one ImGui does not have reads as not down
    nbool32 native_ui_is_key_down(int32_t key);
    nbool32 native_ui_is_key_pressed(int32_t key, nbool32 repeat);
    /// a key with its modifiers, pressed where it is this caller's to take. global: unless
    ///  something that has the keyboard, a text field say, wants the keys itself. otherwise
    ///  only while the window being drawn has the focus
    nbool32 native_ui_shortcut(int32_t key, int32_t mods, nbool32 global);
    /// ImGuiMod bits
    int32_t native_ui_get_key_mods();

    /// the last item's rectangle on the screen
    void native_ui_get_item_rect_min(float* out_x, float* out_y);
    void native_ui_get_item_rect_max(float* out_x, float* out_y);
    void native_ui_get_item_rect_size(float* out_width, float* out_height);

    nbool32 native_ui_drag_int(const char* label, int32_t* value, float speed, int32_t min_val, int32_t max_val);
    nbool32 native_ui_drag_float2(const char* label, float* values, float speed, float min_val, float max_val);
    nbool32 native_ui_drag_float4(const char* label, float* values, float speed, float min_val, float max_val);
    nbool32 native_ui_slider_float2(const char* label, float* values, float min_val, float max_val);
    nbool32 native_ui_slider_float3(const char* label, float* values, float min_val, float max_val);
    nbool32 native_ui_slider_float4(const char* label, float* values, float min_val, float max_val);

    /// what was dropped on the target, when it is of that type: its bytes are ImGui's and
    ///  good until the frame ends. null when nothing of the type was dropped
    const void* native_ui_accept_drag_drop_payload(const char* type, int32_t flags, int32_t* out_size);

  }  // namespace bindings
}  // namespace other

#endif  // OTHERLIB_SCRIPTING_DOTNET_BINDINGS_UI_BINDINGS_HPP