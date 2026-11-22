using OtherCsBindings;
using System;

namespace Other.UI
{
  /// the widgets the native windows are built from, so a window in C# looks like one of them
  public static class Widgets
  {
    [NativeFunction("UIBeginPropertyRow")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, float, void> NativeBeginPropertyRow;
    [NativeFunction("UIEndPropertyRow")]
    internal static unsafe delegate* unmanaged[Cdecl]<void> NativeEndPropertyRow;
    [NativeFunction("UIPropertyDisplay")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, byte*, float, float, float, float, void> NativePropertyDisplay;

    /// a row of the inspector: the label on the left, and whatever is drawn before
    ///  EndPropertyRow in the field on the right. 0 is the inspector's own label width
    public static void BeginPropertyRow(string label, float label_width = 0f)
    {
      unsafe
      {
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        NativeBeginPropertyRow(label_utf8.Pointer, label_width);
      }
    }

    public static void EndPropertyRow() { unsafe { NativeEndPropertyRow(); } }

    /// a row that shows text nobody edits
    public static void PropertyDisplay(string label, string text)
    {
      PropertyDisplay(label, text, new Vec4(0f, 0f, 0f, 0f));
    }

    /// the same in a colour of its own
    public static void PropertyDisplay(string label, string text, Vec4 color)
    {
      unsafe
      {
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        using var text_utf8 = new Utf8Arg(text, stackalloc byte[Utf8Arg.StackSize]);
        NativePropertyDisplay(label_utf8.Pointer, text_utf8.Pointer, color.X, color.Y, color.Z, color.W);
      }
    }
  }
}
