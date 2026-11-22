using System;
using Other.UI;

/// the enums that mirror ImGui's, read member by member so the suite can compare each with
///  ImGui's header. names are handed over in ordinal order, which is the order of the table
///  the suite holds
public static class UiEnumProbe
{
  private static readonly Type[] enums =
  {
    typeof(WindowFlags), typeof(ChildFlags), typeof(TreeNodeFlags), typeof(TableFlags), typeof(InputTextFlags),
    typeof(StyleColor), typeof(StyleVar), typeof(Condition), typeof(Key), typeof(KeyMods),
  };

  private static string[] Names(int which)
  {
    string[] names = Enum.GetNames(enums[which]);
    Array.Sort(names, StringComparer.Ordinal);
    return names;
  }

  /// FNV-1a over the name's characters, the same sum the suite makes of its own copy
  private static int Hash(string text)
  {
    uint hash = 2166136261;
    foreach (char c in text)
    {
      hash = (hash ^ c) * 16777619;
    }
    return unchecked((int)hash);
  }

  /// -1 past the last enum
  public static int Count(int which) { return which < enums.Length ? Names(which).Length : -1; }
  public static int EnumNameHash(int which) { return Hash(enums[which].Name); }
  public static int NameHash(int which, int index) { return Hash(Names(which)[index]); }
  public static int Value(int which, int index) { return Convert.ToInt32(Enum.Parse(enums[which], Names(which)[index])); }
}
