/**
 * \file editor_settings.hpp
 **/
#ifndef OTHER_EDITOR_EDITOR_SETTINGS_HPP
#define OTHER_EDITOR_EDITOR_SETTINGS_HPP

#include <glm/glm.hpp>

namespace other {

  struct editor_settings {
    float grid_line_thickness = 1.f;
    float grid_granularity = 1.f;

    glm::vec4 selection_outline_color = { 1.f, 0.6f, 0.f, 1.f };
    /// in pixels
    float selection_outline_width = 3.f;
  };

}  // namespace other

#endif  // OTHER_EDITOR_EDITOR_SETTINGS_HPP