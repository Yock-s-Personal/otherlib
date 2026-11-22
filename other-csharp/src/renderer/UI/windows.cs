using OtherCsBindings;
using System;

namespace Other.UI
{
  /// the windows the driver can show, by name: native ones and ones declared in C# alike
  public static class Windows
  {
    [NativeFunction("UIRegisterWindow")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, byte*, float, float, int, ulong> NativeRegisterWindow;
    [NativeFunction("UIUnregisterWindow")]
    internal static unsafe delegate* unmanaged[Cdecl]<ulong, void> NativeUnregisterWindow;
    [NativeFunction("UISetWindowDrawFunction")]
    internal static unsafe delegate* unmanaged[Cdecl]<void*, void> NativeSetWindowDrawFunction;
    [NativeFunction("UIOpenWindow")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, NativeBool32> NativeOpenWindow;
    [NativeFunction("UICloseWindow")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, NativeBool32> NativeCloseWindow;
    [NativeFunction("UIIsWindowOpen")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, NativeBool32> NativeIsWindowOpen;
    [NativeFunction("UIWindowDrawCount")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, ulong> NativeWindowDrawCount;
    [NativeFunction("UIWindowDrawMicroseconds")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, float> NativeWindowDrawMicroseconds;

    /// false when no window has that name
    public static bool Open(string name)
    {
      unsafe
      {
        using var name_utf8 = new Utf8Arg(name, stackalloc byte[Utf8Arg.StackSize]);
        return NativeOpenWindow(name_utf8.Pointer);
      }
    }

    /// false when no window has that name
    public static bool Close(string name)
    {
      unsafe
      {
        using var name_utf8 = new Utf8Arg(name, stackalloc byte[Utf8Arg.StackSize]);
        return NativeCloseWindow(name_utf8.Pointer);
      }
    }

    public static bool IsOpen(string name)
    {
      unsafe
      {
        using var name_utf8 = new Utf8Arg(name, stackalloc byte[Utf8Arg.StackSize]);
        return NativeIsWindowOpen(name_utf8.Pointer);
      }
    }

    /// how many times a window declared in C# has been asked to draw its body. a window that
    ///  is closed, collapsed or hidden behind another tab is not asked
    public static ulong DrawCount(string name)
    {
      unsafe
      {
        using var name_utf8 = new Utf8Arg(name, stackalloc byte[Utf8Arg.StackSize]);
        return NativeWindowDrawCount(name_utf8.Pointer);
      }
    }

    /// how long the last of those took, the crossing into C# included
    public static float LastDrawMicroseconds(string name)
    {
      unsafe
      {
        using var name_utf8 = new Utf8Arg(name, stackalloc byte[Utf8Arg.StackSize]);
        return NativeWindowDrawMicroseconds(name_utf8.Pointer);
      }
    }
  }
}
