using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;

namespace Other.Core
{
  /// what a saved field holds. behavior_field_kind on the native side is the same list in the
  ///  same order
  internal enum FieldKind : byte
  {
    None = 0,
    Bool,
    Int32,
    UInt32,
    Int64,
    UInt64,
    Float,
    Double,
    Text,
    Enum,
    Asset,
    Vec2,
    Vec3,
    Vec4,
    Quat,
    Struct,
    Object,
  }

  /// bytes written one after another into a buffer that is kept and grows
  internal sealed class FieldWriter
  {
    private byte[] buffer = new byte[512];
    private int length;

    public int Length => length;
    public ReadOnlySpan<byte> Written => buffer.AsSpan(0, length);

    public void Clear()
    {
      length = 0;
    }

    /// back to a length it had: what was written since is given up
    public void Truncate(int to)
    {
      length = Math.Clamp(to, 0, length);
    }

    private Span<byte> Take(int size)
    {
      if (length + size > buffer.Length)
      {
        Array.Resize(ref buffer, Math.Max(buffer.Length * 2, length + size));
      }
      Span<byte> taken = buffer.AsSpan(length, size);
      length += size;
      return taken;
    }

    public void U8(byte value) { Take(1)[0] = value; }
    public void U16(ushort value) { BinaryPrimitives.WriteUInt16LittleEndian(Take(2), value); }
    public void I32(int value) { BinaryPrimitives.WriteInt32LittleEndian(Take(4), value); }
    public void U32(uint value) { BinaryPrimitives.WriteUInt32LittleEndian(Take(4), value); }
    public void I64(long value) { BinaryPrimitives.WriteInt64LittleEndian(Take(8), value); }
    public void U64(ulong value) { BinaryPrimitives.WriteUInt64LittleEndian(Take(8), value); }
    public void F32(float value) { BinaryPrimitives.WriteSingleLittleEndian(Take(4), value); }
    public void F64(double value) { BinaryPrimitives.WriteDoubleLittleEndian(Take(8), value); }
    public void Bytes(ReadOnlySpan<byte> bytes) { bytes.CopyTo(Take(bytes.Length)); }

    public void Utf8(string text)
    {
      Encoding.UTF8.GetBytes(text, Take(Encoding.UTF8.GetByteCount(text)));
    }

    /// room for a size that is not known until what it counts has been written
    public int Reserve32()
    {
      int at = length;
      U32(0);
      return at;
    }

    public void Patch32(int at, uint value)
    {
      BinaryPrimitives.WriteUInt32LittleEndian(buffer.AsSpan(at, 4), value);
    }
  }

  /// a behaviour's saved fields, to and from the stream the engine keeps (behavior_fields.hpp
  ///  has its layout). what is saved: every public field, and a private one marked
  ///  [SerializableField]; [NonSerialized] keeps a public one out. never a static field, a
  ///  readonly one, a property, or a field of a kind the stream cannot hold
  internal static unsafe class BehaviorFields
  {
    internal sealed class SavedField
    {
      public FieldInfo info = null!;
      public FieldKind kind;
      public string name = "";
      public byte[] name_utf8 = Array.Empty<byte>();
    }

    private static readonly Dictionary<Type, SavedField[]> saved = new();
    /// what has been said about a saved value that did not fit, so each is said once
    private static readonly HashSet<string> said = new();

    internal static void Forget(Assembly assembly)
    {
      var gone = new List<Type>();
      foreach (Type type in saved.Keys)
      {
        if (type.Assembly == assembly)
        {
          gone.Add(type);
        }
      }
      foreach (Type type in gone)
      {
        saved.Remove(type);
      }
      said.Clear();
    }

    internal static SavedField[] Saved(Type type)
    {
      if (saved.TryGetValue(type, out SavedField[]? known))
      {
        return known;
      }

      /// here before its fields are looked at, so a type that holds itself ends
      saved[type] = Array.Empty<SavedField>();

      /// a base class's fields first; its private ones are only listed on the class itself
      var chain = new List<Type>();
      for (Type? at = type; at != null && at != typeof(object) && at != typeof(ValueType); at = at.BaseType)
      {
        chain.Insert(0, at);
      }

      var fields = new List<SavedField>();
      foreach (Type at in chain)
      {
        foreach (FieldInfo info in at.GetFields(BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.DeclaredOnly))
        {
          bool wanted = info.IsPublic || info.GetCustomAttribute<SerializableFieldAttribute>() != null;
          if (!wanted || info.IsNotSerialized || info.IsInitOnly || info.IsLiteral || info.Name.StartsWith('<'))
          {
            continue;
          }
          FieldKind kind = KindOf(info.FieldType);
          if (kind != FieldKind.None)
          {
            fields.Add(new SavedField { info = info, kind = kind, name = info.Name, name_utf8 = Encoding.UTF8.GetBytes(info.Name) });
          }
        }
      }

      SavedField[] found = fields.ToArray();
      saved[type] = found;
      return found;
    }

    internal static FieldKind KindOf(Type type)
    {
      if (type == typeof(bool)) { return FieldKind.Bool; }
      if (type == typeof(sbyte) || type == typeof(short) || type == typeof(int)) { return FieldKind.Int32; }
      if (type == typeof(byte) || type == typeof(ushort) || type == typeof(uint)) { return FieldKind.UInt32; }
      if (type == typeof(long)) { return FieldKind.Int64; }
      if (type == typeof(ulong)) { return FieldKind.UInt64; }
      if (type == typeof(float)) { return FieldKind.Float; }
      if (type == typeof(double)) { return FieldKind.Double; }
      if (type == typeof(string) || type == typeof(char)) { return FieldKind.Text; }
      if (type.IsEnum) { return FieldKind.Enum; }
      if (type == typeof(AssetRef)) { return FieldKind.Asset; }
      if (type == typeof(Vec2)) { return FieldKind.Vec2; }
      if (type == typeof(Vec3)) { return FieldKind.Vec3; }
      if (type == typeof(Vec4)) { return FieldKind.Vec4; }
      if (type == typeof(Quaternion)) { return FieldKind.Quat; }
      /// another object, or a behaviour that is on another object
      if (typeof(OtherObject).IsAssignableFrom(type) || typeof(OtherBehavior).IsAssignableFrom(type)) { return FieldKind.Object; }
      if (type.IsValueType && !type.IsPrimitive && Saved(type).Length > 0) { return FieldKind.Struct; }
      return FieldKind.None;
    }

    /// the saved fields of target as a stream, appended to writer
    internal static void Write(FieldWriter writer, object target)
    {
      SavedField[] fields = Saved(target.GetType());
      writer.U32((uint)fields.Length);
      foreach (SavedField field in fields)
      {
        writer.U16((ushort)field.name_utf8.Length);
        writer.Bytes(field.name_utf8);
        writer.U8((byte)field.kind);
        int size_at = writer.Reserve32();
        int start = writer.Length;
        WriteValue(writer, field, field.info.GetValue(target));
        writer.Patch32(size_at, (uint)(writer.Length - start));
      }
    }

    private static void WriteValue(FieldWriter writer, SavedField field, object? value)
    {
      switch (field.kind)
      {
        case FieldKind.Bool: writer.U8((bool)value! ? (byte)1 : (byte)0); break;
        case FieldKind.Int32: writer.I32(Convert.ToInt32(value)); break;
        case FieldKind.UInt32: writer.U32(Convert.ToUInt32(value)); break;
        case FieldKind.Int64: writer.I64((long)value!); break;
        case FieldKind.UInt64: writer.U64((ulong)value!); break;
        case FieldKind.Float: writer.F32((float)value!); break;
        case FieldKind.Double: writer.F64((double)value!); break;
        case FieldKind.Text: writer.Utf8(value?.ToString() ?? ""); break;
        /// by its member's name: members can be moved or renumbered and a saved value
        ///  still means what it did
        case FieldKind.Enum: writer.Utf8(value!.ToString() ?? ""); break;
        case FieldKind.Asset: writer.Utf8(((AssetRef)value!).Path ?? ""); break;
        case FieldKind.Vec2: { var v = (Vec2)value!; writer.F32(v.X); writer.F32(v.Y); } break;
        case FieldKind.Vec3: { var v = (Vec3)value!; writer.F32(v.X); writer.F32(v.Y); writer.F32(v.Z); } break;
        case FieldKind.Vec4: { var v = (Vec4)value!; writer.F32(v.X); writer.F32(v.Y); writer.F32(v.Z); writer.F32(v.W); } break;
        case FieldKind.Quat: { var v = (Quaternion)value!; writer.F32(v.X); writer.F32(v.Y); writer.F32(v.Z); writer.F32(v.W); } break;
        case FieldKind.Struct: Write(writer, value!); break;
        case FieldKind.Object: writer.U64(ObjectIdOf(value)); break;
        default: break;
      }
    }

    private static void Entry(FieldWriter writer, string name, FieldKind kind, out int size_at)
    {
      writer.U16((ushort)Encoding.UTF8.GetByteCount(name));
      writer.Utf8(name);
      writer.U8((byte)kind);
      size_at = writer.Reserve32();
    }

    private static void Close(FieldWriter writer, int size_at)
    {
      writer.Patch32(size_at, (uint)(writer.Length - size_at - 4));
    }

    private static void Text(FieldWriter writer, string name, string text, ref uint count)
    {
      Entry(writer, name, FieldKind.Text, out int size_at);
      writer.Utf8(text);
      Close(writer, size_at);
      count++;
    }

    private static void Flag(FieldWriter writer, string name, ref uint count)
    {
      Entry(writer, name, FieldKind.Bool, out int size_at);
      writer.U8(1);
      Close(writer, size_at);
      count++;
    }

    private static void Number(FieldWriter writer, string name, float value, ref uint count)
    {
      Entry(writer, name, FieldKind.Float, out int size_at);
      writer.F32(value);
      Close(writer, size_at);
      count++;
    }

    /// what the saved fields of a type are, for whoever draws them: behavior_field_description
    ///  natively, which says what each entry is. read from the attributes a field has
    internal static void Describe(FieldWriter writer, Type type)
    {
      SavedField[] fields = Saved(type);
      writer.U32((uint)fields.Length);
      /// a group lasts until a field names another
      string group = "";
      foreach (SavedField field in fields)
      {
        Entry(writer, field.name, FieldKind.Struct, out int field_at);
        int count_at = writer.Reserve32();
        uint count = 0;

        Entry(writer, "kind", FieldKind.UInt32, out int kind_at);
        writer.U32((uint)field.kind);
        Close(writer, kind_at);
        count++;

        InspectorFieldAttribute? shown = field.info.GetCustomAttribute<InspectorFieldAttribute>();
        RangeAttribute? range = field.info.GetCustomAttribute<RangeAttribute>();
        InspectorGroupAttribute? grouped = field.info.GetCustomAttribute<InspectorGroupAttribute>();
        if (grouped != null)
        {
          group = grouped.GroupName ?? "";
        }

        if (!string.IsNullOrEmpty(shown?.DisplayName)) { Text(writer, "label", shown!.DisplayName!, ref count); }
        if (!string.IsNullOrEmpty(shown?.Tooltip)) { Text(writer, "tooltip", shown!.Tooltip!, ref count); }
        if (group.Length > 0) { Text(writer, "group", group, ref count); }
        if (range != null)
        {
          Number(writer, "min", range.Min, ref count);
          Number(writer, "max", range.Max, ref count);
        }
        else if (shown != null && (shown.Min != float.MinValue || shown.Max != float.MaxValue))
        {
          Number(writer, "min", shown.Min, ref count);
          Number(writer, "max", shown.Max, ref count);
        }
        if (shown != null && shown.ReadOnly) { Flag(writer, "read-only", ref count); }
        if (field.info.GetCustomAttribute<ColorFieldAttribute>() != null) { Flag(writer, "color", ref count); }
        if (field.info.GetCustomAttribute<InspectorSeparatorAttribute>() != null) { Flag(writer, "separator", ref count); }
        if (field.kind == FieldKind.Enum) { Text(writer, "members", string.Join('\n', Enum.GetNames(field.info.FieldType)), ref count); }
        if (field.kind == FieldKind.Object && typeof(OtherBehavior).IsAssignableFrom(field.info.FieldType))
        {
          Text(writer, "behavior", field.info.FieldType.FullName ?? field.info.FieldType.Name, ref count);
        }
        if (field.kind == FieldKind.Struct)
        {
          Entry(writer, "fields", FieldKind.Struct, out int nested_at);
          Describe(writer, field.info.FieldType);
          Close(writer, nested_at);
          count++;
        }

        writer.Patch32(count_at, count);
        Close(writer, field_at);
      }
    }

    /// the id of the object a field holds, or of the object the behaviour it holds is on
    private static ulong ObjectIdOf(object? value)
    {
      if (value is OtherObject held)
      {
        return held.ObjectID;
      }
      return value is OtherBehavior behavior ? behavior.ObjectID : 0;
    }

    /// each field of the stream that target still has is given its value. one that is gone,
    ///  or whose value no longer fits, is said once and left as the class has it. resolve
    ///  turns an object's id into the object; without it a field that names one is left
    internal static void Apply(object target, ReadOnlySpan<byte> stream, delegate* unmanaged[Cdecl]<void*, ulong, void*> resolve, void* context)
    {
      object boxed = target;
      Read(ref boxed, stream, resolve, context, target.GetType().Name);
    }

    private static void Read(ref object target, ReadOnlySpan<byte> stream, delegate* unmanaged[Cdecl]<void*, ulong, void*> resolve, void* context, string where)
    {
      if (stream.Length < 4)
      {
        return;
      }

      SavedField[] fields = Saved(target.GetType());
      uint count = BinaryPrimitives.ReadUInt32LittleEndian(stream);
      int at = 4;
      for (uint i = 0; i < count; i++)
      {
        if (at + 2 > stream.Length)
        {
          return;
        }
        int name_length = BinaryPrimitives.ReadUInt16LittleEndian(stream.Slice(at));
        at += 2;
        if (at + name_length + 5 > stream.Length)
        {
          return;
        }
        ReadOnlySpan<byte> name = stream.Slice(at, name_length);
        at += name_length;
        var kind = (FieldKind)stream[at];
        int size = (int)BinaryPrimitives.ReadUInt32LittleEndian(stream.Slice(at + 1));
        at += 5;
        if (size < 0 || at + size > stream.Length)
        {
          return;
        }
        ReadOnlySpan<byte> body = stream.Slice(at, size);
        /// a field is passed by its size, whatever it holds
        at += size;

        SavedField? field = null;
        foreach (SavedField candidate in fields)
        {
          if (name.SequenceEqual(candidate.name_utf8))
          {
            field = candidate;
            break;
          }
        }
        if (field == null)
        {
          Say($"{where}.{Encoding.UTF8.GetString(name)} is saved but is not a field of the script now: its value is dropped.");
          continue;
        }

        string? problem = Give(ref target, field, kind, body, resolve, context, where);
        if (problem != null)
        {
          Say($"{where}.{field.name}: {problem}; it keeps the value the script gives it.");
        }
      }
    }

    private static void Say(string what)
    {
      if (said.Add(what))
      {
        Logger.LogWarning(what);
      }
    }

    private static bool IsNumber(FieldKind kind)
    {
      return kind >= FieldKind.Int32 && kind <= FieldKind.Double;
    }

    /// null when the field has its value; otherwise what was wrong, to be said
    private static string? Give(ref object target, SavedField field, FieldKind kind, ReadOnlySpan<byte> body, delegate* unmanaged[Cdecl]<void*, ulong, void*> resolve, void* context, string where)
    {
      Type type = field.info.FieldType;
      switch (field.kind)
      {
        case FieldKind.Bool:
          if (kind != FieldKind.Bool || body.Length != 1) { break; }
          field.info.SetValue(target, body[0] != 0);
          return null;

        case FieldKind.Int32:
        case FieldKind.UInt32:
        case FieldKind.Int64:
        case FieldKind.UInt64:
        case FieldKind.Float:
        case FieldKind.Double:
        {
          /// a number is taken from any kind of number: a text scene does not say which
          if (!IsNumber(kind)) { break; }
          object? number = Number(type, kind, body);
          if (number == null) { return "the saved number does not fit in it"; }
          field.info.SetValue(target, number);
          return null;
        }

        case FieldKind.Text:
        {
          if (kind != FieldKind.Text && kind != FieldKind.Enum) { break; }
          string text = Encoding.UTF8.GetString(body);
          if (type == typeof(char))
          {
            if (text.Length != 1) { return "the saved text is not one character"; }
            field.info.SetValue(target, text[0]);
            return null;
          }
          field.info.SetValue(target, text);
          return null;
        }

        case FieldKind.Enum:
        {
          if (kind != FieldKind.Enum && kind != FieldKind.Text) { break; }
          string member = Encoding.UTF8.GetString(body);
          if (!Enum.TryParse(type, member, out object? parsed)) { return $"'{member}' is not a member of {type.Name} now"; }
          field.info.SetValue(target, parsed);
          return null;
        }

        case FieldKind.Asset:
          if (kind != FieldKind.Asset) { break; }
          field.info.SetValue(target, new AssetRef(Encoding.UTF8.GetString(body)));
          return null;

        case FieldKind.Vec2:
        case FieldKind.Vec3:
        case FieldKind.Vec4:
        case FieldKind.Quat:
        {
          int components = field.kind == FieldKind.Vec2 ? 2 : field.kind == FieldKind.Vec3 ? 3 : 4;
          if (kind < FieldKind.Vec2 || kind > FieldKind.Quat || body.Length != components * 4) { break; }
          float x = BinaryPrimitives.ReadSingleLittleEndian(body);
          float y = BinaryPrimitives.ReadSingleLittleEndian(body.Slice(4));
          float z = components > 2 ? BinaryPrimitives.ReadSingleLittleEndian(body.Slice(8)) : 0f;
          float w = components > 3 ? BinaryPrimitives.ReadSingleLittleEndian(body.Slice(12)) : 0f;
          object vector = field.kind switch
          {
            FieldKind.Vec2 => new Vec2(x, y),
            FieldKind.Vec3 => new Vec3(x, y, z),
            FieldKind.Vec4 => new Vec4(x, y, z, w),
            _ => new Quaternion(x, y, z, w),
          };
          field.info.SetValue(target, vector);
          return null;
        }

        case FieldKind.Struct:
        {
          if (kind != FieldKind.Struct) { break; }
          object nested = field.info.GetValue(target)!;
          Read(ref nested, body, resolve, context, where + "." + field.name);
          field.info.SetValue(target, nested);
          return null;
        }

        case FieldKind.Object:
        {
          if (kind != FieldKind.Object || body.Length != 8) { break; }
          ulong id = BinaryPrimitives.ReadUInt64LittleEndian(body);
          if (id == 0)
          {
            field.info.SetValue(target, null);
            return null;
          }
          /// nobody to ask which object that is: the field is left as it is
          if (resolve == null)
          {
            return null;
          }
          void* handle = resolve(context, id);
          OtherObject? named = handle == null ? null : GCHandle.FromIntPtr((IntPtr)handle).Target as OtherObject;
          if (named == null || type.IsInstanceOfType(named))
          {
            field.info.SetValue(target, named);
            return null;
          }
          /// a behaviour on that object; null when it has none of the type
          field.info.SetValue(target, typeof(OtherBehavior).IsAssignableFrom(type) ? named.FindBehavior(type) : null);
          return null;
        }

        default: break;
      }
      return $"what is saved for it is {Named(kind)} and the field is {Named(field.kind)} now";
    }

    private static string Named(FieldKind kind)
    {
      return kind switch
      {
        FieldKind.Bool => "a bool",
        FieldKind.Text => "text",
        FieldKind.Enum => "an enum member",
        FieldKind.Asset => "an asset",
        FieldKind.Struct => "a struct",
        FieldKind.Object => "an object",
        FieldKind.Vec2 or FieldKind.Vec3 or FieldKind.Vec4 or FieldKind.Quat => "a vector",
        FieldKind.None => "of no kind this build knows",
        _ => "a number",
      };
    }

    /// the saved number as the type the field is; null when it does not fit in it
    private static object? Number(Type type, FieldKind kind, ReadOnlySpan<byte> body)
    {
      try
      {
        bool whole = kind != FieldKind.Float && kind != FieldKind.Double;
        long integer = 0;
        double real = 0.0;
        switch (kind)
        {
          case FieldKind.Int32: if (body.Length != 4) { return null; } integer = BinaryPrimitives.ReadInt32LittleEndian(body); break;
          case FieldKind.UInt32: if (body.Length != 4) { return null; } integer = BinaryPrimitives.ReadUInt32LittleEndian(body); break;
          /// an unsigned 64 bit value is kept by its bits: a text scene writes it signed
          case FieldKind.Int64: if (body.Length != 8) { return null; } integer = BinaryPrimitives.ReadInt64LittleEndian(body); break;
          case FieldKind.UInt64: if (body.Length != 8) { return null; } integer = unchecked((long)BinaryPrimitives.ReadUInt64LittleEndian(body)); break;
          case FieldKind.Float: if (body.Length != 4) { return null; } real = BinaryPrimitives.ReadSingleLittleEndian(body); break;
          case FieldKind.Double: if (body.Length != 8) { return null; } real = BinaryPrimitives.ReadDoubleLittleEndian(body); break;
          default: return null;
        }

        checked
        {
          switch (Type.GetTypeCode(type))
          {
            case TypeCode.SByte: return whole ? (sbyte)integer : (sbyte)real;
            case TypeCode.Int16: return whole ? (short)integer : (short)real;
            case TypeCode.Int32: return whole ? (int)integer : (int)real;
            case TypeCode.Int64: return whole ? integer : (long)real;
            case TypeCode.Byte: return whole ? (byte)integer : (byte)real;
            case TypeCode.UInt16: return whole ? (ushort)integer : (ushort)real;
            case TypeCode.UInt32: return whole ? (uint)integer : (uint)real;
            case TypeCode.UInt64: return whole ? unchecked((ulong)integer) : (ulong)real;
            case TypeCode.Single: return whole ? (float)integer : (float)real;
            case TypeCode.Double: return whole ? (double)integer : real;
            default: return null;
          }
        }
      }
      catch (OverflowException)
      {
        return null;
      }
    }
  }
}
