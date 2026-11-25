using System;
using System.Diagnostics.CodeAnalysis;
using System.Globalization;
using System.Runtime.InteropServices;

namespace Other
{
  [StructLayout(LayoutKind.Sequential)]
  public struct Vec2 : IEquatable<Vec2>
  {
    public float X;
    public float Y;

    public Vec2(float x, float y)
    {
      X = x;
      Y = y;
    }

    /// 0 is X and 1 is Y
    public float this[int index]
    {
      get
      {
        switch (index)
        {
          case 0: return X;
          case 1: return Y;
          default: throw new IndexOutOfRangeException($"a Vec2 has no component {index}");
        }
      }
      set
      {
        switch (index)
        {
          case 0: X = value; break;
          case 1: Y = value; break;
          default: throw new IndexOutOfRangeException($"a Vec2 has no component {index}");
        }
      }
    }

    public static Vec2 operator+(Vec2 left, Vec2 right) => new Vec2(left.X + right.X, left.Y + right.Y);
    public static Vec2 operator-(Vec2 left, Vec2 right) => new Vec2(left.X - right.X, left.Y - right.Y);
    public static Vec2 operator*(Vec2 left, Vec2 right) => new Vec2(left.X * right.X, left.Y * right.Y);
    public static Vec2 operator/(Vec2 left, Vec2 right) => new Vec2(left.X / right.X, left.Y / right.Y);
    public static Vec2 operator+(Vec2 left, float right) => new Vec2(left.X + right, left.Y + right);
    public static Vec2 operator-(Vec2 left, float right) => new Vec2(left.X - right, left.Y - right);
    public static Vec2 operator*(Vec2 left, float right) => new Vec2(left.X * right, left.Y * right);
    public static Vec2 operator/(Vec2 left, float right) => new Vec2(left.X / right, left.Y / right);
    public static Vec2 operator*(float left, Vec2 right) => new Vec2(left * right.X, left * right.Y);
    public static Vec2 operator-(Vec2 value) => new Vec2(-value.X, -value.Y);

    public static bool operator==(Vec2 left, Vec2 right) => left.X == right.X && left.Y == right.Y;
    public static bool operator!=(Vec2 left, Vec2 right) => !(left == right);
    public bool Equals(Vec2 other) => this == other;
    public override bool Equals([NotNullWhen(true)] object obj)
    {
      if (obj is Vec2 other)
      {
        return this == other;
      }
      return false;
    }
    public override int GetHashCode() => HashCode.Combine(X, Y);
    public override string ToString() => string.Create(CultureInfo.InvariantCulture, $"({X}, {Y})");

    public void Deconstruct(out float x, out float y)
    {
      x = X;
      y = Y;
    }

    public static float Dot(Vec2 a, Vec2 b) => a.X * b.X + a.Y * b.Y;
    /// the Z of the cross product of the two as if they lay flat in 3D: positive when b
    ///  is counter-clockwise of a
    public static float Cross(Vec2 a, Vec2 b) => a.X * b.Y - a.Y * b.X;

    public float LengthSquared() => X * X + Y * Y;
    public float Length() => MathF.Sqrt(LengthSquared());
    public Vec2 Normalized()
    {
      float len = Length();
      return len <= float.Epsilon ? Zero : this / len;
    }

    public static float Distance(Vec2 a, Vec2 b) => (b - a).Length();
    public static float DistanceSquared(Vec2 a, Vec2 b) => (b - a).LengthSquared();

    /// from a at t = 0 to b at t = 1, t held between them
    public static Vec2 Lerp(Vec2 a, Vec2 b, float t) => a + (b - a) * Mathf.Clamp01(t);
    public static Vec2 LerpUnclamped(Vec2 a, Vec2 b, float t) => a + (b - a) * t;

    /// current, moved towards target by no more than max_delta and never past it
    public static Vec2 MoveTowards(Vec2 current, Vec2 target, float max_delta)
    {
      Vec2 to = target - current;
      float distance = to.Length();
      if (distance <= max_delta || distance <= float.Epsilon)
      {
        return target;
      }
      return current + to / distance * max_delta;
    }

    public static Vec2 Min(Vec2 a, Vec2 b) => new Vec2(MathF.Min(a.X, b.X), MathF.Min(a.Y, b.Y));
    public static Vec2 Max(Vec2 a, Vec2 b) => new Vec2(MathF.Max(a.X, b.X), MathF.Max(a.Y, b.Y));
    public static Vec2 Abs(Vec2 value) => new Vec2(MathF.Abs(value.X), MathF.Abs(value.Y));
    /// each component held between the same component of min and of max
    public static Vec2 Clamp(Vec2 value, Vec2 min, Vec2 max) => Min(Max(value, min), max);

    /// the same vector, shortened to max_length if it is longer
    public static Vec2 ClampLength(Vec2 value, float max_length)
    {
      if (value.LengthSquared() <= max_length * max_length)
      {
        return value;
      }
      return value.Normalized() * max_length;
    }

    /// direction, bounced off a surface that faces along normal. normal is of length one
    public static Vec2 Reflect(Vec2 direction, Vec2 normal) => direction - normal * (2f * Dot(direction, normal));

    /// the part of value that lies along onto
    public static Vec2 Project(Vec2 value, Vec2 onto)
    {
      float length_squared = onto.LengthSquared();
      return length_squared <= float.Epsilon ? Zero : onto * (Dot(value, onto) / length_squared);
    }

    /// value turned a quarter turn counter-clockwise
    public static Vec2 Perpendicular(Vec2 value) => new Vec2(-value.Y, value.X);

    /// the angle between two directions in degrees, from 0 to 180
    public static float Angle(Vec2 from, Vec2 to)
    {
      float lengths = MathF.Sqrt(from.LengthSquared() * to.LengthSquared());
      if (lengths <= float.Epsilon)
      {
        return 0f;
      }
      return MathF.Acos(Mathf.Clamp(Dot(from, to) / lengths, -1f, 1f)) * Mathf.kRadToDeg;
    }

    /// the angle from one direction to another in degrees, from -180 to 180: positive
    ///  when to is counter-clockwise of from
    public static float SignedAngle(Vec2 from, Vec2 to) => Angle(from, to) * (Cross(from, to) < 0f ? -1f : 1f);

    public static bool Approximately(Vec2 a, Vec2 b, float tolerance = Mathf.kEpsilon)
    {
      return Mathf.Approximately(a.X, b.X, tolerance) && Mathf.Approximately(a.Y, b.Y, tolerance);
    }

    public static Vec2 Zero => new Vec2(0f, 0f);
    public static Vec2 One => new Vec2(1f, 1f);
    public static Vec2 UnitX => new Vec2(1f, 0f);
    public static Vec2 UnitY => new Vec2(0f, 1f);
    public static Vec2 Up => new Vec2(0f, 1f);
    public static Vec2 Down => new Vec2(0f, -1f);
    public static Vec2 Left => new Vec2(-1f, 0f);
    public static Vec2 Right => new Vec2(1f, 0f);
  }
}
