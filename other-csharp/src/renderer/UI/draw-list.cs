using OtherCsBindings;
using System;

namespace Other.UI
{
  /// shapes drawn straight into the window being drawn, over or under its widgets in the
  ///  order they are asked for. positions are on the screen: start from
  ///  UIBindings.CursorScreenPos or an item's rectangle. a colour is red, green, blue and
  ///  alpha from 0 to 1
  public static class DrawList
  {
    [NativeFunction("UIDrawLine")]
    internal static unsafe delegate* unmanaged[Cdecl]<float, float, float, float, uint, float, void> NativeDrawLine;
    [NativeFunction("UIDrawRect")]
    internal static unsafe delegate* unmanaged[Cdecl]<float, float, float, float, uint, float, float, void> NativeDrawRect;
    [NativeFunction("UIDrawRectFilled")]
    internal static unsafe delegate* unmanaged[Cdecl]<float, float, float, float, uint, float, void> NativeDrawRectFilled;
    [NativeFunction("UIDrawCircle")]
    internal static unsafe delegate* unmanaged[Cdecl]<float, float, float, uint, int, float, void> NativeDrawCircle;
    [NativeFunction("UIDrawCircleFilled")]
    internal static unsafe delegate* unmanaged[Cdecl]<float, float, float, uint, int, void> NativeDrawCircleFilled;
    [NativeFunction("UIDrawTriangle")]
    internal static unsafe delegate* unmanaged[Cdecl]<float, float, float, float, float, float, uint, float, void> NativeDrawTriangle;
    [NativeFunction("UIDrawTriangleFilled")]
    internal static unsafe delegate* unmanaged[Cdecl]<float, float, float, float, float, float, uint, void> NativeDrawTriangleFilled;
    [NativeFunction("UIDrawBezierCubic")]
    internal static unsafe delegate* unmanaged[Cdecl]<float, float, float, float, float, float, float, float, uint, float, int, void> NativeDrawBezierCubic;
    [NativeFunction("UIDrawText")]
    internal static unsafe delegate* unmanaged[Cdecl]<float, float, uint, byte*, void> NativeDrawText;
    [NativeFunction("UIDrawImage")]
    internal static unsafe delegate* unmanaged[Cdecl]<ulong, float, float, float, float, float, float, float, float, uint, void> NativeDrawImage;
    [NativeFunction("UIPushClipRect")]
    internal static unsafe delegate* unmanaged[Cdecl]<float, float, float, float, NativeBool32, void> NativePushClipRect;
    [NativeFunction("UIPopClipRect")]
    internal static unsafe delegate* unmanaged[Cdecl]<void> NativePopClipRect;

    private static readonly Vec4 kWhite = new Vec4(1f, 1f, 1f, 1f);

    /// the colour as ImGui packs it: alpha, blue, green, red from the top byte down
    public static uint Pack(Vec4 color)
    {
      static uint Byte(float channel) { return (uint)(Math.Clamp(channel, 0f, 1f) * 255f + 0.5f); }
      return Byte(color.W) << 24 | Byte(color.Z) << 16 | Byte(color.Y) << 8 | Byte(color.X);
    }

    public static void Line(Vec2 from, Vec2 to, Vec4 color, float thickness = 1f)
    {
      unsafe { NativeDrawLine(from.X, from.Y, to.X, to.Y, Pack(color), thickness); }
    }

    public static void Rect(Vec2 min, Vec2 max, Vec4 color, float rounding = 0f, float thickness = 1f)
    {
      unsafe { NativeDrawRect(min.X, min.Y, max.X, max.Y, Pack(color), rounding, thickness); }
    }

    public static void RectFilled(Vec2 min, Vec2 max, Vec4 color, float rounding = 0f)
    {
      unsafe { NativeDrawRectFilled(min.X, min.Y, max.X, max.Y, Pack(color), rounding); }
    }

    /// 0 segments lets ImGui pick how many the radius needs
    public static void Circle(Vec2 center, float radius, Vec4 color, int segments = 0, float thickness = 1f)
    {
      unsafe { NativeDrawCircle(center.X, center.Y, radius, Pack(color), segments, thickness); }
    }

    public static void CircleFilled(Vec2 center, float radius, Vec4 color, int segments = 0)
    {
      unsafe { NativeDrawCircleFilled(center.X, center.Y, radius, Pack(color), segments); }
    }

    public static void Triangle(Vec2 a, Vec2 b, Vec2 c, Vec4 color, float thickness = 1f)
    {
      unsafe { NativeDrawTriangle(a.X, a.Y, b.X, b.Y, c.X, c.Y, Pack(color), thickness); }
    }

    public static void TriangleFilled(Vec2 a, Vec2 b, Vec2 c, Vec4 color)
    {
      unsafe { NativeDrawTriangleFilled(a.X, a.Y, b.X, b.Y, c.X, c.Y, Pack(color)); }
    }

    /// a curve from the first point to the last, pulled towards the two between
    public static void BezierCubic(Vec2 from, Vec2 control_a, Vec2 control_b, Vec2 to, Vec4 color, float thickness = 1f, int segments = 0)
    {
      unsafe { NativeDrawBezierCubic(from.X, from.Y, control_a.X, control_a.Y, control_b.X, control_b.Y, to.X, to.Y, Pack(color), thickness, segments); }
    }

    public static void Text(Vec2 position, Vec4 color, string text)
    {
      unsafe
      {
        using var text_utf8 = new Utf8Arg(text, stackalloc byte[Utf8Arg.StackSize]);
        NativeDrawText(position.X, position.Y, Pack(color), text_utf8.Pointer);
      }
    }

    /// the texture stretched over the rectangle, the right way up
    public static void Image(UITexture texture, Vec2 min, Vec2 max)
    {
      Image(texture, min, max, UITexture.kTopLeft, UITexture.kBottomRight, kWhite);
    }

    /// the part of the texture between the two corners, multiplied by the tint
    public static void Image(UITexture texture, Vec2 min, Vec2 max, Vec2 uv_min, Vec2 uv_max, Vec4 tint)
    {
      unsafe { NativeDrawImage(texture.Id, min.X, min.Y, max.X, max.Y, uv_min.X, uv_min.Y, uv_max.X, uv_max.Y, Pack(tint)); }
    }

    /// what is drawn until PopClipRect is cut to the rectangle: to that rectangle alone, or
    ///  to where it and what the window already shows overlap
    public static void PushClipRect(Vec2 min, Vec2 max, bool intersect_with_current = true)
    {
      unsafe { NativePushClipRect(min.X, min.Y, max.X, max.Y, intersect_with_current); }
    }

    public static void PopClipRect() { unsafe { NativePopClipRect(); } }
  }
}
