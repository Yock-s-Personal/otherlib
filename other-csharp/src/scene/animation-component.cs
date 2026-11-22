using System;
using OtherCsBindings;

namespace Other
{
#nullable enable
  [NativeComponent("animation_component")]
  public class AnimationComponent : Component
  {
    [NativeFunction("GetAnimationClipPath")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, byte*> NativeGetAnimationClipPath;
    [NativeFunction("SetAnimationClipPath")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, byte*, void> NativeSetAnimationClipPath;

    public AnimationComponent(ulong object_id)
      : base(object_id)
    {
    }

    /// clips are assets: the property is the .oanim path ("" = play EmbeddedClip from
    /// the model's own clips instead).
    public string Clip {
      get
      {
        unsafe
        {
          return Utf8Arg.Read(NativeGetAnimationClipPath(ObjectId));
        }
      }
      set
      {
        unsafe
        {
          using var value_utf8 = new Utf8Arg(value ?? "", stackalloc byte[Utf8Arg.StackSize]);
          NativeSetAnimationClipPath(ObjectId, value_utf8.Pointer);
        }
      }
    }

    /// name of a clip embedded in this object's model; only consulted while Clip is "".
    [NativeField("clip_name")]
    public string EmbeddedClip { get => GetNativeString(); set => SetNativeString(value); }

    [NativeField("playing")]
    public bool Playing { get => GetBool(); set => SetBool(value); }
    [NativeField("looping")]
    public bool Looping { get => GetBool(); set => SetBool(value); }
    [NativeField("speed")]
    public float Speed { get => GetF32(); set => SetF32(value); }
    /// seconds into the clip; writable for scrubbing.
    [NativeField("time")]
    public float Time { get => GetF32(); set => SetF32(value); }

    /// play an embedded clip by name from the start.
    public void Play(string name)
    {
      Clip = "";
      EmbeddedClip = name ?? "";
      Time = 0f;
      Playing = true;
    }
  }
#nullable disable
}
