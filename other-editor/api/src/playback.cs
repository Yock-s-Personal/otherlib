using OtherCsBindings;

namespace Other.Editor
{
  public enum PlaybackState
  {
    Stopped = 0,
    Playing = 1,
    /// played and held: stop still puts back what the scene was before it was played
    Paused = 2,
  }

  /// playing the scene that is in the editor. stop puts the scene back as it was when play
  ///  was pressed: every object is made again, so one held from before is no longer alive
  public static unsafe class Playback
  {
    [NativeFunction("EditorPlaybackState")]
    internal static delegate* unmanaged[Cdecl]<int> NativeState;
    [NativeFunction("EditorPlaybackCommand")]
    internal static delegate* unmanaged[Cdecl]<int, void> NativeCommand;

    public static PlaybackState State => (PlaybackState)NativeState();

    public static void Play()
    {
      NativeCommand(0);
    }

    public static void Pause()
    {
      NativeCommand(1);
    }

    public static void Stop()
    {
      NativeCommand(2);
    }
  }

  /// the scene that is in the editor, as a file
  public static unsafe class EditorScene
  {
    [NativeFunction("EditorSceneSave")]
    internal static delegate* unmanaged[Cdecl]<NativeBool32> NativeSave;

    /// writes the scene to the file it was loaded from. false when there is no scene, it
    ///  has no file, or the write failed; the log says which
    public static bool Save()
    {
      return NativeSave();
    }
  }
}
