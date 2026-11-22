using System;
using OtherCsBindings;

namespace Other.Core
{
  public static class Application
  {
    [NativeFunction("DriverGetState")]
    internal static unsafe delegate* unmanaged[Cdecl]<int> NativeGetState;
    [NativeFunction("DriverRequestShutdown")]
    internal static unsafe delegate* unmanaged[Cdecl]<void> NativeRequestShutdown;
    [NativeFunction("DriverGetProjectName")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*> NativeGetProjectName;

    [NativeFunction("ConfigGetString")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, byte*, byte*, byte*> NativeConfigGetString;
    [NativeFunction("ConfigGetInt")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, byte*, int, int> NativeConfigGetInt;
    [NativeFunction("ConfigGetFloat")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, byte*, float, float> NativeConfigGetFloat;
    [NativeFunction("ConfigGetBool")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, byte*, NativeBool32, NativeBool32> NativeConfigGetBool;

    public static DriverState State
    {
      get { unsafe { return (DriverState)NativeGetState(); } }
    }
    public static void RequestShutdown()
    {
      unsafe { NativeRequestShutdown(); }
    }

    public static string ProjectName
    {
      get
      {
        unsafe
        {
          return Utf8Arg.Read(NativeGetProjectName());
        }
      }
    }

    public static string GetConfigString(string section, string key, string default_value = "")
    {
      unsafe
      {
        using var section_utf8 = new Utf8Arg(section, stackalloc byte[Utf8Arg.StackSize]);
        using var key_utf8 = new Utf8Arg(key, stackalloc byte[Utf8Arg.StackSize]);
        using var default_value_utf8 = new Utf8Arg(default_value, stackalloc byte[Utf8Arg.StackSize]);
        return Utf8Arg.Read(NativeConfigGetString(section_utf8.Pointer, key_utf8.Pointer, default_value_utf8.Pointer));
      }
    }

    public static int GetConfigInt(string section, string key, int default_value = 0)
    {
      unsafe
      {
        using var section_utf8 = new Utf8Arg(section, stackalloc byte[Utf8Arg.StackSize]);
        using var key_utf8 = new Utf8Arg(key, stackalloc byte[Utf8Arg.StackSize]);
        return NativeConfigGetInt(section_utf8.Pointer, key_utf8.Pointer, default_value);
      }
    }

    public static float GetConfigFloat(string section, string key, float default_value = 0)
    {
      unsafe
      {
        using var section_utf8 = new Utf8Arg(section, stackalloc byte[Utf8Arg.StackSize]);
        using var key_utf8 = new Utf8Arg(key, stackalloc byte[Utf8Arg.StackSize]);
        return NativeConfigGetFloat(section_utf8.Pointer, key_utf8.Pointer, default_value);
      }
    }

    public static bool GetConfigBool(string section, string key, bool default_value = false)
    {
      unsafe
      {
        using var section_utf8 = new Utf8Arg(section, stackalloc byte[Utf8Arg.StackSize]);
        using var key_utf8 = new Utf8Arg(key, stackalloc byte[Utf8Arg.StackSize]);
        return NativeConfigGetBool(section_utf8.Pointer, key_utf8.Pointer, default_value);
      }
    }
  }

  public enum DriverState
  {
    Stopped = 0,
    Initializing = 1,
    Running = 2,
    Paused = 3,
    ShuttingDown = 4,
  }
}