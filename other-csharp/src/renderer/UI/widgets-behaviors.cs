using Other.Core;
using OtherCsBindings;
using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.Reflection;
using System.Text;

namespace Other.UI
{
  /// the rows of an object's scripts: each behaviour's switch, then its saved fields. what
  ///  a behaviour's fields are and what they hold is asked of the engine, which asks the
  ///  behaviour's runtime; nothing here asks .NET, so a behaviour that is not .NET would be
  ///  drawn the same way
  public static partial class Widgets
  {
    [NativeFunction("BehaviorCount")]
    internal static unsafe delegate* unmanaged[Cdecl]<ulong, int> NativeBehaviorCount;
    [NativeFunction("BehaviorAt")]
    internal static unsafe delegate* unmanaged[Cdecl]<ulong, int, NativeBool32*, NativeBool32*, byte*> NativeBehaviorAt;
    [NativeFunction("BehaviorSetEnabledOn")]
    internal static unsafe delegate* unmanaged[Cdecl]<ulong, byte*, NativeBool32, NativeBool32> NativeBehaviorSetEnabledOn;
    [NativeFunction("BehaviorDescribe")]
    internal static unsafe delegate* unmanaged[Cdecl]<ulong, byte*, byte*, int, int> NativeBehaviorDescribe;
    [NativeFunction("BehaviorCapture")]
    internal static unsafe delegate* unmanaged[Cdecl]<ulong, byte*, byte*, int, int> NativeBehaviorCapture;
    [NativeFunction("BehaviorApply")]
    internal static unsafe delegate* unmanaged[Cdecl]<ulong, byte*, byte*, int, void> NativeBehaviorApply;

    /// one behaviour of the object being drawn, kept so that its name is not made into a
    ///  string every frame
    private sealed class BehaviorRow
    {
      public string type_name = "";
      /// with its terminator, as the engine is given it
      public byte[] type_utf8 = Array.Empty<byte>();
      public string class_name = "";
      public string name_space = "";
    }

    private static readonly List<BehaviorRow> behavior_rows = new();
    private static ulong behavior_rows_of;
    /// what a type's fields are, by its name. forgotten when a script assembly is unloaded:
    ///  the next build's type may have other fields
    private static readonly Dictionary<string, FieldDescription[]> field_descriptions = new();
    private static bool watching_script_unloads;
    private static byte[] field_values = new byte[1024];
    private static readonly FieldWriter field_edit = new();
    private static Vec4 text_muted;
    private static Vec4 text_disabled;

    /// the rows of the object's behaviours. true when the user changed something: a switch,
    ///  or a field, which the behaviour has by the time this returns
    public static bool Behaviors(SceneObject item)
    {
      if (item == null || !item.IsAlive)
      {
        return false;
      }
      if (!watching_script_unloads)
      {
        watching_script_unloads = true;
        ScriptAssemblies.Unloading += ForgetDescriptions;
      }

      text_muted = Theme.Color("core.text_muted");
      text_disabled = Theme.Color("core.text_disabled");

      ulong id = item.ObjectID;
      RefreshBehaviorRows(id);
      if (behavior_rows.Count == 0)
      {
        UIBindings.TextDisabled("No behaviors attached.");
        return false;
      }

      bool changed = false;
      for (int i = 0; i < behavior_rows.Count; i++)
      {
        changed |= DrawBehavior(id, i, behavior_rows[i]);
      }
      return changed;
    }

    private static void ForgetDescriptions(Assembly assembly)
    {
      field_descriptions.Clear();
      behavior_rows.Clear();
      behavior_rows_of = 0;
      if (assembly == typeof(Widgets).Assembly)
      {
        ScriptAssemblies.Unloading -= ForgetDescriptions;
        watching_script_unloads = false;
      }
    }

    /// the rows are made again only when the object, or what it has, is not what they say
    private static void RefreshBehaviorRows(ulong id)
    {
      unsafe
      {
        int count = NativeBehaviorCount(id);
        bool same = id == behavior_rows_of && count == behavior_rows.Count;
        NativeBool32 enabled = false;
        NativeBool32 built = false;
        for (int i = 0; same && i < count; i++)
        {
          same = SameText(NativeBehaviorAt(id, i, &enabled, &built), behavior_rows[i].type_utf8);
        }
        if (same)
        {
          return;
        }

        behavior_rows_of = id;
        behavior_rows.Clear();
        for (int i = 0; i < count; i++)
        {
          string type_name = Utf8Arg.Read(NativeBehaviorAt(id, i, &enabled, &built));
          int dot = type_name.LastIndexOf('.');
          byte[] utf8 = new byte[Encoding.UTF8.GetByteCount(type_name) + 1];
          Encoding.UTF8.GetBytes(type_name, utf8);
          behavior_rows.Add(new BehaviorRow
          {
            type_name = type_name,
            type_utf8 = utf8,
            class_name = dot < 0 ? type_name : type_name.Substring(dot + 1),
            name_space = dot < 0 ? "" : type_name.Substring(0, dot),
          });
        }
      }
    }

    /// whether text the engine handed back is the text kept, which has its terminator
    private static unsafe bool SameText(byte* text, byte[] kept)
    {
      if (text == null)
      {
        return kept.Length <= 1;
      }
      for (int i = 0; i < kept.Length; i++)
      {
        if (text[i] != kept[i])
        {
          return false;
        }
      }
      return true;
    }

    private static bool DrawBehavior(ulong id, int index, BehaviorRow row)
    {
      bool changed = false;
      UIBindings.PushID(index);
      bool open = UIBindings.TreeNodeEx(row.class_name, TreeNodeFlags.DefaultOpen | TreeNodeFlags.Framed | TreeNodeFlags.SpanAvailWidth);
      if (row.name_space.Length > 0)
      {
        UIBindings.SameLine();
        UIBindings.TextDisabled(row.name_space);
      }

      if (open)
      {
        unsafe
        {
          NativeBool32 was_enabled = false;
          NativeBool32 built = false;
          NativeBehaviorAt(id, index, &was_enabled, &built);

          bool enabled = was_enabled;
          if (PropertyBool("Enabled", ref enabled))
          {
            fixed (byte* type = row.type_utf8)
            {
              NativeBehaviorSetEnabledOn(id, type, enabled);
            }
            changed = true;
          }

          if (!built)
          {
            PropertyDisplay("Status", "not built; its saved values are kept", text_disabled);
          }
          else
          {
            changed |= DrawBehaviorFields(id, row);
          }
        }
        UIBindings.TreePop();
      }
      UIBindings.PopID();
      return changed;
    }

    private static unsafe bool DrawBehaviorFields(ulong id, BehaviorRow row)
    {
      FieldDescription[]? described = Described(id, row);
      if (described == null)
      {
        PropertyDisplay("Status", "what its fields are could not be read", text_disabled);
        return false;
      }

      int size;
      fixed (byte* type = row.type_utf8)
      {
        fixed (byte* values = field_values)
        {
          size = NativeBehaviorCapture(id, type, values, field_values.Length);
        }
        if (size > field_values.Length)
        {
          field_values = new byte[size * 2];
          fixed (byte* values = field_values)
          {
            size = NativeBehaviorCapture(id, type, values, field_values.Length);
          }
        }
      }
      if (size <= 0 || size > field_values.Length)
      {
        return false;
      }

      /// what is changed is written as a stream of its own, of only the fields that changed
      field_edit.Clear();
      int count_at = field_edit.Reserve32();
      uint count = 0;
      DrawFields(described, field_values.AsSpan(0, size), ref count);
      if (count == 0)
      {
        return false;
      }

      field_edit.Patch32(count_at, count);
      fixed (byte* type = row.type_utf8)
      {
        fixed (byte* edit = field_edit.Written)
        {
          NativeBehaviorApply(id, type, edit, field_edit.Length);
        }
      }
      return true;
    }

    private static unsafe FieldDescription[]? Described(ulong id, BehaviorRow row)
    {
      if (field_descriptions.TryGetValue(row.type_name, out FieldDescription[]? known))
      {
        return known;
      }

      byte[] stream = new byte[1024];
      int size;
      fixed (byte* type = row.type_utf8)
      {
        fixed (byte* bytes = stream)
        {
          size = NativeBehaviorDescribe(id, type, bytes, stream.Length);
        }
        if (size > stream.Length)
        {
          stream = new byte[size];
          fixed (byte* bytes = stream)
          {
            size = NativeBehaviorDescribe(id, type, bytes, stream.Length);
          }
        }
      }

      FieldDescription[]? described = size >= 0 && size <= stream.Length ? FieldDescription.Read(stream.AsSpan(0, size)) : null;
      if (described == null)
      {
        Core.Logger.LogError($"What the fields of the behavior '{row.type_name}' are could not be read: its rows are not drawn.");
        return null;
      }
      field_descriptions[row.type_name] = described;
      return described;
    }

    private static void DrawFields(FieldDescription[] described, ReadOnlySpan<byte> values, ref uint count)
    {
      string? group = null;
      bool group_open = true;
      for (int i = 0; i < described.Length; i++)
      {
        FieldDescription d = described[i];
        /// the fields that share a group are under one node, which can be folded away
        if (group == null || group != d.Group)
        {
          if (group != null && group.Length > 0 && group_open)
          {
            UIBindings.TreePop();
          }
          group = d.Group;
          group_open = d.Group.Length == 0 || UIBindings.TreeNodeEx(d.Group, TreeNodeFlags.DefaultOpen);
        }
        if (!group_open)
        {
          continue;
        }

        if (d.Separator)
        {
          UIBindings.Separator();
        }
        UIBindings.PushID(d.Name);
        if (!FieldReader.Find(values, d.NameUtf8, out FieldEntry field) || field.Kind != d.Kind)
        {
          PropertyDisplay(d.Label, "<unreadable>", text_disabled);
        }
        else
        {
          DrawField(d, field, ref count);
        }
        UIBindings.PopID();
      }
      if (group != null && group.Length > 0 && group_open)
      {
        UIBindings.TreePop();
      }
    }

    /// starts a field of the edit, and ends it: what is written between is its body
    private static int BeginEdit(FieldDescription d)
    {
      field_edit.U16((ushort)d.NameUtf8.Length);
      field_edit.Bytes(d.NameUtf8);
      field_edit.U8((byte)d.Kind);
      return field_edit.Reserve32();
    }

    private static void EndEdit(int size_at, ref uint count)
    {
      field_edit.Patch32(size_at, (uint)(field_edit.Length - size_at - 4));
      count++;
    }

    /// one field. when it is changed, the field with its new value goes into the edit
    private static void DrawField(FieldDescription d, FieldEntry f, ref uint count)
    {
      ReadOnlySpan<byte> body = f.Body;
      if (d.ReadOnly && d.Kind != FieldKind.Struct)
      {
        PropertyDisplay(d.Label, ShownText(d, body), text_muted);
        FieldTooltip(d);
        return;
      }

      switch (d.Kind)
      {
        case FieldKind.Bool:
        {
          bool on = body.Length == 1 && body[0] != 0;
          if (PropertyBool(d.Label, ref on))
          {
            int at = BeginEdit(d);
            field_edit.U8(on ? (byte)1 : (byte)0);
            EndEdit(at, ref count);
          }
          break;
        }

        case FieldKind.Int32:
        {
          int v = body.Length == 4 ? BinaryPrimitives.ReadInt32LittleEndian(body) : 0;
          if (PropertyInt(d.Label, ref v))
          {
            int at = BeginEdit(d);
            field_edit.I32(v);
            EndEdit(at, ref count);
          }
          break;
        }

        case FieldKind.UInt32:
        {
          uint v = body.Length == 4 ? BinaryPrimitives.ReadUInt32LittleEndian(body) : 0;
          if (PropertyUInt(d.Label, ref v))
          {
            int at = BeginEdit(d);
            field_edit.U32(v);
            EndEdit(at, ref count);
          }
          break;
        }

        case FieldKind.Int64:
        {
          long v = body.Length == 8 ? BinaryPrimitives.ReadInt64LittleEndian(body) : 0;
          if (PropertyLong(d.Label, ref v))
          {
            int at = BeginEdit(d);
            field_edit.I64(v);
            EndEdit(at, ref count);
          }
          break;
        }

        case FieldKind.UInt64:
        {
          ulong v = body.Length == 8 ? BinaryPrimitives.ReadUInt64LittleEndian(body) : 0;
          if (PropertyULong(d.Label, ref v))
          {
            int at = BeginEdit(d);
            field_edit.U64(v);
            EndEdit(at, ref count);
          }
          break;
        }

        case FieldKind.Float:
        case FieldKind.Double:
        {
          /// a double is edited as a float: the widgets are floats
          float v = 0f;
          if (d.Kind == FieldKind.Double && body.Length == 8)
          {
            v = (float)BinaryPrimitives.ReadDoubleLittleEndian(body);
          }
          else if (body.Length == 4)
          {
            v = BinaryPrimitives.ReadSingleLittleEndian(body);
          }

          bool changed;
          if (d.HasRange)
          {
            BeginPropertyRow(d.Label);
            changed = UIBindings.SliderFloat("##value", ref v, d.Min, d.Max);
            EndPropertyRow();
          }
          else
          {
            changed = PropertyFloat(d.Label, ref v);
          }
          if (changed)
          {
            int at = BeginEdit(d);
            if (d.Kind == FieldKind.Double)
            {
              field_edit.F64(v);
            }
            else
            {
              field_edit.F32(v);
            }
            EndEdit(at, ref count);
          }
          break;
        }

        case FieldKind.Text:
        {
          /// text too long for the field is shown and not edited, so an edit cannot cut it short
          if (body.Length >= kTextSize)
          {
            PropertyDisplay(d.Label, ShownText(d, body), text_muted);
            break;
          }
          Span<byte> text = text_buffer.AsSpan(0, kTextSize);
          body.CopyTo(text);
          text[body.Length] = 0;
          if (PropertyTextBytes(d.Label, text))
          {
            int end = text.IndexOf((byte)0);
            int at = BeginEdit(d);
            field_edit.Bytes(text.Slice(0, end < 0 ? text.Length : end));
            EndEdit(at, ref count);
          }
          break;
        }

        case FieldKind.Enum:
        {
          int current = -1;
          for (int m = 0; m < d.MembersUtf8.Length && current < 0; m++)
          {
            current = body.SequenceEqual(d.MembersUtf8[m]) ? m : -1;
          }
          BeginPropertyRow(d.Label);
          if (UIBindings.BeginCombo("##value", current >= 0 ? d.Members[current] : ShownText(d, body)))
          {
            for (int m = 0; m < d.Members.Length; m++)
            {
              if (UIBindings.Selectable(d.Members[m], m == current) && m != current)
              {
                int at = BeginEdit(d);
                field_edit.Bytes(d.MembersUtf8[m]);
                EndEdit(at, ref count);
              }
            }
            UIBindings.EndCombo();
          }
          EndPropertyRow();
          break;
        }

        case FieldKind.Vec2:
        {
          if (body.Length != 8)
          {
            break;
          }
          var v = new Vec2(Float(body, 0), Float(body, 1));
          if (PropertyVec2(d.Label, ref v))
          {
            int at = BeginEdit(d);
            field_edit.F32(v.X);
            field_edit.F32(v.Y);
            EndEdit(at, ref count);
          }
          break;
        }

        case FieldKind.Vec3:
        {
          if (body.Length != 12)
          {
            break;
          }
          var v = new Vec3(Float(body, 0), Float(body, 1), Float(body, 2));
          bool changed;
          if (d.Color)
          {
            BeginPropertyRow(d.Label);
            changed = UIBindings.ColorEdit3("##value", ref v);
            EndPropertyRow();
          }
          else
          {
            changed = PropertyVec3(d.Label, ref v);
          }
          if (changed)
          {
            int at = BeginEdit(d);
            field_edit.F32(v.X);
            field_edit.F32(v.Y);
            field_edit.F32(v.Z);
            EndEdit(at, ref count);
          }
          break;
        }

        case FieldKind.Vec4:
        case FieldKind.Quat:
        {
          /// a rotation is edited as the four numbers it is saved as
          if (body.Length != 16)
          {
            break;
          }
          var v = new Vec4(Float(body, 0), Float(body, 1), Float(body, 2), Float(body, 3));
          bool changed;
          if (d.Color)
          {
            BeginPropertyRow(d.Label);
            changed = UIBindings.ColorEdit4("##value", ref v);
            EndPropertyRow();
          }
          else
          {
            changed = PropertyVec4(d.Label, ref v);
          }
          if (changed)
          {
            int at = BeginEdit(d);
            field_edit.F32(v.X);
            field_edit.F32(v.Y);
            field_edit.F32(v.Z);
            field_edit.F32(v.W);
            EndEdit(at, ref count);
          }
          break;
        }

        case FieldKind.Asset:
        {
          /// an asset is dragged onto the row from the asset browser
          bool holds = body.Length > 0;
          bool cleared = ReferenceRow(d.Label, holds ? ShownText(d, body) : "<none>", holds);
          string? dropped = AssetDropTarget();
          if (cleared || dropped != null)
          {
            int at = BeginEdit(d);
            field_edit.Utf8(dropped ?? "");
            EndEdit(at, ref count);
          }
          EndPropertyRow();
          break;
        }

        case FieldKind.Object:
        {
          /// an object is dragged onto the row from a list of them; where the field holds a
          ///  behaviour, only an object that has it is taken
          ulong held = body.Length == 8 ? BinaryPrimitives.ReadUInt64LittleEndian(body) : 0;
          bool cleared = ReferenceRow(d.Label, held != 0 ? ObjectText(d, held) : "<none>", held != 0);
          SceneObject? dropped = ObjectDropTarget();
          if (dropped != null && d.Behavior.Length > 0 && !HasBehavior(dropped.ObjectID, d.Behavior))
          {
            Core.Logger.LogWarning($"'{dropped.Name}' was not put in '{d.Label}': the field holds a {d.Behavior}, and that object has none.");
            dropped = null;
          }
          if (cleared || dropped != null)
          {
            int at = BeginEdit(d);
            field_edit.U64(dropped != null ? dropped.ObjectID : 0);
            EndEdit(at, ref count);
          }
          EndPropertyRow();
          break;
        }

        case FieldKind.Struct:
        {
          if (!UIBindings.TreeNodeEx(d.Label, TreeNodeFlags.DefaultOpen))
          {
            break;
          }
          /// only what changed is given back: the struct's other fields are left as they are
          int rewind = field_edit.Length;
          int at = BeginEdit(d);
          int nested_at = field_edit.Reserve32();
          uint nested = 0;
          DrawFields(d.Fields, body, ref nested);
          if (nested > 0)
          {
            field_edit.Patch32(nested_at, nested);
            EndEdit(at, ref count);
          }
          else
          {
            field_edit.Truncate(rewind);
          }
          UIBindings.TreePop();
          break;
        }

        default:
          PropertyDisplay(d.Label, "<a kind this build cannot draw>", text_disabled);
          break;
      }

      FieldTooltip(d);
    }

    private static float Float(ReadOnlySpan<byte> body, int index)
    {
      return BinaryPrimitives.ReadSingleLittleEndian(body.Slice(index * 4));
    }

    private static void FieldTooltip(FieldDescription d)
    {
      if (d.Tooltip.Length > 0 && UIBindings.IsItemHovered())
      {
        UIBindings.SetTooltip(d.Tooltip);
      }
    }

    /// a row that names what a field holds, as wide as the row so that it is easy to drop
    ///  on, with a button before it that makes the field hold nothing. the row is left
    ///  open for its drop target: the caller ends it. true when the button was clicked
    private static bool ReferenceRow(string label, string text, bool holds)
    {
      BeginPropertyRow(label);
      bool cleared = false;
      if (holds)
      {
        cleared = UIBindings.SmallButton("x");
        UIBindings.SameLine();
      }
      UIBindings.Selectable(text);
      return cleared;
    }

    /// a body as text, for a row that only shows it. the text is made again only when the
    ///  body is not the one it was made from
    private static string ShownText(FieldDescription d, ReadOnlySpan<byte> body)
    {
      if (d.ShownLength == body.Length && body.SequenceEqual(d.ShownBody.AsSpan(0, d.ShownLength)))
      {
        return d.ShownText;
      }
      if (d.ShownBody.Length < body.Length)
      {
        d.ShownBody = new byte[body.Length];
      }
      body.CopyTo(d.ShownBody);
      d.ShownLength = body.Length;
      d.ShownText = TextOf(d.Kind, body);
      return d.ShownText;
    }

    private static string TextOf(FieldKind kind, ReadOnlySpan<byte> body)
    {
      switch (kind)
      {
        case FieldKind.Bool: return body.Length == 1 && body[0] != 0 ? "true" : "false";
        case FieldKind.Int32: return body.Length == 4 ? BinaryPrimitives.ReadInt32LittleEndian(body).ToString() : "";
        case FieldKind.UInt32: return body.Length == 4 ? BinaryPrimitives.ReadUInt32LittleEndian(body).ToString() : "";
        case FieldKind.Int64: return body.Length == 8 ? BinaryPrimitives.ReadInt64LittleEndian(body).ToString() : "";
        case FieldKind.UInt64: return body.Length == 8 ? BinaryPrimitives.ReadUInt64LittleEndian(body).ToString() : "";
        case FieldKind.Float: return body.Length == 4 ? BinaryPrimitives.ReadSingleLittleEndian(body).ToString("F3") : "";
        case FieldKind.Double: return body.Length == 8 ? BinaryPrimitives.ReadDoubleLittleEndian(body).ToString("F3") : "";
        case FieldKind.Text:
        case FieldKind.Enum:
        case FieldKind.Asset:
          return Encoding.UTF8.GetString(body);

        case FieldKind.Object:
        {
          ulong id = body.Length == 8 ? BinaryPrimitives.ReadUInt64LittleEndian(body) : 0;
          SceneObject? named = id != 0 ? SceneObject.FromId(id) : null;
          return id == 0 ? "<none>" : named != null ? named.Name : "<gone>";
        }

        case FieldKind.Vec2:
        case FieldKind.Vec3:
        case FieldKind.Vec4:
        case FieldKind.Quat:
        {
          var text = new StringBuilder();
          for (int i = 0; i + 4 <= body.Length; i += 4)
          {
            text.Append(i == 0 ? "" : ", ").Append(BinaryPrimitives.ReadSingleLittleEndian(body.Slice(i)).ToString("F3"));
          }
          return text.ToString();
        }

        default: return "";
      }
    }

    /// the name of the object a field holds. asked of the engine again only when the field
    ///  holds another object or the scene has changed, since asking makes a string
    private static string ObjectText(FieldDescription d, ulong id)
    {
      ulong stamp = id ^ (Scene.Revision * 0x9E3779B97F4A7C15UL);
      if (d.ShownLength == -2 && d.ShownStamp == stamp)
      {
        return d.ShownText;
      }
      SceneObject? named = SceneObject.FromId(id);
      d.ShownLength = -2;
      d.ShownStamp = stamp;
      d.ShownText = named != null ? named.Name : "<gone>";
      return d.ShownText;
    }

    /// whether the object has a behaviour the engine calls that, by its full name or its
    ///  class's own
    private static unsafe bool HasBehavior(ulong id, string type_name)
    {
      int dot = type_name.LastIndexOf('.');
      string class_name = dot < 0 ? type_name : type_name.Substring(dot + 1);
      int count = NativeBehaviorCount(id);
      NativeBool32 enabled = false;
      NativeBool32 built = false;
      for (int i = 0; i < count; i++)
      {
        string has = Utf8Arg.Read(NativeBehaviorAt(id, i, &enabled, &built));
        if (has == type_name || has == class_name)
        {
          return true;
        }
      }
      return false;
    }
  }
}
