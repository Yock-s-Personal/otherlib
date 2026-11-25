/**
 * \file services/editor_camera.hpp
 * what moves the camera of the editor's own view of the scene. it is told what the pointer
 * and the keys did in a frame and how long the frame was, and asks nothing of a window, so
 * that it is tested without one
 **/
#ifndef OTHER_EDITOR_SERVICES_EDITOR_CAMERA_HPP
#define OTHER_EDITOR_SERVICES_EDITOR_CAMERA_HPP

#include <cstdint>

#include <glm/glm.hpp>

namespace other {

  class camera;
  struct bounding_box;

  /// one frame of what the pointer and the keys did
  struct editor_camera_input {
    /// the pointer is over the view's picture. a drag begins only there, and goes on
    ///  wherever the pointer is taken until its button is let go
    bool over_view = false;
    /// the right button: look round, and fly with the keys
    bool look_held = false;
    /// the middle button: slide the view
    bool pan_held = false;
    /// alt and the left button: turn round the focus point
    bool orbit_held = false;
    /// how far the pointer went, in pixels, right and down
    glm::vec2 pointer_delta = { 0.f, 0.f };
    /// a turn asked for in degrees, right and down, as a gamepad's stick gives
    glm::vec2 turn_degrees = { 0.f, 0.f };
    /// notches of the wheel, away from the hand is more than 0
    float wheel = 0.f;
    /// right, up, forward, each from -1 to 1
    glm::vec3 move = { 0.f, 0.f, 0.f };
    bool fast = false;
    /// how long the frame is, in seconds
    float seconds = 0.f;
    /// how tall the view's picture is on the screen, in pixels
    float view_height = 1.f;
  };

  struct editor_camera_settings {
    /// metres a second while flying
    float move_speed = 8.f;
    /// degrees for each pixel the pointer goes
    float look_sensitivity = 0.25f;
    /// how many times faster flying is while fast is held
    float fast_factor = 4.f;
  };

  class editor_camera_controller {
   public:
    enum class drag : uint8_t {
      NONE,
      LOOK,
      PAN,
      ORBIT,
    };

    void update(camera& cam, const editor_camera_input& in);

    /// puts the box in the middle of the view and all of it in the view, looking the way
    ///  the camera already looks. the box's middle is the focus point from then on
    void focus(camera& cam, const bounding_box& box);

    /// what orbiting turns round and zooming goes towards
    glm::vec3 focus_point(const camera& cam) const;

    /// turns the camera round the focus point by so many degrees, to the right and down,
    ///  as a drag with alt and the left button does. it stops short of straight up or down
    void orbit(camera& cam, const glm::vec2& degrees);
    /// turns the camera round the focus point until it looks the given way, as far as it
    ///  can: straight up and straight down it stops a degree short of
    void look_along(camera& cam, const glm::vec3& direction);

    drag current_drag() const { return dragging; }

    editor_camera_settings settings;
    /// how far in front of the camera the focus point is
    float focus_distance = 5.f;
    /// the wheel's say in how fast flying is, while looking
    float fly_speed_scale = 1.f;

   private:
    drag dragging = drag::NONE;
  };

}  // namespace other

#endif  // OTHER_EDITOR_SERVICES_EDITOR_CAMERA_HPP
