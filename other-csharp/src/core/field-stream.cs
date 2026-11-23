using System;
using System.Buffers.Binary;
using System.Text;

namespace Other.Core
{
  /// one field of a stream as it is read. its name and body are the stream's own bytes
  internal ref struct FieldEntry
  {
    public ReadOnlySpan<byte> Name;
    public FieldKind Kind;
    public ReadOnlySpan<byte> Body;
  }

  /// reads a field stream a field at a time, without copying it: a count, then for each
  ///  field its name, its kind and its body, each with its size. it is what a behaviour's
  ///  values are captured as, and what its description is written as. a stream that is cut
  ///  short ends early and says so (Broken); nothing is trusted past its size
  internal ref struct FieldReader
  {
    private readonly ReadOnlySpan<byte> stream;
    private int offset;
    private uint left;

    public bool Broken { get; private set; }

    public FieldReader(ReadOnlySpan<byte> stream)
    {
      this.stream = stream;
      offset = 0;
      left = 0;
      Broken = false;
      /// a stream of no bytes has no fields
      if (stream.Length >= 4)
      {
        left = BinaryPrimitives.ReadUInt32LittleEndian(stream);
        offset = 4;
      }
      else if (stream.Length > 0)
      {
        Broken = true;
      }
    }

    public bool Next(out FieldEntry entry)
    {
      entry = default;
      if (left == 0 || Broken)
      {
        return false;
      }

      if (offset + 2 > stream.Length)
      {
        Broken = true;
        return false;
      }
      int name_length = BinaryPrimitives.ReadUInt16LittleEndian(stream.Slice(offset));
      offset += 2;
      if (offset + name_length + 5 > stream.Length)
      {
        Broken = true;
        return false;
      }
      entry.Name = stream.Slice(offset, name_length);
      offset += name_length;

      byte kind = stream[offset];
      uint size = BinaryPrimitives.ReadUInt32LittleEndian(stream.Slice(offset + 1));
      offset += 5;
      if (size > (uint)(stream.Length - offset))
      {
        Broken = true;
        return false;
      }
      /// a kind a later build added is kept as bytes and matches nothing here
      entry.Kind = kind <= (byte)FieldKind.Object ? (FieldKind)kind : FieldKind.None;
      entry.Body = stream.Slice(offset, (int)size);
      offset += (int)size;
      left--;
      return true;
    }

    /// the field of that name, looked for from the start
    public static bool Find(ReadOnlySpan<byte> stream, ReadOnlySpan<byte> name, out FieldEntry found)
    {
      var reader = new FieldReader(stream);
      while (reader.Next(out found))
      {
        if (found.Name.SequenceEqual(name))
        {
          return true;
        }
      }
      found = default;
      return false;
    }
  }

  /// what one saved field of a behaviour is, for whoever draws it: read from the stream the
  ///  behaviour's runtime describes it in, which is what the engine keeps (natively,
  ///  behavior_field_description). nothing here asks .NET what the field is, so a behaviour
  ///  that is not .NET is described the same way
  internal sealed class FieldDescription
  {
    public string Name = "";
    public byte[] NameUtf8 = Array.Empty<byte>();
    public FieldKind Kind;
    /// what a row calls it: its label, or its name when it has none
    public string Label = "";
    public string Tooltip = "";
    /// the fields that share a group are drawn under its name
    public string Group = "";
    public bool ReadOnly;
    public bool Color;
    /// a line is drawn above it
    public bool Separator;
    public bool HasRange;
    public float Min;
    public float Max;
    /// an enum's members, by name
    public string[] Members = Array.Empty<string>();
    public byte[][] MembersUtf8 = Array.Empty<byte[]>();
    /// a struct's own fields
    public FieldDescription[] Fields = Array.Empty<FieldDescription>();
    /// for a field that holds a behaviour: the type it holds. empty for one that holds an object
    public string Behavior = "";

    /// the last body a row showed as text, and the text: made again only when the body differs
    public byte[] ShownBody = Array.Empty<byte>();
    public int ShownLength = -1;
    public string ShownText = "";
    public ulong ShownStamp;

    /// null when the stream is cut short
    public static FieldDescription[]? Read(ReadOnlySpan<byte> stream)
    {
      int count = 0;
      var counter = new FieldReader(stream);
      while (counter.Next(out _))
      {
        count++;
      }
      if (counter.Broken)
      {
        return null;
      }

      var described = new FieldDescription[count];
      var reader = new FieldReader(stream);
      for (int i = 0; i < count && reader.Next(out FieldEntry field); i++)
      {
        if (field.Kind != FieldKind.Struct)
        {
          return null;
        }

        var description = new FieldDescription { Name = Encoding.UTF8.GetString(field.Name), NameUtf8 = field.Name.ToArray() };
        var said = new FieldReader(field.Body);
        while (said.Next(out FieldEntry about))
        {
          if (Is(about.Name, "kind") && about.Body.Length == 4)
          {
            uint kind = BinaryPrimitives.ReadUInt32LittleEndian(about.Body);
            description.Kind = kind <= (uint)FieldKind.Object ? (FieldKind)kind : FieldKind.None;
          }
          else if (Is(about.Name, "label")) { description.Label = Encoding.UTF8.GetString(about.Body); }
          else if (Is(about.Name, "tooltip")) { description.Tooltip = Encoding.UTF8.GetString(about.Body); }
          else if (Is(about.Name, "group")) { description.Group = Encoding.UTF8.GetString(about.Body); }
          else if (Is(about.Name, "behavior")) { description.Behavior = Encoding.UTF8.GetString(about.Body); }
          else if (Is(about.Name, "min") && about.Body.Length == 4)
          {
            description.Min = BinaryPrimitives.ReadSingleLittleEndian(about.Body);
            description.HasRange = true;
          }
          else if (Is(about.Name, "max") && about.Body.Length == 4)
          {
            description.Max = BinaryPrimitives.ReadSingleLittleEndian(about.Body);
            description.HasRange = true;
          }
          else if (Is(about.Name, "read-only") && about.Body.Length == 1) { description.ReadOnly = about.Body[0] != 0; }
          else if (Is(about.Name, "color") && about.Body.Length == 1) { description.Color = about.Body[0] != 0; }
          else if (Is(about.Name, "separator") && about.Body.Length == 1) { description.Separator = about.Body[0] != 0; }
          else if (Is(about.Name, "members"))
          {
            description.Members = Encoding.UTF8.GetString(about.Body).Split('\n', StringSplitOptions.RemoveEmptyEntries);
            description.MembersUtf8 = new byte[description.Members.Length][];
            for (int m = 0; m < description.Members.Length; m++)
            {
              description.MembersUtf8[m] = Encoding.UTF8.GetBytes(description.Members[m]);
            }
          }
          else if (Is(about.Name, "fields"))
          {
            FieldDescription[]? nested = Read(about.Body);
            if (nested == null)
            {
              return null;
            }
            description.Fields = nested;
          }
        }
        if (said.Broken)
        {
          return null;
        }
        if (description.Label.Length == 0)
        {
          description.Label = description.Name;
        }
        described[i] = description;
      }
      return described;
    }

    private static bool Is(ReadOnlySpan<byte> name, string what)
    {
      if (name.Length != what.Length)
      {
        return false;
      }
      for (int i = 0; i < what.Length; i++)
      {
        if (name[i] != what[i])
        {
          return false;
        }
      }
      return true;
    }
  }
}
