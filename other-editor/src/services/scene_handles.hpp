/**
 * \file services/scene_handles.hpp
 * handles: things in the Scene View that the pointer takes hold of and drags, which give
 * back a place, an angle or a size. a tool asks for each of its handles every frame, as a
 * window asks for its widgets; what is drawn for one is the tool's affair. nothing here
 * asks a window for anything: it is told what the pointer did, so it is tested without one
 **/
#ifndef OTHER_EDITOR_SERVICES_SCENE_HANDLES_HPP
#define OTHER_EDITOR_SERVICES_SCENE_HANDLES_HPP

#include <cstdint>

#include <glm/glm.hpp>

#include "core/defines.hpp"

namespace other {

  class camera;

  /// one frame of the view and the pointer in it
  struct handle_input {
    const camera* cam = nullptr;
    glm::vec2 view_size = { 0.f, 0.f };
    /// from the top left of the view's picture
    glm::vec2 pointer = { 0.f, 0.f };
    /// the pointer is over the picture with nothing in front of it
    bool over_view = false;
    /// the left button is held, and it went down this frame over the picture
    bool down = false;
    bool pressed = false;
    /// alt is held: a press is the camera's and no handle's
    bool blocked = false;
    /// escape was pressed: what is being dragged goes back to where it was
    bool cancel = false;
  };

  /// what a handle says of itself when asked for, any of these together
  enum handle_flags : uint32_t {
    HANDLE_NONE = 0,
    /// the pointer is nearer to it than to any other handle, and near enough to take it
    HANDLE_HOT = 1u << 0,
    /// it is being dragged
    HANDLE_ACTIVE = 1u << 1,
    /// the drag began this frame
    HANDLE_STARTED = 1u << 2,
    /// what it gives back is not what it gave back the last frame
    HANDLE_CHANGED = 1u << 3,
    /// the drag ended this frame: the button was let go, or it was cancelled
    HANDLE_ENDED = 1u << 4,
    /// escape ended it, and what it gives back is as it was when the drag began
    HANDLE_CANCELLED = 1u << 5,
  };

  class scene_handles {
   public:
    /// how near the pointer must be to a handle to take it, in the picture's units
    constexpr static float kReach = 8.f;

    /// before the tools are asked for their handles, and after
    void begin_frame(const handle_input& in);
    void end_frame();

    /// a handle along an axis, from the position for the reach. dragged, the position
    ///  goes along the axis and nowhere else, in steps of snap when there is one
    uint32_t slide_axis(natural_t id, glm::vec3& position, const glm::vec3& axis, float reach, float snap = 0.f);
    /// a square handle in the plane of u and v, its corners from near_edge to far_edge
    ///  along each from the position. dragged, the position goes in that plane
    uint32_t slide_plane(natural_t id, glm::vec3& position, const glm::vec3& u, const glm::vec3& v, float near_edge, float far_edge, float snap = 0.f);
    /// a ring round an axis. while it is dragged the angle is how far round it has been
    ///  taken since the drag began, in radians, counter-clockwise seen from where the axis
    ///  points; it is left alone otherwise. near_half_only is for a ring drawn as if round
    ///  a ball, of which only the half on the camera's side is seen: the pointer takes
    ///  hold of that half and not of the half behind
    uint32_t turn(natural_t id, const glm::vec3& centre, const glm::vec3& axis, float radius, float& angle, float snap = 0.f, bool near_half_only = false);
    /// from the centre of the ring that is being turned towards where it was taken hold
    ///  of, of length one: where a tool draws how far it has been turned from
    const glm::vec3& grab_direction() const { return start_hit; }
    /// a handle along an axis as slide_axis has. while it is dragged the factor is how
    ///  many times as far from the centre the pointer is as where it took hold
    uint32_t stretch_axis(natural_t id, const glm::vec3& centre, const glm::vec3& axis, float reach, float& factor, float snap = 0.f);
    /// a round handle at the centre, of a size on the screen. while it is dragged the
    ///  factor grows as the pointer goes right or up and shrinks the other way
    uint32_t stretch_all(natural_t id, const glm::vec3& centre, float radius_pixels, float& factor, float snap = 0.f);

    bool any_hot() const { return hot != 0; }
    bool any_active() const { return active != 0; }
    /// the press of the left button that is held, or was last let go, took a handle: its
    ///  release is the end of a drag and no click on the view
    bool press_taken() const { return taken; }
    /// the tools are being asked for their handles now: between begin_frame and end_frame
    bool in_frame() const { return asking; }
    const handle_input& input() const { return in; }

   private:
    handle_input in;
    bool asking = false;

    natural_t hot = 0;
    natural_t active = 0;
    bool active_asked_for = false;
    bool taken = false;
    /// the nearest handle the pointer was in reach of this frame: the hot one of the next
    natural_t nearest = 0;
    float nearest_distance = 0.f;

    /// what is being dragged, as it was when the drag began
    glm::vec3 start_position = { 0.f, 0.f, 0.f };
    glm::vec3 start_hit = { 0.f, 0.f, 0.f };
    glm::vec2 start_pointer = { 0.f, 0.f };
    float start_along = 0.f;
    float last_angle = 0.f;
    float turned = 0.f;
    float given = 0.f;

    /// the part every handle has: whether it is hot, is taken hold of, or is let go.
    ///  distance is how near the pointer is, none when the handle cannot be told where it is
    uint32_t track(natural_t id, opt<float> distance);
  };

}  // namespace other

#endif  // OTHER_EDITOR_SERVICES_SCENE_HANDLES_HPP
