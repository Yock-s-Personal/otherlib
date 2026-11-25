using System;
using System.Diagnostics.CodeAnalysis;
using System.Globalization;
using System.Runtime.InteropServices;

namespace Other
{
  [StructLayout(LayoutKind.Sequential)]
  public struct Vec3 : IEquatable<Vec3>
  {
    public float X;
    public float Y;
    public float Z;

    public Vec3(float x, float y, float z)
    {
      X = x;
      Y = y;
      Z = z;
    }

    public Vec3(Vec2 xy, float z)
    {
      X = xy.X;
      Y = xy.Y;
      Z = z;
    }

    /// 0 is X, 1 is Y and 2 is Z
    public float this[int index]
    {
      get
      {
        switch (index)
        {
          case 0: return X;
          case 1: return Y;
          case 2: return Z;
          default: throw new IndexOutOfRangeException($"a Vec3 has no component {index}");
        }
      }
      set
      {
        switch (index)
        {
          case 0: X = value; break;
          case 1: Y = value; break;
          case 2: Z = value; break;
          default: throw new IndexOutOfRangeException($"a Vec3 has no component {index}");
        }
      }
    }

    public Vec2 XY => new Vec2(X, Y);

    public static Vec3 operator+(Vec3 left, Vec3 right) => new Vec3(left.X + right.X, left.Y + right.Y, left.Z + right.Z);
    public static Vec3 operator-(Vec3 left, Vec3 right) => new Vec3(left.X - right.X, left.Y - right.Y, left.Z - right.Z);
    public static Vec3 operator*(Vec3 left, Vec3 right) => new Vec3(left.X * right.X, left.Y * right.Y, left.Z * right.Z);
    public static Vec3 operator/(Vec3 left, Vec3 right) => new Vec3(left.X / right.X, left.Y / right.Y, left.Z / right.Z);
    public static Vec3 operator+(Vec3 left, float right) => new Vec3(left.X + right, left.Y + right, left.Z + right);
    public static Vec3 operator-(Vec3 left, float right) => new Vec3(left.X - right, left.Y - right, left.Z - right);
    public static Vec3 operator*(Vec3 left, float right) => new Vec3(left.X * right, left.Y * right, left.Z * right);
    public static Vec3 operator/(Vec3 left, float right) => new Vec3(left.X / right, left.Y / right, left.Z / right);
    public static Vec3 operator*(float left, Vec3 right) => new Vec3(left * right.X, left * right.Y, left * right.Z);
    public static Vec3 operator-(Vec3 value) => new Vec3(-value.X, -value.Y, -value.Z);

    public static bool operator==(Vec3 left, Vec3 right) => left.X == right.X && left.Y == right.Y && left.Z == right.Z;
    public static bool operator!=(Vec3 left, Vec3 right) => !(left == right);
    public bool Equals(Vec3 other) => this == other;
    public override bool Equals([NotNullWhen(true)] object obj)
    {
      if (obj is Vec3 other)
      {
        return this == other;
      }
      return false;
    }
    public override int GetHashCode() => HashCode.Combine(X, Y, Z);
    public override string ToString() => string.Create(CultureInfo.InvariantCulture, $"({X}, {Y}, {Z})");

    public void Deconstruct(out float x, out float y, out float z)
    {
      x = X;
      y = Y;
      z = Z;
    }

    public static float Dot(Vec3 a, Vec3 b) => a.X * b.X + a.Y * b.Y + a.Z * b.Z;
    public static Vec3 Cross(Vec3 a, Vec3 b) => new Vec3(
      a.Y * b.Z - a.Z * b.Y,
      a.Z * b.X - a.X * b.Z,
      a.X * b.Y - a.Y * b.X
    );

    public float LengthSquared() => X * X + Y * Y + Z * Z;
    public float Length() => MathF.Sqrt(LengthSquared());
    public Vec3 Normalized()
    {
      float len = Length();
      return len <= float.Epsilon ? Zero : this / len;
    }

    public static float Distance(Vec3 a, Vec3 b) => (b - a).Length();
    public static float DistanceSquared(Vec3 a, Vec3 b) => (b - a).LengthSquared();

    /// from a at t = 0 to b at t = 1, t held between them
    public static Vec3 Lerp(Vec3 a, Vec3 b, float t) => a + (b - a) * Mathf.Clamp01(t);
    public static Vec3 LerpUnclamped(Vec3 a, Vec3 b, float t) => a + (b - a) * t;

    /// current, moved towards target by no more than max_delta and never past it
    public static Vec3 MoveTowards(Vec3 current, Vec3 target, float max_delta)
    {
      Vec3 to = target - current;
      float distance = to.Length();
      if (distance <= max_delta || distance <= float.Epsilon)
      {
        return target;
      }
      return current + to / distance * max_delta;
    }

    public static Vec3 Min(Vec3 a, Vec3 b) => new Vec3(MathF.Min(a.X, b.X), MathF.Min(a.Y, b.Y), MathF.Min(a.Z, b.Z));
    public static Vec3 Max(Vec3 a, Vec3 b) => new Vec3(MathF.Max(a.X, b.X), MathF.Max(a.Y, b.Y), MathF.Max(a.Z, b.Z));
    public static Vec3 Abs(Vec3 value) => new Vec3(MathF.Abs(value.X), MathF.Abs(value.Y), MathF.Abs(value.Z));
    /// each component held between the same component of min and of max
    public static Vec3 Clamp(Vec3 value, Vec3 min, Vec3 max) => Min(Max(value, min), max);

    /// the same vector, shortened to max_length if it is longer
    public static Vec3 ClampLength(Vec3 value, float max_length)
    {
      if (value.LengthSquared() <= max_length * max_length)
      {
        return value;
      }
      return value.Normalized() * max_length;
    }

    /// direction, bounced off a surface that faces along normal. normal is of length one
    public static Vec3 Reflect(Vec3 direction, Vec3 normal) => direction - normal * (2f * Dot(direction, normal));

    /// the part of value that lies along onto
    public static Vec3 Project(Vec3 value, Vec3 onto)
    {
      float length_squared = onto.LengthSquared();
      return length_squared <= float.Epsilon ? Zero : onto * (Dot(value, onto) / length_squared);
    }

    /// the part of value that lies in the plane normal stands on
    public static Vec3 ProjectOnPlane(Vec3 value, Vec3 normal) => value - Project(value, normal);

    /// the angle between two directions in degrees, from 0 to 180
    public static float Angle(Vec3 from, Vec3 to)
    {
      float lengths = MathF.Sqrt(from.LengthSquared() * to.LengthSquared());
      if (lengths <= float.Epsilon)
      {
        return 0f;
      }
      return MathF.Acos(Mathf.Clamp(Dot(from, to) / lengths, -1f, 1f)) * Mathf.kRadToDeg;
    }

    /// the angle from one direction to another in degrees, from -180 to 180: positive
    ///  when the turn is counter-clockwise seen from where axis points
    public static float SignedAngle(Vec3 from, Vec3 to, Vec3 axis) => Angle(from, to) * (Dot(axis, Cross(from, to)) < 0f ? -1f : 1f);

    public static bool Approximately(Vec3 a, Vec3 b, float tolerance = Mathf.kEpsilon)
    {
      return Mathf.Approximately(a.X, b.X, tolerance) && Mathf.Approximately(a.Y, b.Y, tolerance) && Mathf.Approximately(a.Z, b.Z, tolerance);
    }

    public static Vec3 Zero => new Vec3(0f, 0f, 0f);
    public static Vec3 One => new Vec3(1f, 1f, 1f);
    /// the axes by name. they cannot be Vec3.X, Y and Z: those are the components
    public static Vec3 UnitX => new Vec3(1f, 0f, 0f);
    public static Vec3 UnitY => new Vec3(0f, 1f, 0f);
    public static Vec3 UnitZ => new Vec3(0f, 0f, 1f);
    public static Vec3 Up => new Vec3(0f, 1f, 0f);
    public static Vec3 Down => new Vec3(0f, -1f, 0f);
    public static Vec3 Left => new Vec3(-1f, 0f, 0f);
    public static Vec3 Right => new Vec3(1f, 0f, 0f);
    public static Vec3 Forward => new Vec3(0f, 0f, 1f);
    public static Vec3 Backward => new Vec3(0f, 0f, -1f);
  }
}
