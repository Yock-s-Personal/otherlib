using System;
using OtherCsBindings;
using Other.UI;

namespace Other.Editor
{
  /// what a handle says of itself when it is asked for, any of these together
  [Flags]
  public enum HandleState : uint
  {
    None = 0,
    /// the pointer is nearer to it than to any other handle, and near enough to take it
    Hot = 1u << 0,
    /// it is being dragged
    Active = 1u << 1,
    /// the drag began this frame
    Started = 1u << 2,
    /// what it gives back is not what it gave back the last frame
    Changed = 1u << 3,
    /// the drag ended this frame: the button was let go, or it was cancelled
    Ended = 1u << 4,
    /// Escape ended it, and what it gives back is as it was when the drag began
    Cancelled = 1u << 5,
  }

  /// handles: things in the Scene View that the pointer takes hold of and drags, and the
  ///  shapes a tool draws for them. they are for a SceneTool's OnSceneView and do nothing
  ///  anywhere else. a handle is asked for every frame, by an id of its own, as a widget
  ///  is: it answers whether the pointer is on it or dragging it, and what it has been
  ///  dragged to. drawing it is the tool's to do, with the shapes here, which are given in
  ///  the world and drawn over the view's picture in front of everything in it
  public static unsafe class Handles
  {
    [NativeFunction("EditorHandlesState")]
    internal static delegate* unmanaged[Cdecl]<uint> NativeState;
    [NativeFunction("EditorHandlesSnaps")]
    internal static delegate* unmanaged[Cdecl]<float*, float*, float*, void> NativeSnaps;
    [NativeFunction("EditorHandlesSlideAxis")]
    internal static delegate* unmanaged[Cdecl]<ulong, float*, float*, float, float, uint> NativeSlideAxis;
    [NativeFunction("EditorHandlesSlidePlane")]
    internal static delegate* unmanaged[Cdecl]<ulong, float*, float*, float*, float, float, float, uint> NativeSlidePlane;
    [NativeFunction("EditorHandlesTurn")]
    internal static delegate* unmanaged[Cdecl]<ulong, float*, float*, float, float*, float, uint, uint> NativeTurn;
    [NativeFunction("EditorHandlesGrabDirection")]
    internal static delegate* unmanaged[Cdecl]<float*, NativeBool32> NativeGrabDirection;
    [NativeFunction("EditorHandlesStretchAxis")]
    internal static delegate* unmanaged[Cdecl]<ulong, float*, float*, float, float*, float, uint> NativeStretchAxis;
    [NativeFunction("EditorHandlesStretchAll")]
    internal static delegate* unmanaged[Cdecl]<ulong, float*, float, float*, float, uint> NativeStretchAll;
    [NativeFunction("EditorHandlesLine")]
    internal static delegate* unmanaged[Cdecl]<float*, float*, uint, float, void> NativeLine;
    [NativeFunction("EditorHandlesArrow")]
    internal static delegate* unmanaged[Cdecl]<float*, float*, uint, float, void> NativeArrow;
    [NativeFunction("EditorHandlesCircle")]
    internal static delegate* unmanaged[Cdecl]<float*, float*, float, uint, float, void> NativeCircle;
    [NativeFunction("EditorHandlesArc")]
    internal static delegate* unmanaged[Cdecl]<float*, float*, float*, float, float, uint, float, void> NativeArc;
    [NativeFunction("EditorHandlesPie")]
    internal static delegate* unmanaged[Cdecl]<float*, float*, float*, float, float, uint, void> NativePie;
    [NativeFunction("EditorHandlesQuad")]
    internal static delegate* unmanaged[Cdecl]<float*, float*, float*, float*, uint, void> NativeQuad;
    [NativeFunction("EditorHandlesDot")]
    internal static delegate* unmanaged[Cdecl]<float*, float, uint, void> NativeDot;
    [NativeFunction("EditorHandlesLabel")]
    internal static delegate* unmanaged[Cdecl]<float*, byte*, uint, void> NativeLabel;

    /// an id for a handle from a name of its own: "move.x"
    public static ulong Id(string name)
    {
      return Fnv.Hash(name);
    }

    /// the pointer is on some handle
    public static bool AnyHot => (NativeState() & 1u) != 0;
    /// some handle is being dragged
    public static bool AnyActive => (NativeState() & 2u) != 0;

    /// what a drag goes in steps of when it snaps, as the editor is set up: metres
    public static float MoveSnap
    {
      get
      {
        float move, degrees, scale;
        NativeSnaps(&move, &degrees, &scale);
        return move;
      }
    }

    /// in radians
    public static float TurnSnap
    {
      get
      {
        float move, degrees, scale;
        NativeSnaps(&move, &degrees, &scale);
        return degrees * MathF.PI / 180f;
      }
    }

    /// in times the size
    public static float ScaleSnap
    {
      get
      {
        float move, degrees, scale;
        NativeSnaps(&move, &degrees, &scale);
        return scale;
      }
    }

    /// a handle along an axis, from the position for the reach. dragged, the position
    ///  goes along the axis and nowhere else, in steps of snap when there is one. the
    ///  axis is of length one
    public static HandleState SlideAxis(ulong id, ref Vec3 position, Vec3 axis, float reach, float snap = 0f)
    {
      Vec3 moved = position;
      HandleState state = (HandleState)NativeSlideAxis(id, (float*)&moved, (float*)&axis, reach, snap);
      position = moved;
      return state;
    }

    /// a square handle in the plane of u and v, its corners from near to far along each
    ///  from the position. dragged, the position goes in that plane. u and v are of
    ///  length one and square to each other
    public static HandleState SlidePlane(ulong id, ref Vec3 position, Vec3 u, Vec3 v, float near, float far, float snap = 0f)
    {
      Vec3 moved = position;
      HandleState state = (HandleState)NativeSlidePlane(id, (float*)&moved, (float*)&u, (float*)&v, near, far, snap);
      position = moved;
      return state;
    }

    /// a ring round an axis. while it is dragged the angle is how far round it has been
    ///  taken since the drag began, in radians, counter-clockwise seen from where the axis
    ///  points; it is left alone otherwise. near_half_only is for a ring drawn as if round
    ///  a ball, of which only the half on the camera's side is seen: the pointer takes
    ///  hold of that half and not of the half behind
    public static HandleState Turn(ulong id, Vec3 centre, Vec3 axis, float radius, ref float angle, float snap = 0f, bool near_half_only = false)
    {
      float turned = angle;
      HandleState state = (HandleState)NativeTurn(id, (float*)&centre, (float*)&axis, radius, &turned, snap, near_half_only ? 1u : 0u);
      angle = turned;
      return state;
    }

    /// from the centre of the ring that is being turned towards where it was taken hold
    ///  of, of length one: where a tool draws how far it has been turned from. zero when
    ///  nothing is being dragged
    public static Vec3 GrabDirection
    {
      get
      {
        Vec3 direction = new Vec3(0f, 0f, 0f);
        NativeGrabDirection((float*)&direction);
        return direction;
      }
    }

    /// a handle along an axis as SlideAxis has. while it is dragged the factor is how many
    ///  times as far from the centre the pointer is as where it took hold
    public static HandleState StretchAxis(ulong id, Vec3 centre, Vec3 axis, float reach, ref float factor, float snap = 0f)
    {
      float stretched = factor;
      HandleState state = (HandleState)NativeStretchAxis(id, (float*)&centre, (float*)&axis, reach, &stretched, snap);
      factor = stretched;
      return state;
    }

    /// a round handle at the centre, so many units of the screen across. while it is
    ///  dragged the factor grows as the pointer goes right or up and shrinks the other way
    public static HandleState StretchAll(ulong id, Vec3 centre, float radius, ref float factor, float snap = 0f)
    {
      float stretched = factor;
      HandleState state = (HandleState)NativeStretchAll(id, (float*)&centre, radius, &stretched, snap);
      factor = stretched;
      return state;
    }

    public static void Line(Vec3 a, Vec3 b, Vec4 color, float thickness = 2f)
    {
      NativeLine((float*)&a, (float*)&b, DrawList.Pack(color), thickness);
    }

    /// a line with a head at `to`
    public static void Arrow(Vec3 from, Vec3 to, Vec4 color, float thickness = 2f)
    {
      NativeArrow((float*)&from, (float*)&to, DrawList.Pack(color), thickness);
    }

    /// a circle round an axis
    public static void Circle(Vec3 centre, Vec3 axis, float radius, Vec4 color, float thickness = 2f)
    {
      NativeCircle((float*)&centre, (float*)&axis, radius, DrawList.Pack(color), thickness);
    }

    /// part of a circle round an axis: from the direction `start`, which is square to the
    ///  axis, so many radians counter-clockwise seen from where the axis points
    public static void Arc(Vec3 centre, Vec3 axis, Vec3 start, float radians, float radius, Vec4 color, float thickness = 2f)
    {
      NativeArc((float*)&centre, (float*)&axis, (float*)&start, radians, radius, DrawList.Pack(color), thickness);
    }

    /// that part of a circle filled in, to the centre
    public static void Pie(Vec3 centre, Vec3 axis, Vec3 start, float radians, float radius, Vec4 color)
    {
      NativePie((float*)&centre, (float*)&axis, (float*)&start, radians, radius, DrawList.Pack(color));
    }

    /// four corners in order round it, filled
    public static void Quad(Vec3 a, Vec3 b, Vec3 c, Vec3 d, Vec4 color)
    {
      NativeQuad((float*)&a, (float*)&b, (float*)&c, (float*)&d, DrawList.Pack(color));
    }

    /// a filled dot of a size on the screen
    public static void Dot(Vec3 at, float radius, Vec4 color)
    {
      NativeDot((float*)&at, radius, DrawList.Pack(color));
    }

    public static void Label(Vec3 at, string text, Vec4 color)
    {
      using var text_utf8 = new Utf8Arg(text, stackalloc byte[Utf8Arg.StackSize]);
      NativeLabel((float*)&at, text_utf8.Pointer, DrawList.Pack(color));
    }
  }
}
