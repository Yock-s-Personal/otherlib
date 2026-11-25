using System;
using System.Diagnostics.CodeAnalysis;
using System.Globalization;
using System.Runtime.InteropServices;

namespace Other
{
  [StructLayout(LayoutKind.Sequential)]
  public struct Vec4 : IEquatable<Vec4>
  {
    public float X;
    public float Y;
    public float Z;
    public float W;

    public Vec4(float x, float y, float z, float w)
    {
      X = x;
      Y = y;
      Z = z;
      W = w;
    }

    public Vec4(Vec3 xyz, float w)
    {
      X = xyz.X;
      Y = xyz.Y;
      Z = xyz.Z;
      W = w;
    }

    /// 0 is X, 1 is Y, 2 is Z and 3 is W
    public float this[int index]
    {
      get
      {
        switch (index)
        {
          case 0: return X;
          case 1: return Y;
          case 2: return Z;
          case 3: return W;
          default: throw new IndexOutOfRangeException($"a Vec4 has no component {index}");
        }
      }
      set
      {
        switch (index)
        {
          case 0: X = value; break;
          case 1: Y = value; break;
          case 2: Z = value; break;
          case 3: W = value; break;
          default: throw new IndexOutOfRangeException($"a Vec4 has no component {index}");
        }
      }
    }

    public Vec2 XY => new Vec2(X, Y);
    public Vec3 XYZ => new Vec3(X, Y, Z);

    public static Vec4 operator+(Vec4 left, Vec4 right) => new Vec4(left.X + right.X, left.Y + right.Y, left.Z + right.Z, left.W + right.W);
    public static Vec4 operator-(Vec4 left, Vec4 right) => new Vec4(left.X - right.X, left.Y - right.Y, left.Z - right.Z, left.W - right.W);
    public static Vec4 operator*(Vec4 left, Vec4 right) => new Vec4(left.X * right.X, left.Y * right.Y, left.Z * right.Z, left.W * right.W);
    public static Vec4 operator/(Vec4 left, Vec4 right) => new Vec4(left.X / right.X, left.Y / right.Y, left.Z / right.Z, left.W / right.W);
    public static Vec4 operator+(Vec4 left, float right) => new Vec4(left.X + right, left.Y + right, left.Z + right, left.W + right);
    public static Vec4 operator-(Vec4 left, float right) => new Vec4(left.X - right, left.Y - right, left.Z - right, left.W - right);
    public static Vec4 operator*(Vec4 left, float right) => new Vec4(left.X * right, left.Y * right, left.Z * right, left.W * right);
    public static Vec4 operator/(Vec4 left, float right) => new Vec4(left.X / right, left.Y / right, left.Z / right, left.W / right);
    public static Vec4 operator*(float left, Vec4 right) => new Vec4(left * right.X, left * right.Y, left * right.Z, left * right.W);
    public static Vec4 operator-(Vec4 value) => new Vec4(-value.X, -value.Y, -value.Z, -value.W);

    public static bool operator==(Vec4 left, Vec4 right) => left.X == right.X && left.Y == right.Y && left.Z == right.Z && left.W == right.W;
    public static bool operator!=(Vec4 left, Vec4 right) => !(left == right);
    public bool Equals(Vec4 other) => this == other;
    public override bool Equals([NotNullWhen(true)] object obj)
    {
      if (obj is Vec4 other)
      {
        return this == other;
      }
      return false;
    }
    public override int GetHashCode() => HashCode.Combine(X, Y, Z, W);
    public override string ToString() => string.Create(CultureInfo.InvariantCulture, $"({X}, {Y}, {Z}, {W})");

    public void Deconstruct(out float x, out float y, out float z, out float w)
    {
      x = X;
      y = Y;
      z = Z;
      w = W;
    }

    public static float Dot(Vec4 a, Vec4 b) => a.X * b.X + a.Y * b.Y + a.Z * b.Z + a.W * b.W;

    public float LengthSquared() => X * X + Y * Y + Z * Z + W * W;
    public float Length() => MathF.Sqrt(LengthSquared());
    public Vec4 Normalized()
    {
      float len = Length();
      return len <= float.Epsilon ? Zero : this / len;
    }

    public static float Distance(Vec4 a, Vec4 b) => (b - a).Length();
    public static float DistanceSquared(Vec4 a, Vec4 b) => (b - a).LengthSquared();

    /// from a at t = 0 to b at t = 1, t held between them
    public static Vec4 Lerp(Vec4 a, Vec4 b, float t) => a + (b - a) * Mathf.Clamp01(t);
    public static Vec4 LerpUnclamped(Vec4 a, Vec4 b, float t) => a + (b - a) * t;

    public static Vec4 Min(Vec4 a, Vec4 b) => new Vec4(MathF.Min(a.X, b.X), MathF.Min(a.Y, b.Y), MathF.Min(a.Z, b.Z), MathF.Min(a.W, b.W));
    public static Vec4 Max(Vec4 a, Vec4 b) => new Vec4(MathF.Max(a.X, b.X), MathF.Max(a.Y, b.Y), MathF.Max(a.Z, b.Z), MathF.Max(a.W, b.W));
    public static Vec4 Abs(Vec4 value) => new Vec4(MathF.Abs(value.X), MathF.Abs(value.Y), MathF.Abs(value.Z), MathF.Abs(value.W));
    /// each component held between the same component of min and of max
    public static Vec4 Clamp(Vec4 value, Vec4 min, Vec4 max) => Min(Max(value, min), max);

    public static bool Approximately(Vec4 a, Vec4 b, float tolerance = Mathf.kEpsilon)
    {
      return Mathf.Approximately(a.X, b.X, tolerance) && Mathf.Approximately(a.Y, b.Y, tolerance) && Mathf.Approximately(a.Z, b.Z, tolerance) && Mathf.Approximately(a.W, b.W, tolerance);
    }

    public static Vec4 Zero => new Vec4(0f, 0f, 0f, 0f);
    public static Vec4 One => new Vec4(1f, 1f, 1f, 1f);
    public static Vec4 UnitX => new Vec4(1f, 0f, 0f, 0f);
    public static Vec4 UnitY => new Vec4(0f, 1f, 0f, 0f);
    public static Vec4 UnitZ => new Vec4(0f, 0f, 1f, 0f);
    public static Vec4 UnitW => new Vec4(0f, 0f, 0f, 1f);
  }
}
