using OtherCsBindings;

namespace Other.Editor
{
  /// the Scene View: the editor's own view of the scene, and the camera it is drawn with.
  ///  the camera is the editor's and no object of the scene
  public static unsafe class SceneViewport
  {
    [NativeFunction("EditorViewCamera")]
    internal static delegate* unmanaged[Cdecl]<float*, float*, float*, NativeBool32> NativeCamera;
    [NativeFunction("EditorViewFocusSelection")]
    internal static delegate* unmanaged[Cdecl]<NativeBool32> NativeFocusSelection;

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
