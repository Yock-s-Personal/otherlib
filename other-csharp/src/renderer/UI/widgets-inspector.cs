using OtherCsBindings;
using System;

namespace Other.UI
{
  /// the inspector's own widgets: what a window that inspects an object is built from. the
  ///  engine draws each, so such a window looks like the editor's
  public static partial class Widgets
  {
    [NativeFunction("UIObjectHeader")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, int, ulong, NativeBool32> NativeObjectHeader;
    [NativeFunction("UIBeginComponentSection")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, float, float, float, float, NativeBool32, NativeBool32*, NativeBool32> NativeBeginComponentSection;
    [NativeFunction("UIEndComponentSection")]
    internal static unsafe delegate* unmanaged[Cdecl]<void> NativeEndComponentSection;
    [NativeFunction("UIComponentFields")]
    internal static unsafe delegate* unmanaged[Cdecl]<ulong, ulong, NativeBool32> NativeComponentFields;
    [NativeFunction("UIAddComponentButton")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, NativeBool32> NativeAddComponentButton;
    [NativeFunction("UIPropertyBool")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, NativeBool32*, NativeBool32> NativePropertyBool;
    [NativeFunction("UIPropertyFloat")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, float*, float, NativeBool32> NativePropertyFloat;
    [NativeFunction("UIPropertyInt32")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, int*, NativeBool32> NativePropertyInt32;
    [NativeFunction("UIPropertyUInt32")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, uint*, NativeBool32> NativePropertyUInt32;
    [NativeFunction("UIPropertyInt64")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, long*, NativeBool32> NativePropertyInt64;
    [NativeFunction("UIPropertyUInt64")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, ulong*, NativeBool32> NativePropertyUInt64;
    [NativeFunction("UIPropertyVec2")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, float*, float, NativeBool32> NativePropertyVec2;
    [NativeFunction("UIPropertyVec3")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, float*, float, NativeBool32> NativePropertyVec3;
    [NativeFunction("UIPropertyVec4")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, float*, float, NativeBool32> NativePropertyVec4;
    [NativeFunction("UIPropertyText")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, byte*, int, NativeBool32> NativePropertyText;
    [NativeFunction("UIAssetDropTarget")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, int, NativeBool32> NativeAssetDropTarget;

    /// the longest text a row edits, in bytes of UTF-8 with its terminator
    public const int kTextSize = 256;
    private const int kPathSize = 512;

    /// text the engine edits in place. one is enough: a row is drawn and read at once
    private static readonly byte[] text_buffer = new byte[kPathSize];

    /// the header of the object being inspected, with its name to edit. true when another
    ///  name was entered, which name then is. the name is given and not asked for, since
    ///  asking the engine for it makes a string each time
    public static bool ObjectHeader(ref string name, ulong object_id)
    {
      Utf8Arg.Fill(name, text_buffer.AsSpan(0, kTextSize));
      unsafe
      {
        fixed (byte* buffer = text_buffer)
        {
          if (!NativeObjectHeader(buffer, kTextSize, object_id))
          {
            return false;
          }
        }
      }
      name = Utf8Arg.Read(text_buffer.AsSpan(0, kTextSize));
      return true;
    }

    /// a component's section: its header, which opens and shuts it. true while it is open.
    ///  EndComponentSection is called either way. remove is true when its remove button
    ///  was clicked, which a section that is not removable does not have
    public static bool BeginComponentSection(string name, Vec4 color, bool removable, out bool remove)
    {
      unsafe
      {
        using var name_utf8 = new Utf8Arg(name, stackalloc byte[Utf8Arg.StackSize]);
        NativeBool32 asked = false;
        bool open = NativeBeginComponentSection(name_utf8.Pointer, color.X, color.Y, color.Z, color.W, removable, &asked);
        remove = asked;
        return open;
      }
    }

    public static bool BeginComponentSection(ComponentType type, out bool remove)
    {
      return BeginComponentSection(type.Name, type.Color, type.Removable, out remove);
    }

    public static void EndComponentSection() { unsafe { NativeEndComponentSection(); } }

    /// the rows of the object's component of that kind, drawn by the engine from what the
    ///  component is. true when one was changed. an object's scripts have no rows here:
    ///  Behaviors draws them
    public static bool ComponentFields(SceneObject item, ComponentType type)
    {
      unsafe { return NativeComponentFields(item.ObjectID, type.Id); }
    }

    public static bool AddComponentButton(string label)
    {
      unsafe
      {
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        return NativeAddComponentButton(label_utf8.Pointer);
      }
    }

    /// a row for a value: its label, and the field that edits it. true when it was changed
    public static bool PropertyBool(string label, ref bool value)
    {
      unsafe
      {
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        NativeBool32 on = value;
        bool changed = NativePropertyBool(label_utf8.Pointer, &on);
        value = on;
        return changed;
      }
    }

    public static bool PropertyFloat(string label, ref float value, float speed = 0.1f)
    {
      unsafe
      {
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        fixed (float* at = &value)
        {
          return NativePropertyFloat(label_utf8.Pointer, at, speed);
        }
      }
    }

    public static bool PropertyInt(string label, ref int value)
    {
      unsafe
      {
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        fixed (int* at = &value)
        {
          return NativePropertyInt32(label_utf8.Pointer, at);
        }
      }
    }

    public static bool PropertyUInt(string label, ref uint value)
    {
      unsafe
      {
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        fixed (uint* at = &value)
        {
          return NativePropertyUInt32(label_utf8.Pointer, at);
        }
      }
    }

    public static bool PropertyLong(string label, ref long value)
    {
      unsafe
      {
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        fixed (long* at = &value)
        {
          return NativePropertyInt64(label_utf8.Pointer, at);
        }
      }
    }

    public static bool PropertyULong(string label, ref ulong value)
    {
      unsafe
      {
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        fixed (ulong* at = &value)
        {
          return NativePropertyUInt64(label_utf8.Pointer, at);
        }
      }
    }

    public static bool PropertyVec2(string label, ref Vec2 value, float speed = 0.1f)
    {
      unsafe
      {
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        float* xy = stackalloc float[2] { value.X, value.Y };
        bool changed = NativePropertyVec2(label_utf8.Pointer, xy, speed);
        value = new Vec2(xy[0], xy[1]);
        return changed;
      }
    }

    public static bool PropertyVec3(string label, ref Vec3 value, float speed = 0.1f)
    {
      unsafe
      {
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        float* xyz = stackalloc float[3] { value.X, value.Y, value.Z };
        bool changed = NativePropertyVec3(label_utf8.Pointer, xyz, speed);
        value = new Vec3(xyz[0], xyz[1], xyz[2]);
        return changed;
      }
    }

    public static bool PropertyVec4(string label, ref Vec4 value, float speed = 0.1f)
    {
      unsafe
      {
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        float* xyzw = stackalloc float[4] { value.X, value.Y, value.Z, value.W };
        bool changed = NativePropertyVec4(label_utf8.Pointer, xyzw, speed);
        value = new Vec4(xyzw[0], xyzw[1], xyzw[2], xyzw[3]);
        return changed;
      }
    }

    /// text of up to kTextSize bytes. a string is made only when it was changed
    public static bool PropertyText(string label, ref string text)
    {
      Utf8Arg.Fill(text, text_buffer.AsSpan(0, kTextSize));
      if (!PropertyTextBytes(label, text_buffer.AsSpan(0, kTextSize)))
      {
        return false;
      }
      text = Utf8Arg.Read(text_buffer.AsSpan(0, kTextSize));
      return true;
    }

    /// the same on text that is already UTF-8 with its terminator, edited in place
    internal static bool PropertyTextBytes(string label, Span<byte> text)
    {
      unsafe
      {
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        fixed (byte* buffer = text)
        {
          return NativePropertyText(label_utf8.Pointer, buffer, text.Length);
        }
      }
    }

    /// after an item: the path of the asset that the asset browser's drag dropped on it this
    ///  frame. null when none was
    public static string? AssetDropTarget()
    {
      unsafe
      {
        fixed (byte* buffer = text_buffer)
        {
          return NativeAssetDropTarget(buffer, kPathSize) ? Utf8Arg.Read(buffer) : null;
        }
      }
    }
  }
}
