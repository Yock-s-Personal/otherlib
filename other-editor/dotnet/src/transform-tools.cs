using System;
using System.Collections.Generic;
using Other.UI;

namespace Other.Editor
{
  /// what the move, rotate and scale tools share: which objects a drag takes with it, how
  ///  they were when it began, and the one undo step a drag is
  public abstract class TransformTool : SceneTool
  {
    protected static readonly Vec4 kRed = new Vec4(0.92f, 0.26f, 0.24f, 1f);
    protected static readonly Vec4 kGreen = new Vec4(0.45f, 0.80f, 0.20f, 1f);
    protected static readonly Vec4 kBlue = new Vec4(0.25f, 0.52f, 0.96f, 1f);
    protected static readonly Vec4 kPlain = new Vec4(0.85f, 0.85f, 0.85f, 1f);
    protected static readonly Vec4 kTaken = new Vec4(1f, 0.85f, 0.2f, 1f);
    protected static readonly Vec4[] kAxisColours = { kRed, kGreen, kBlue };
    /// how long a handle is on the screen, in the UI's units
    protected const float kReach = 90f;

    /// one object a drag takes with it, as it was when the drag began
    protected struct Start
    {
      public SceneObject item;
      public Vec3 position;
      public Quaternion rotation;
      public Vec3 scale;
      public Vec3 world_position;
      public Quaternion world_rotation;
    }

    protected readonly List<Start> starts = new();
    /// where the handles stood, and how they were turned, when the drag began
    protected Vec3 pivot_start;
    protected Quaternion frame_start = Quaternion.Identity;
    private EditScope edit;
    private bool dragging;

    protected bool Dragging => dragging;

    /// where the handles stand and how they are turned: at the active object, along its
    ///  own axes or the world's. while a drag goes on they stay as they were when it began,
    ///  so that an axis does not turn under the pointer that is dragging round it
    protected bool Pivot(bool own_axes, out Vec3 at, out Quaternion frame)
    {
      SceneObject? active = Selection.Active;
      if (active == null || !active.IsAlive)
      {
        at = Vec3.Zero;
        frame = Quaternion.Identity;
        return false;
      }

      if (dragging)
      {
        at = pivot_start;
        frame = frame_start;
        return true;
      }
      at = active.Transform.WorldPosition;
      frame = own_axes ? active.Transform.WorldRotation.Normalized() : Quaternion.Identity;
      return true;
    }

    protected static Vec3[] Axes(Quaternion frame)
    {
      return new[] { frame * new Vec3(1f, 0f, 0f), frame * new Vec3(0f, 1f, 0f), frame * new Vec3(0f, 0f, 1f) };
    }

    protected static Vec4 Colour(HandleState state, Vec4 colour)
    {
      return (state & (HandleState.Hot | HandleState.Active)) != 0 ? kTaken : colour;
    }

    protected static bool Snapping => (UIWidgets.KeyMods & KeyMods.Ctrl) != 0;

    /// what every handle of a tool does when its state is read: begins the undo step when
    ///  a drag begins, and ends it when the drag does. true while the handle's value is to
    ///  be applied: it is being dragged, and its drag did not begin this very frame
    protected bool Follow(HandleState state, string what, Vec3 at, Quaternion frame)
    {
      if ((state & HandleState.Started) != 0)
      {
        Begin(what, at, frame);
        return false;
      }
      if ((state & HandleState.Cancelled) != 0)
      {
        PutBack();
        End(false);
        return false;
      }
      if ((state & HandleState.Ended) != 0)
      {
        End(true);
        return false;
      }
      return dragging && (state & HandleState.Active) != 0;
    }

    private void Begin(string what, Vec3 at, Quaternion frame)
    {
      starts.Clear();
      IReadOnlyList<SceneObject> selected = Selection.Objects;
      for (int i = 0; i < selected.Count; i++)
      {
        SceneObject item = selected[i];
        /// an object whose parent is dragged goes with its parent, and not twice
        if (!item.IsAlive || UnderSelected(item, selected))
        {
          continue;
        }
        Transform transform = item.Transform;
        starts.Add(new Start
        {
          item = item,
          position = transform.Position,
          rotation = transform.Rotation,
          scale = transform.Scale,
          world_position = transform.WorldPosition,
          world_rotation = transform.WorldRotation.Normalized(),
        });
      }

      pivot_start = at;
      frame_start = frame;
      string named = starts.Count == 1 ? starts[0].item.Name : $"{starts.Count} objects";
      edit = Edits.Begin($"{what} {named}");
      dragging = true;
    }

    /// the drag is one undo step, or none when it was cancelled and everything put back
    private void End(bool record)
    {
      if (!dragging)
      {
        return;
      }
      dragging = false;
      if (record)
      {
        edit.Dispose();
      }
      else
      {
        edit.Drop();
      }
      starts.Clear();
    }

    /// everything as it was when the drag began
    private void PutBack()
    {
      for (int i = 0; i < starts.Count; i++)
      {
        Start start = starts[i];
        if (!start.item.IsAlive)
        {
          continue;
        }
        Transform transform = start.item.Transform;
        transform.Position = start.position;
        transform.Rotation = start.rotation;
        transform.Scale = start.scale;
      }
    }

    private static bool UnderSelected(SceneObject item, IReadOnlyList<SceneObject> selected)
    {
      for (SceneObject? above = item.Parent; above != null; above = above.Parent)
      {
        for (int i = 0; i < selected.Count; i++)
        {
          if (ReferenceEquals(selected[i], above))
          {
            return true;
          }
        }
      }
      return false;
    }
  }

  /// three arrows and three squares at the active object: an arrow slides what is selected
  ///  along an axis, a square in the plane of two
  [SceneTool("move", Title = "Move", Order = 10)]
  public class MoveTool : TransformTool
  {
    private static readonly ulong[] kAxisIds = { Handles.Id("move.x"), Handles.Id("move.y"), Handles.Id("move.z") };
    private static readonly ulong[] kPlaneIds = { Handles.Id("move.yz"), Handles.Id("move.zx"), Handles.Id("move.xy") };
    private readonly HandleState[] plane_states = new HandleState[3];
    private readonly HandleState[] axis_states = new HandleState[3];

    public override void OnSceneView()
    {
      if (!Pivot(!SceneTools.WorldAxes, out Vec3 at, out Quaternion frame))
      {
        return;
      }
      float unit = SceneViewport.PixelSize(at);
      if (unit <= 0f)
      {
        return;
      }
      float reach = unit * kReach;
      float snap = Snapping ? Handles.MoveSnap : 0f;
      Vec3[] axes = Axes(frame);

      /// the squares first: where one and an arrow are both under the pointer it is the
      ///  square that is meant. a square is named by the axis it is square to
      for (int i = 0; i < 3; i++)
      {
        Vec3 moved = at;
        plane_states[i] = Handles.SlidePlane(kPlaneIds[i], ref moved, axes[(i + 1) % 3], axes[(i + 2) % 3], reach * 0.25f, reach * 0.5f, snap);
        if (Follow(plane_states[i], "Move", at, frame))
        {
          Apply(moved);
        }
      }
      for (int i = 0; i < 3; i++)
      {
        Vec3 moved = at;
        axis_states[i] = Handles.SlideAxis(kAxisIds[i], ref moved, axes[i], reach, snap);
        if (Follow(axis_states[i], "Move", at, frame))
        {
          Apply(moved);
        }
      }

      /// drawn where the active object is now, which while it is dragged is not where the
      ///  drag began, and at the size that place is on the screen
      Vec3 shown = Selection.Active!.Transform.WorldPosition;
      float size = SceneViewport.PixelSize(shown) * kReach;
      for (int i = 0; i < 3; i++)
      {
        Vec3 u = axes[(i + 1) % 3];
        Vec3 v = axes[(i + 2) % 3];
        Vec4 colour = Colour(plane_states[i], kAxisColours[i]);
        colour.W = (plane_states[i] & (HandleState.Hot | HandleState.Active)) != 0 ? 0.7f : 0.35f;
        Handles.Quad(shown + (u + v) * (size * 0.25f), shown + u * (size * 0.5f) + v * (size * 0.25f), shown + (u + v) * (size * 0.5f), shown + u * (size * 0.25f) + v * (size * 0.5f), colour);
      }
      for (int i = 0; i < 3; i++)
      {
        Handles.Arrow(shown, shown + axes[i] * size, Colour(axis_states[i], kAxisColours[i]), 2.5f);
      }
    }

    /// everything that is dragged goes as far as the handles have
    private void Apply(Vec3 pivot)
    {
      Vec3 delta = pivot - pivot_start;
      for (int i = 0; i < starts.Count; i++)
      {
        if (starts[i].item.IsAlive)
        {
          starts[i].item.Transform.WorldPosition = starts[i].world_position + delta;
        }
      }
    }
  }

  /// a ball at the active object with three rings round it, one for each axis, and a ring
  ///  outside it that faces the view: a ring turns what is selected round its axis, about
  ///  where the ball stands. of the three only the half on the near side of the ball is
  ///  drawn and taken hold of, so that the rings read as round a ball and not as three
  ///  loops through one another. while one is turned it is the only one, drawn whole, with
  ///  how far it has been turned filled in
  [SceneTool("rotate", Title = "Rotate", Order = 11)]
  public class RotateTool : TransformTool
  {
    private static readonly ulong[] kIds = { Handles.Id("rotate.x"), Handles.Id("rotate.y"), Handles.Id("rotate.z"), Handles.Id("rotate.view") };
    private static readonly Vec4 kBallEdge = new Vec4(0.75f, 0.75f, 0.75f, 0.3f);
    private static readonly Vec4 kTurned = new Vec4(1f, 0.85f, 0.2f, 0.22f);
    private static readonly Vec4 kLabel = new Vec4(1f, 1f, 1f, 1f);
    /// the ring that faces the view is this many times the ball across
    private const float kOuter = 1.15f;
    private const int kView = 3;

    private readonly Vec3[] axes = new Vec3[4];
    private readonly HandleState[] states = new HandleState[4];
    private readonly float[] angles = new float[4];
    /// the ring a drag has hold of, and the axis it had when the drag began. -1 for none
    private int turning = -1;
    private Vec3 turning_axis;

    public override void OnSceneView()
    {
      if (!Pivot(!SceneTools.WorldAxes, out Vec3 at, out Quaternion frame))
      {
        return;
      }
      float unit = SceneViewport.PixelSize(at);
      if (unit <= 0f)
      {
        return;
      }
      float radius = unit * kReach * 0.9f;
      float snap = Snapping ? Handles.TurnSnap : 0f;

      Vec3[] own = Axes(frame);
      /// towards the viewer from the ball, so that dragging the outer ring
      ///  counter-clockwise turns things that way
      Vec3 to_viewer = (SceneViewport.CameraPosition - at).Normalized();
      for (int i = 0; i < 4; i++)
      {
        axes[i] = i == turning ? turning_axis : i == kView ? to_viewer : own[i];
      }

      for (int i = 0; i < 4; i++)
      {
        angles[i] = 0f;
        states[i] = Handles.Turn(kIds[i], at, axes[i], i == kView ? radius * kOuter : radius, ref angles[i], snap, i != kView);
        if ((states[i] & HandleState.Started) != 0)
        {
          turning = i;
          turning_axis = axes[i];
        }
        if (Follow(states[i], "Rotate", at, frame))
        {
          Apply(axes[i], angles[i]);
        }
        if ((states[i] & HandleState.Ended) != 0)
        {
          turning = -1;
        }
      }
      if (!Dragging)
      {
        turning = -1;
      }

      if (turning >= 0)
      {
        DrawTurning(at, axes[turning], turning == kView ? radius * kOuter : radius, angles[turning]);
        return;
      }

      Handles.Circle(at, to_viewer, radius, kBallEdge, 1f);
      for (int i = 0; i < 3; i++)
      {
        Vec4 colour = Colour(states[i], kAxisColours[i]);
        float thickness = (states[i] & HandleState.Hot) != 0 ? 3.5f : 2.5f;
        /// the half of the ring on the viewer's side is the half turn that has, in its
        ///  middle, the way to the viewer laid flat in the ring's plane
        Vec3 flat = to_viewer - axes[i] * Vec3.Dot(to_viewer, axes[i]);
        if (flat.Length() < 0.05f)
        {
          /// it faces the viewer: all of it is the near side
          Handles.Circle(at, axes[i], radius, colour, thickness);
          continue;
        }
        Handles.Arc(at, axes[i], Vec3.Cross(flat.Normalized(), axes[i]), MathF.PI, radius, colour, thickness);
      }
      Handles.Circle(at, to_viewer, radius * kOuter, Colour(states[kView], kPlain), (states[kView] & HandleState.Hot) != 0 ? 3f : 2f);
    }

    /// the one ring that is being turned: whole, with the part it has been turned through
    ///  filled in from where it was taken hold of, and how far that is in degrees
    private void DrawTurning(Vec3 at, Vec3 axis, float radius, float angle)
    {
      Vec3 from = Handles.GrabDirection;
      Vec3 to = Quaternion.FromAxisAngle(axis, angle) * from;
      Handles.Pie(at, axis, from, angle, radius, kTurned);
      Handles.Circle(at, axis, radius, kTaken, 2.5f);
      Handles.Line(at, at + from * radius, kTaken, 1.5f);
      Handles.Line(at, at + to * radius, kTaken, 2.5f);
      Handles.Label(at + to * (radius * 1.12f), $"{angle * 180f / MathF.PI:F1}\u00B0", kLabel);
    }

    private void Apply(Vec3 axis, float angle)
    {
      Quaternion turned = Quaternion.FromAxisAngle(axis, angle);
      for (int i = 0; i < starts.Count; i++)
      {
        Start start = starts[i];
        if (!start.item.IsAlive)
        {
          continue;
        }
        /// each is turned where it stands, and carried round where the ball stands
        start.item.Transform.WorldRotation = (turned * start.world_rotation).Normalized();
        start.item.Transform.WorldPosition = pivot_start + turned * (start.world_position - pivot_start);
      }
    }
  }

  /// three handles along the active object's own axes, and one in the middle: one along an
  ///  axis makes what is selected longer or shorter that way, the middle one bigger or
  ///  smaller all ways. sizes are along an object's own axes, whichever axes are chosen
  [SceneTool("scale", Title = "Scale", Order = 12)]
  public class ScaleTool : TransformTool
  {
    private static readonly ulong[] kAxisIds = { Handles.Id("scale.x"), Handles.Id("scale.y"), Handles.Id("scale.z") };
    private static readonly ulong kAllId = Handles.Id("scale.all");
    private const float kMiddle = 9f;

    public override void OnSceneView()
    {
      if (!Pivot(true, out Vec3 at, out Quaternion frame))
      {
        return;
      }
      float unit = SceneViewport.PixelSize(at);
      if (unit <= 0f)
      {
        return;
      }
      float reach = unit * kReach;
      float snap = Snapping ? Handles.ScaleSnap : 0f;
      Vec3[] axes = Axes(frame);

      /// the middle first: it is under the near ends of the other three
      float all = 1f;
      HandleState middle = Handles.StretchAll(kAllId, at, kMiddle, ref all, snap);
      if (Follow(middle, "Scale", at, frame))
      {
        Apply(new Vec3(all, all, all));
      }

      for (int i = 0; i < 3; i++)
      {
        float factor = 1f;
        HandleState state = Handles.StretchAxis(kAxisIds[i], at, axes[i], reach, ref factor, snap);
        if (Follow(state, "Scale", at, frame))
        {
          Apply(new Vec3(i == 0 ? factor : 1f, i == 1 ? factor : 1f, i == 2 ? factor : 1f));
        }
        Vec4 colour = Colour(state, kAxisColours[i]);
        Handles.Line(at, at + axes[i] * reach, colour, 2.5f);
        Handles.Dot(at + axes[i] * reach, 5f, colour);
      }
      Handles.Dot(at, kMiddle, Colour(middle, kPlain));
    }

    private void Apply(Vec3 factor)
    {
      for (int i = 0; i < starts.Count; i++)
      {
        if (starts[i].item.IsAlive)
        {
          starts[i].item.Transform.Scale = starts[i].scale * factor;
        }
      }
    }
  }
}
