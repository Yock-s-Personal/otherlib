using System;
using Other;
using Other.UI;

/// the theme as C# reads it
public static class ThemeProbe
{
  private static readonly string[] tokens = { "core.accent", "core.bg1", "core.no_such_token" };

  private static int Packed(Vec4 color)
  {
    static int Byte(float channel) { return (int)(channel * 255f + 0.5f); }
    return Byte(color.X) << 16 | Byte(color.Y) << 8 | Byte(color.Z);
  }

  /// the token's red, green and blue as 0xRRGGBB. -1 when no token has the name
  public static int Color(int which)
  {
    return Theme.TryColor(tokens[which], out Vec4 color) ? Packed(color) : -1;
  }

  /// what a window gets for a name that is no token
  public static int ColorOfNoToken() { return Packed(Theme.Color(tokens[2])); }

  /// which is a FontRole by its place in the enum, so the two sides' numbers are compared
  public static int FontRoleValue(int which)
  {
    FontRole[] roles = { FontRole.Default, FontRole.Monospace, FontRole.Heading, FontRole.Small, FontRole.Icons };
    return (int)roles[which];
  }
}
