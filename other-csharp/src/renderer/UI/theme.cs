using OtherCsBindings;
using System;
using System.Collections.Generic;

namespace Other.UI
{
  /// what a font is for. font_role on the native side is the same list in the same order
  public enum FontRole : int
  {
    Default = 0,
    Monospace = 1,
    Heading = 2,
    Small = 3,
    Icons = 4,
  }

  /// the theme the UI is drawn with: its colours by the names a theme file gives them, and
  ///  its fonts by what they are for. a driver with no theme has the colours the engine was
  ///  built with and one font
  public static class Theme
  {
    [NativeFunction("UIThemeColor")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, float*, NativeBool32> NativeThemeColor;
    [NativeFunction("UIPushFont")]
    internal static unsafe delegate* unmanaged[Cdecl]<int, void> NativePushFont;
    [NativeFunction("UIPopFont")]
    internal static unsafe delegate* unmanaged[Cdecl]<void> NativePopFont;

    /// what Color hands back for a name that is no token: hard to miss on screen
    private static readonly Vec4 kNoSuchToken = new Vec4(1f, 0f, 1f, 1f);
    /// a name that is no token is an error once, not once a frame
    private static readonly HashSet<string> reported = new();

    /// false when no token has the name
    public static bool TryColor(string token, out Vec4 color)
    {
      unsafe
      {
        using var token_utf8 = new Utf8Arg(token, stackalloc byte[Utf8Arg.StackSize]);
        float* rgba = stackalloc float[4];
        bool found = NativeThemeColor(token_utf8.Pointer, rgba);
        color = found ? new Vec4(rgba[0], rgba[1], rgba[2], rgba[3]) : kNoSuchToken;
        return found;
      }
    }

    /// the colour a token has now: "core.accent", "core.text_error". a name that is no token
    ///  is magenta, and an error in the log once
    public static Vec4 Color(string token)
    {
      if (!TryColor(token, out Vec4 color) && reported.Add(token))
      {
        Core.Logger.LogError($"'{token}' is not the name of a colour token: it is drawn magenta. This is said once.");
      }
      return color;
    }

    /// what is drawn until PopFont is in the theme's font for that role. a role the theme
    ///  does not give is the default font
    public static void PushFont(FontRole role) { unsafe { NativePushFont((int)role); } }
    public static void PopFont() { unsafe { NativePopFont(); } }
  }
}
