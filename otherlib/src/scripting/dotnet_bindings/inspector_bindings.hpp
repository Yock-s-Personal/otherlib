/**
 * \file scripting/dotnet_bindings/inspector_bindings.hpp
 * the inspector's own widgets, for a window written in C# that inspects an object: the
 * header, a component's section and its fields, and a row for each kind of value
 **/
#ifndef OTHERLIB_SCRIPTING_DOTNET_BINDINGS_INSPECTOR_BINDINGS_HPP
#define OTHERLIB_SCRIPTING_DOTNET_BINDINGS_INSPECTOR_BINDINGS_HPP

#include "core/defines.hpp"

#include "dotnet/types.hpp"

namespace other {
  namespace bindings {

    /// the object's header with its name to edit. true when a new name was entered, which
    ///  is then in the buffer
    nbool32 native_ui_object_header(char* name_buffer, int32_t buffer_size, natural_t object_id);

    /// a component's section. true while it is open; the end is called either way.
    ///  out_remove is set when its remove button was clicked
    nbool32 native_ui_begin_component_section(const char* name, float r, float g, float b, float a, nbool32 removable, nbool32* out_remove);
    void native_ui_end_component_section();
    /// the rows of the object's component of that kind, as the engine draws them. true when
    ///  one was changed. nothing, and false, where there is no driver to draw them with
    nbool32 native_ui_component_fields(natural_t object_id, natural_t component_id);
    nbool32 native_ui_add_component_button(const char* label);

    /// a row for a value: the label, and the field that edits it. true when it was changed
    nbool32 native_ui_property_bool(const char* label, nbool32* value);
    nbool32 native_ui_property_float(const char* label, float* value, float speed);
    nbool32 native_ui_property_int32(const char* label, int32_t* value);
    nbool32 native_ui_property_uint32(const char* label, uint32_t* value);
    nbool32 native_ui_property_int64(const char* label, int64_t* value);
    nbool32 native_ui_property_uint64(const char* label, uint64_t* value);
    nbool32 native_ui_property_vec2(const char* label, float* xy, float speed);
    nbool32 native_ui_property_vec3(const char* label, float* xyz, float speed);
    nbool32 native_ui_property_vec4(const char* label, float* xyzw, float speed);
    nbool32 native_ui_property_text(const char* label, char* buffer, int32_t buffer_size);

    /// after an item: the path of the asset the asset browser's drag dropped on it this
    ///  frame. false when none was, or its path does not fit
    nbool32 native_ui_asset_drop_target(char* out_path, int32_t capacity);

  }  // namespace bindings
}  // namespace other

#endif  // OTHERLIB_SCRIPTING_DOTNET_BINDINGS_INSPECTOR_BINDINGS_HPP
