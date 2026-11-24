using OtherCsBindings;
using System;

namespace Other.UI
{
  /// the widgets the native windows are built from, so a window in C# looks like one of them
  public static partial class Widgets
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

    /// what a dragged scene object is carried as: its id. a list of objects starts such a
    ///  drag, and whatever takes an object (another row, a field of a script) accepts it
    public const string kObjectPayload = "OTHER_SCENE_OBJECT";

    /// after an item: an object can be dragged from it. true while it is being dragged
    public static bool ObjectDragSource(SceneObject item)
    {
      if (!UIWidgets.BeginDragDropSource())
      {
        return false;
      }

      unsafe
      {
        ulong id = item.ObjectID;
        UIWidgets.SetDragDropPayload(kObjectPayload, (IntPtr)(&id), sizeof(ulong));
      }
      UIWidgets.Text(item.Name);
      UIWidgets.EndDragDropSource();
      return true;
    }

    /// after an item: the object that was dropped on it this frame. null when none was
    public static SceneObject? ObjectDropTarget()
    {
      if (!UIWidgets.BeginDragDropTarget())
      {
        return null;
      }

      byte[]? data = UIWidgets.AcceptDragDropPayload(kObjectPayload);
      UIWidgets.EndDragDropTarget();
      return data != null && data.Length == sizeof(ulong) ? SceneObject.FromId(BitConverter.ToUInt64(data, 0)) : null;
    }

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
