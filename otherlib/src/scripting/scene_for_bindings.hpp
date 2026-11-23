/**
 * \file scripting/scene_for_bindings.hpp
 * the scene the script bindings act on is the driver's active one. where no driver runs
 * one, which is the test suite, a scene can be put in its place
 **/
#ifndef OTHERLIB_SCRIPTING_SCENE_FOR_BINDINGS_HPP
#define OTHERLIB_SCRIPTING_SCENE_FOR_BINDINGS_HPP

namespace other {

  class scene;

  namespace detail {

    /// null gives the driver's scene back
    void set_scene_for_bindings(scene* s);
    /// what was set; null while the driver's scene is the one in use
    scene* get_scene_for_bindings();

  }  // namespace detail
}  // namespace other

#endif  // OTHERLIB_SCRIPTING_SCENE_FOR_BINDINGS_HPP
