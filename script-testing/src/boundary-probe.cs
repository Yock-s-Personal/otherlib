using System;
using Other;
using Other.Core;
using OtherCsBindings;

/// fields for the suite to bind test functions to: one declared right, and one for each way
///  of declaring a binding wrong that the binder has to refuse
public static class BindingProbe
{
  [NativeFunction("TestBindingRight")]
  internal static unsafe delegate* unmanaged[Cdecl]<int, int> NativeRight;
  /// the native function takes one argument
  [NativeFunction("TestBindingWrongCount")]
  internal static unsafe delegate* unmanaged[Cdecl]<int, int, int> NativeWrongCount;
  /// the native function returns an int
  [NativeFunction("TestBindingWrongResult")]
  internal static unsafe delegate* unmanaged[Cdecl]<int, float> NativeWrongResult;
  /// the convention every binding had before
  [NativeFunction("TestBindingManaged")]
  internal static unsafe delegate*<int, int> NativeManaged;
  /// a struct by value: nothing native has this signature
  [NativeFunction("TestBindingStruct")]
  internal static unsafe delegate* unmanaged[Cdecl]<Vec3, int> NativeStruct;
  [NativeFunction("TestBindingEcho")]
  internal static unsafe delegate* unmanaged[Cdecl]<byte*, byte*> NativeEcho;
  [NativeFunction("TestBindingLength")]
  internal static unsafe delegate* unmanaged[Cdecl]<byte*, int> NativeLength;

  public static int CallRight(int value)
  {
    unsafe { return NativeRight(value); }
  }

  /// one bit for each field that holds a function: 1 right, 2 wrong count, 4 wrong result,
  ///  8 managed, 16 struct, 32 echo, 64 length
  public static int BoundMask()
  {
    unsafe
    {
      return (NativeRight != null ? 1 : 0) | (NativeWrongCount != null ? 2 : 0) | (NativeWrongResult != null ? 4 : 0) |
             (NativeManaged != null ? 8 : 0) | (NativeStruct != null ? 16 : 0) | (NativeEcho != null ? 32 : 0) | (NativeLength != null ? 64 : 0);
    }
  }

  private static string? Case(int which)
  {
    switch (which)
    {
      case 0: return "Compile Project##window";
      case 1: return "grüße ☃ 日本語";
      case 2: return "";
      case 3: return null;
      default: return new string('x', 5000) + "é";
    }
  }

  /// 1 when the string of that case reaches native code whole and comes back the same
  public static int EchoCase(int which)
  {
    string? text = Case(which);
    string expected = text ?? "";
    unsafe
    {
      using var text_utf8 = new Utf8Arg(text, stackalloc byte[Utf8Arg.StackSize]);
      int length = NativeLength(text_utf8.Pointer);
      string back = Utf8Arg.Read(NativeEcho(text_utf8.Pointer));
      return length == System.Text.Encoding.UTF8.GetByteCount(expected) && back == expected ? 1 : 0;
    }
  }

  /// 1 when text native code edits in a buffer of that many bytes reads back as it should
  public static int BufferCase(int capacity)
  {
    Span<byte> buffer = stackalloc byte[capacity];
    Utf8Arg.Fill("abcdefé", buffer);
    string kept = Utf8Arg.Read(buffer);
    /// the text is 8 bytes of UTF-8; a smaller buffer keeps what fits, never half a letter
    string expected = capacity >= 9 ? "abcdefé" : "abcdef".Substring(0, Math.Min(6, capacity - 1));
    return kept == expected ? 1 : 0;
  }
}

/// handlers for the suite to raise events at, and what each last received
public static class EventProbe
{
  private static Subscription? plain;
  private static Subscription? typed_u64;
  private static Subscription? typed_int;
  private static Subscription? typed_text;
  private static Subscription? typed_vec3;
  private static Subscription? typed_bool;
  private static Subscription? wrong;

  private static int plain_calls;
  private static ulong last_u64;
  private static int last_int;
  private static string last_text = "";
  private static Vec3 last_vec3;
  private static bool last_bool;
  private static int wrong_calls;

  public static int SubscribeAll()
  {
    plain_calls = 0;
    last_u64 = 0;
    last_int = 0;
    last_text = "";
    last_vec3 = default;
    last_bool = false;
    wrong_calls = 0;

    plain = Events.Subscribe("probe.plain", () => plain_calls++);
    typed_u64 = Events.Subscribe<ulong>("probe.u64", value => last_u64 = value);
    /// an event that carries a 64-bit id, taken as an int while it fits
    typed_int = Events.Subscribe<int>("probe.u64", value => last_int = value);
    typed_text = Events.Subscribe<string>("probe.text", value => last_text = value);
    typed_vec3 = Events.Subscribe<Vec3>("probe.vec3", value => last_vec3 = value);
    typed_bool = Events.Subscribe<bool>("probe.bool", value => last_bool = value);
    /// asks for a float and is sent text
    wrong = Events.Subscribe<float>("probe.text", value => wrong_calls++);

    Subscription?[] all = { plain, typed_u64, typed_int, typed_text, typed_vec3, typed_bool, wrong };
    int active = 0;
    foreach (Subscription? subscription in all)
    {
      active += subscription != null && subscription.Active ? 1 : 0;
    }
    return active;
  }

  public static int PlainCalls() => plain_calls;
  public static ulong LastU64() => last_u64;
  public static int LastInt() => last_int;
  public static int TextIs(int which) => last_text == (which == 0 ? "" : "grüße from an event") ? 1 : 0;
  public static int Vec3IsOneTwoThree() => last_vec3.X == 1f && last_vec3.Y == 2f && last_vec3.Z == 3f ? 1 : 0;
  public static int LastBool() => last_bool ? 1 : 0;
  public static int WrongCalls() => wrong_calls;

  public static int StopListeningForU64()
  {
    typed_u64?.Dispose();
    return typed_u64 != null && !typed_u64.Active ? 1 : 0;
  }

  /// raises events for native listeners, one of each kind a script can send
  public static int RaiseFromScript()
  {
    Events.Register("probe.from-script.u64");
    Events.Register("probe.from-script.text");
    Events.Register("probe.from-script.vec3");
    Events.Register("probe.from-script.bool");
    Events.Trigger("probe.from-script.u64", 77ul);
    Events.Trigger("probe.from-script.text", "from a script");
    Events.Trigger("probe.from-script.vec3", new Vec3(4f, 5f, 6f));
    Events.Trigger("probe.from-script.bool", true);
    return 1;
  }
}
