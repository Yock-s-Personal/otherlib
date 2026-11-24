/**
 * \file services/editor_selection.hpp
 **/
#ifndef OTHER_EDITOR_SERVICES_EDITOR_SELECTION_HPP
#define OTHER_EDITOR_SERVICES_EDITOR_SELECTION_HPP

#include <glm/glm.hpp>

#include "data-structures/std_container.hpp"

#include "renderer/draw_command.hpp"

namespace other {

  class scene;

  struct selection {
    scene* scene_ptr = nullptr;

    bool multiple_selection_enabled = false;
    ostd::vector<natural_t> objects = {};
  };

}  // namespace other

#endif  // OTHER_EDITOR_SERVICES_EDITOR_SELECTION_HPP