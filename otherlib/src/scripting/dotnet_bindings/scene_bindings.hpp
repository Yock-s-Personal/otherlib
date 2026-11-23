/**
 * \file scripting/dotnet_bindings/scene_bindings.hpp
 **/
#ifndef OTHERLIB_SCRIPTING_DOTNET_BINDINGS_SCENE_BINDINGS_HPP
#define OTHERLIB_SCRIPTING_DOTNET_BINDINGS_SCENE_BINDINGS_HPP

#include "core/defines.hpp"

#include "dotnet/native_string.hpp"
#include "dotnet/types.hpp"

#include "scripting/scene_for_bindings.hpp"

namespace other {

  class scene;

  namespace detail {

    scene* get_active_scene_checked();
    /// the scene the bindings act on; null when there is none
    scene* get_active_scene();

  }  // namespace detail
  namespace bindings {

    natural_t native_scene_create_object(const char* name, float x, float y, float z);
    void native_scene_destroy_object(natural_t id);
    nbool32 native_scene_has_object(natural_t id);
    const char* native_scene_get_object_name(natural_t id);
    void native_scene_set_object_name(natural_t id, const char* name);
    void native_scene_get_object_ids(natural_t* out_ids, int32_t* out_count, int32_t max_count);
    natural_t native_scene_get_object_count();
    natural_t native_scene_find_object_by_name(const char* name);

    /// the runtime's handle to the object's own script object; null when there is no such
    ///  object, and for the root, which is not an object a script holds
    void* native_scene_object_instance(natural_t id);
    /// parent 0 is the scene itself. keep_world leaves the object where it is in the
    ///  world; without it, where it was under its old parent is where it is under the new.
    ///  false when the parent is the object or one under it, or either is no object
    nbool32 native_scene_set_parent(natural_t id, natural_t parent_id, nbool32 keep_world);
    /// 0 when no object has the tag
    natural_t native_scene_find_object_with_tag(const char* tag);
    void native_scene_find_objects_with_tag(const char* tag, natural_t* out_ids, int32_t* out_count, int32_t max_count);

    natural_t native_scene_get_parent_id(natural_t id);
    void native_scene_get_children_ids(natural_t id, natural_t* out_ids, int32_t* out_count, int32_t max_count);

    nbool32 native_scene_object_has_tag(natural_t id, const char* tag);
    void native_scene_add_object_tag(natural_t id, const char* tag);
    void native_scene_remove_object_tag(natural_t id, const char* tag);

    nbool32 native_scene_get_object_visible(natural_t id);
    void native_scene_set_object_visible(natural_t id, nbool32 visible);

  }  // namespace bindings
}  // namespace other

#endif  // OTHERLIB_SCRIPTING_DOTNET_BINDINGS_SCENE_BINDINGS_HPP