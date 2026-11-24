using System;
using OtherCsBindings;

namespace Other.Core
{
  /// what the engine's loop did over its last frames: how long a frame takes and where the
  ///  time goes, and how much time the scene and its scripts are told passes. the numbers
  ///  are made again about twice a second, so they can be read as they stand
  public static unsafe class FrameStats
  {
    [NativeFunction("FrameStatsValue")]
    internal static delegate* unmanaged[Cdecl]<int, float> NativeValue;
    [NativeFunction("FrameStatsHistory")]
    internal static delegate* unmanaged[Cdecl]<float*, int, int> NativeHistory;

    /// \note frame_stat natively is the same list in the same order
    private enum FrameStat
    {
      FramesPerSecond,
      FrameMilliseconds,
      LongestFrameMilliseconds,
      UpdateMilliseconds,
      RenderMilliseconds,
      WaitMilliseconds,
      EngineSecondsPerSecond,
      ScriptSecondsPerSecond,
      FixedStepsPerSecond,
    }

    /// how many frames back History goes
    public const int kHistory = 240;

    public static float FramesPerSecond => NativeValue((int)FrameStat.FramesPerSecond);
    /// the average frame, and the longest of them
    public static float FrameMilliseconds => NativeValue((int)FrameStat.FrameMilliseconds);
    public static float LongestFrameMilliseconds => NativeValue((int)FrameStat.LongestFrameMilliseconds);
    /// where the average frame went: the engine's update, its drawing, and what the loop
    ///  slept to hold its frame rate
    public static float UpdateMilliseconds => NativeValue((int)FrameStat.UpdateMilliseconds);
    public static float RenderMilliseconds => NativeValue((int)FrameStat.RenderMilliseconds);
    public static float WaitMilliseconds => NativeValue((int)FrameStat.WaitMilliseconds);
    /// how many seconds the scene was told passed for each second of the clock, and how
    ///  many a script was told (Time.DeltaTime added up). both are 1 when time is kept
    public static float EngineSecondsPerSecond => NativeValue((int)FrameStat.EngineSecondsPerSecond);
    public static float ScriptSecondsPerSecond => NativeValue((int)FrameStat.ScriptSecondsPerSecond);
    /// the fixed steps the scene in play takes in a second
    public static float FixedStepsPerSecond => NativeValue((int)FrameStat.FixedStepsPerSecond);

    /// the lengths of the last frames in milliseconds, oldest first, into what is given.
    ///  how many were written
    public static int History(Span<float> frame_milliseconds)
    {
      fixed (float* into = frame_milliseconds)
      {
        return NativeHistory(into, frame_milliseconds.Length);
      }
    }
  }
}
