using System;
using OtherCsBindings;

namespace Other.Core
{
  public static class Filesystem
  {
    [NativeFunction("GetProgramFilesFolder")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, byte*> NativeGetProgramFilesFolder;

    [NativeFunction("GetAppDataFolder")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, NativeBool32, byte*> NativeGetAppDataFolder;
    [NativeFunction("GetInstallFolder")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*> NativeGetInstallFolder;

    public static string GetProgramFilesFolder(string app_name)
    {
      unsafe
      {
        using var app_name_utf8 = new Utf8Arg(app_name, stackalloc byte[Utf8Arg.StackSize]);
        return Utf8Arg.Read(NativeGetProgramFilesFolder(app_name_utf8.Pointer));
      }
    }

    public static string GetAppDataFolder(string app_name, bool create = false)
    {
      unsafe
      {
        using var app_name_utf8 = new Utf8Arg(app_name, stackalloc byte[Utf8Arg.StackSize]);
        return Utf8Arg.Read(NativeGetAppDataFolder(app_name_utf8.Pointer, create));
      }
    }

    public static string GetInstallFolder()
    {
      unsafe
      {
        return Utf8Arg.Read(NativeGetInstallFolder());
      }
    }
  }
}