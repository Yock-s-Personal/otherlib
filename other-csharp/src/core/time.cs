using System;
using OtherCsBindings;

namespace Other.Core
{
  public static class Time
  {
    [NativeFunction("TimeGetDeltaTime")]
    internal static unsafe delegate* unmanaged[Cdecl]<float> NativeGetDeltaTime;
    [NativeFunction("TimeGetElapsedTime")]
    internal static unsafe delegate* unmanaged[Cdecl]<float> NativeGetElapsedTime;
    [NativeFunction("TimeGetFrameCount")]
    internal static unsafe delegate* unmanaged[Cdecl]<Int64> NativeGetFrameCount;

    public static float DeltaTime { get { unsafe { return NativeGetDeltaTime(); } } }
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