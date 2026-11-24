using OtherCsBindings;

namespace Other.Editor
{
  /// the Scene View: the editor's own view of the scene, and the camera it is drawn with.
  ///  the camera is the editor's and no object of the scene. places on the screen are in
  ///  the UI's units, as UIWidgets.MousePos and DrawList have them
  public static unsafe class SceneViewport
  {
    [NativeFunction("EditorViewCamera")]
    internal static delegate* unmanaged[Cdecl]<float*, float*, float*, NativeBool32> NativeCamera;
    [NativeFunction("EditorViewFocusSelection")]
    internal static delegate* unmanaged[Cdecl]<NativeBool32> NativeFocusSelection;
    [NativeFunction("EditorViewState")]
    internal static delegate* unmanaged[Cdecl]<float*, uint> NativeState;
    [NativeFunction("EditorViewRay")]
    internal static delegate* unmanaged[Cdecl]<float, float, float*, float*, NativeBool32> NativeRay;
    [NativeFunction("EditorViewPlace")]
    internal static delegate* unmanaged[Cdecl]<float, float, float, float*, NativeBool32> NativePlace;
    [NativeFunction("EditorViewPixelSize")]
    internal static delegate* unmanaged[Cdecl]<float, float, float, float> NativePixelSize;
    [NativeFunction("EditorViewPick")]
    internal static delegate* unmanaged[Cdecl]<float, float, ulong> NativePick;

    private const uint kShown = 1u << 0;
    private const uint kHovered = 1u << 1;
    private const uint kClicked = 1u << 2;

    /// the view's window is open and in front, so its picture is on the screen
    public static bool Shown => (State(out _, out _) & kShown) != 0;

    /// the pointer is over the view's picture, with nothing in front of it
    public static bool Hovered => (State(out _, out _) & kHovered) != 0;

    /// the left button was pressed and let go on the picture this frame without the
    ///  pointer being dragged between, and without Alt: a drag, and Alt, move the camera
    public static bool Clicked => (State(out _, out _) & kClicked) != 0;

    /// where the picture's top left is on the screen
    public static Vec2 Position
    {
      get
      {
        State(out Vec2 position, out _);
        return position;
      }
    }

    public static Vec2 Size
    {
      get
      {
        State(out _, out Vec2 size);
        return size;
      }
    }

    /// where the camera is. zero when there is no view
    public static Vec3 CameraPosition
    {
      get
      {
        Read(out Vec3 position, out _, out _);
        return position;
      }
    }

    /// which way the camera looks, of length one. zero when there is no view
    public static Vec3 CameraDirection
    {
      get
      {
        Read(out _, out Vec3 direction, out _);
        return direction;
      }
    }

    /// the point in front of the camera that orbiting turns round and the wheel goes
    ///  towards: the middle of what was last focused on
    public static Vec3 FocusPoint
    {
      get
      {
        Read(out _, out _, out Vec3 focus);
        return focus;
      }
    }

    /// brings what is selected to the middle of the view, all of it in the view, looking the
    ///  way the camera already looks. false with nothing selected
    public static bool FocusOnSelection()
    {
      return NativeFocusSelection();
    }

    /// the ray from the camera through a place of the screen: where it starts and which
    ///  way it goes, of length one. false when there is no view
    public static bool Ray(Vec2 screen, out Vec3 origin, out Vec3 direction)
    {
      float* values = stackalloc float[6];
      if (!NativeRay(screen.X, screen.Y, values, values + 3))
      {
        origin = direction = new Vec3(0f, 0f, 0f);
        return false;
      }
      origin = new Vec3(values[0], values[1], values[2]);
      direction = new Vec3(values[3], values[4], values[5]);
      return true;
    }

    /// the ray from the camera through the pointer
    public static bool PointerRay(out Vec3 origin, out Vec3 direction)
    {
      return Ray(UIWidgets.MousePos, out origin, out direction);
    }

    /// where a point of the world is on the screen. false when it is behind the camera,
    ///  or there is no view
    public static bool WorldToScreen(Vec3 world, out Vec2 screen)
    {
      float* place = stackalloc float[2];
      if (!NativePlace(world.X, world.Y, world.Z, place))
      {
        screen = new Vec2(0f, 0f);
        return false;
      }
      screen = new Vec2(place[0], place[1]);
      return true;
    }

    /// how much of the world one unit of the screen covers at a point's distance. what
    ///  is to stay a size on the screen is sized by this
    public static float PixelSize(Vec3 world)
    {
      return NativePixelSize(world.X, world.Y, world.Z);
    }

    /// the object drawn at a place of the screen: the nearest whose triangles the ray from
    ///  there meets, posed as it is drawn. null when there is none. an object that is
    ///  hidden, or has no model, is not found
    public static SceneObject? Pick(Vec2 screen)
    {
      return SceneObject.FromId(NativePick(screen.X, screen.Y));
    }

    /// the object drawn under the pointer. null when there is none, or the pointer is not
    ///  over the view
    public static SceneObject? ObjectUnderPointer => Hovered ? Pick(UIWidgets.MousePos) : null;

    private static uint State(out Vec2 position, out Vec2 size)
    {
      float* rectangle = stackalloc float[4];
      uint flags = NativeState(rectangle);
      position = new Vec2(rectangle[0], rectangle[1]);
      size = new Vec2(rectangle[2], rectangle[3]);
      return flags;
    }

    private static void Read(out Vec3 position, out Vec3 direction, out Vec3 focus)
    {
      float* values = stackalloc float[9];
      if (!NativeCamera(values, values + 3, values + 6))
      {
        position = direction = focus = new Vec3(0f, 0f, 0f);
        return;
      }
      position = new Vec3(values[0], values[1], values[2]);
      direction = new Vec3(values[3], values[4], values[5]);
      focus = new Vec3(values[6], values[7], values[8]);
    }
  }
}
