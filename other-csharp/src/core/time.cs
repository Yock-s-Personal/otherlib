using System;
using OtherCsBindings;

namespace Other.Core
{
  /// the engine's clock, as a script reads it. it is the driver's own, the same whether or
  ///  not the driver draws a UI
  public static class Time
  {
    [NativeFunction("TimeGetDeltaTime")]
    internal static unsafe delegate* unmanaged[Cdecl]<float> NativeGetDeltaTime;
    [NativeFunction("TimeGetElapsedTime")]
    internal static unsafe delegate* unmanaged[Cdecl]<float> NativeGetElapsedTime;
    [NativeFunction("TimeGetFrameCount")]
    internal static unsafe delegate* unmanaged[Cdecl]<Int64> NativeGetFrameCount;

    /// how long the frame being run is taken to be, in seconds: what the scene is updated with
    public static float DeltaTime { get { unsafe { return NativeGetDeltaTime(); } } }
    /// the frames' lengths added up since the engine's loop began
    public static float ElapsedTime { get { unsafe { return NativeGetElapsedTime(); } } }
    public static long FrameCount { get { unsafe { return NativeGetFrameCount(); } } }
    public static float FPS
    {
      get
      {
        float dt = DeltaTime;
        return dt > 0 ? 1.0f / dt : 0;
      }
    }
  }
}