using System;
using System.Diagnostics.CodeAnalysis;
using System.Globalization;
using System.Runtime.InteropServices;

namespace Other
{
  [StructLayout(LayoutKind.Sequential)]
  public struct Quaternion : IEquatable<Quaternion>
  {
    public float X;
    public float Y;
    public float Z;
    public float W;

    public Quaternion(float x, float y, float z, float w)
    {
      X = x;
      Y = y;
      Z = z;
      W = w;
    }

    /// the angles FromEuler makes this rotation of, in degrees: X is the pitch, from -90
    ///  to 90, Y the yaw and Z the roll, each from -180 to 180. looking straight up or
    ///  down, yaw and roll are one turn and it is all given as yaw. the engine's
    ///  euler_from_quat is the same sum, and the Inspector shows what it gives
    public Vec3 ToEulerAngles()
    {
      float length = Length();
      if (length <= 0f)
      {
        return Vec3.Zero;
      }

      /// read off where the rotation sends the three axes
      Quaternion unit = this / length;
      Vec3 right = unit * Vec3.UnitX;
      Vec3 up = unit * Vec3.UnitY;
      Vec3 forward = unit * Vec3.UnitZ;

      float pitch = MathF.Asin(Mathf.Clamp(-forward.Y, -1f, 1f));
      if (MathF.Abs(forward.Y) < 0.999999f)
      {
        return new Vec3(pitch, MathF.Atan2(forward.X, forward.Z), MathF.Atan2(right.Y, up.Y)) * Mathf.kRadToDeg;
      }
      /// straight up or down: forward has no heading left to read the yaw from
      return new Vec3(pitch, MathF.Atan2(-right.Z, right.X), 0f) * Mathf.kRadToDeg;
    }

    public static readonly Quaternion Identity = new Quaternion(0f, 0f, 0f, 1f);

    public static Quaternion operator+(Quaternion left, Quaternion right) => new Quaternion(left.X + right.X, left.Y + right.Y, left.Z + right.Z, left.W + right.W);
    public static Quaternion operator-(Quaternion left, Quaternion right) => new Quaternion(left.X - right.X, left.Y - right.Y, left.Z - right.Z, left.W - right.W);
    /// Hamilton product: rotation by right, then by left
    public static Quaternion operator*(Quaternion left, Quaternion right) => new Quaternion(
      left.W * right.X + left.X * right.W + left.Y * right.Z - left.Z * right.Y,
      left.W * right.Y - left.X * right.Z + left.Y * right.W + left.Z * right.X,
      left.W * right.Z + left.X * right.Y - left.Y * right.X + left.Z * right.W,
      left.W * right.W - left.X * right.X - left.Y * right.Y - left.Z * right.Z
    );
    public static Quaternion operator/(Quaternion left, Quaternion right) => left * right.Inverse();
    public static Quaternion operator*(Quaternion left, float right) => new Quaternion(left.X * right, left.Y * right, left.Z * right, left.W * right);
    public static Quaternion operator/(Quaternion left, float right) => new Quaternion(left.X / right, left.Y / right, left.Z / right, left.W / right);
    /// the same rotation, written the other of the two ways it can be
    public static Quaternion operator-(Quaternion value) => new Quaternion(-value.X, -value.Y, -value.Z, -value.W);

    /// rotates a vector by this quaternion (assumes unit length)
    public static Vec3 operator*(Quaternion q, Vec3 v)
    {
      // v' = v + 2w(u x v) + 2(u x (u x v)), u = (X, Y, Z)
      Vec3 u = new Vec3(q.X, q.Y, q.Z);
      Vec3 uxv = Vec3.Cross(u, v);
      Vec3 uxuxv = Vec3.Cross(u, uxv);
      return v + (uxv * (2.0f * q.W)) + (uxuxv * 2.0f);
    }

    public float LengthSquared() => X * X + Y * Y + Z * Z + W * W;
    public float Length() => MathF.Sqrt(LengthSquared());

    public Quaternion Conjugate() => new Quaternion(-X, -Y, -Z, W);

    public Quaternion Inverse()
    {
      float len_sq = LengthSquared();
      if (len_sq <= float.Epsilon)
      {
        return Identity;
      }
      Quaternion conj = Conjugate();
      return conj / len_sq;
    }

    public Quaternion Normalized()
    {
      float len = Length();
      if (len <= float.Epsilon)
      {
        return Identity;
      }
      return this / len;
    }

    public static float Dot(Quaternion a, Quaternion b) => a.X * b.X + a.Y * b.Y + a.Z * b.Z + a.W * b.W;

    public static bool operator==(Quaternion left, Quaternion right) => left.X == right.X && left.Y == right.Y && left.Z == right.Z && left.W == right.W;
    public static bool operator!=(Quaternion left, Quaternion right) => !(left == right);
    public bool Equals(Quaternion other) => this == other;
    public override bool Equals([NotNullWhen(true)] object obj)
    {
      if (obj is Quaternion other)
      {
        return this == other;
      }
      return false;
    }
    public override int GetHashCode() => HashCode.Combine(X, Y, Z, W);
    public override string ToString() => string.Create(CultureInfo.InvariantCulture, $"({X}, {Y}, {Z}, {W})");

    /// the same rotation, near enough: a quaternion and its negative are one rotation,
    ///  which comparing them part by part would call two
    public static bool Approximately(Quaternion a, Quaternion b, float tolerance = Mathf.kEpsilon)
    {
      return MathF.Abs(Dot(a, b)) >= 1f - tolerance;
    }

    /// a turn of so many radians round an axis, counter-clockwise seen from where the axis
    ///  points. the axis need not be of length one
    public static Quaternion FromAxisAngle(Vec3 axis, float radians)
    {
      Vec3 unit = axis.Normalized();
      float half = radians * 0.5f;
      float s = MathF.Sin(half);
      return new Quaternion(unit.X * s, unit.Y * s, unit.Z * s, MathF.Cos(half));
    }

    /// the axis and the radians FromAxisAngle would make this rotation of, the turn from
    ///  0 to pi. a rotation that turns nothing is given the X axis
    public void ToAxisAngle(out Vec3 axis, out float radians)
    {
      Quaternion unit = Normalized();
      if (unit.W < 0f)
      {
        unit = -unit;
      }
      Vec3 along = new Vec3(unit.X, unit.Y, unit.Z);
      float sine = along.Length();
      radians = 2f * MathF.Atan2(sine, unit.W);
      axis = sine <= 1e-6f ? Vec3.UnitX : along / sine;
    }

    /// how far apart two rotations are in degrees, from 0 to 180
    public static float Angle(Quaternion a, Quaternion b)
    {
      float cosine = MathF.Min(MathF.Abs(Dot(a.Normalized(), b.Normalized())), 1f);
      return 2f * MathF.Acos(cosine) * Mathf.kRadToDeg;
    }

    /// from a at t = 0 to b at t = 1 the shortest way, t held between them. cheaper than
    ///  Slerp, and not at an even speed when the two are far apart
    public static Quaternion Lerp(Quaternion a, Quaternion b, float t)
    {
      t = Mathf.Clamp01(t);
      if (Dot(a, b) < 0f)
      {
        b = -b;
      }
      return (a * (1f - t) + b * t).Normalized();
    }

    /// from a at t = 0 to b at t = 1 the shortest way and at an even speed, t held
    ///  between them
    public static Quaternion Slerp(Quaternion a, Quaternion b, float t)
    {
      t = Mathf.Clamp01(t);
      float cosine = Dot(a, b);
      if (cosine < 0f)
      {
        b = -b;
        cosine = -cosine;
      }
      /// too near each other to divide by the sine of what is between them
      if (cosine > 0.9995f)
      {
        return (a * (1f - t) + b * t).Normalized();
      }
      float between = MathF.Acos(cosine);
      float sine = MathF.Sin(between);
      return (a * (MathF.Sin((1f - t) * between) / sine) + b * (MathF.Sin(t * between) / sine)).Normalized();
    }

    /// from, turned towards to by no more than max_degrees and never past it
    public static Quaternion RotateTowards(Quaternion from, Quaternion to, float max_degrees)
    {
      float apart = Angle(from, to);
      if (apart <= max_degrees || apart <= 0f)
      {
        return to;
      }
      return Slerp(from, to, max_degrees / apart);
    }

    /// the rotation that turns Vec3.Forward to point along forward, with Vec3.Up as near
    ///  to up as that leaves it. neither need be of length one
    public static Quaternion LookRotation(Vec3 forward, Vec3 up)
    {
      Vec3 f = forward.Normalized();
      if (f.LengthSquared() <= 0f)
      {
        return Identity;
      }
      Vec3 r = Vec3.Cross(up, f);
      if (r.LengthSquared() <= 1e-10f)
      {
        /// up is along forward and says nothing: any up that is not will do
        r = Vec3.Cross(MathF.Abs(f.Y) < 0.99f ? Vec3.UnitY : Vec3.UnitX, f);
      }
      r = r.Normalized();
      Vec3 u = Vec3.Cross(f, r);

      /// the rotation whose matrix has r, u and f as its columns
      float trace = r.X + u.Y + f.Z;
      if (trace > 0f)
      {
        float s = MathF.Sqrt(trace + 1f) * 2f;
        return new Quaternion((u.Z - f.Y) / s, (f.X - r.Z) / s, (r.Y - u.X) / s, s * 0.25f);
      }
      if (r.X > u.Y && r.X > f.Z)
      {
        float s = MathF.Sqrt(1f + r.X - u.Y - f.Z) * 2f;
        return new Quaternion(s * 0.25f, (u.X + r.Y) / s, (f.X + r.Z) / s, (u.Z - f.Y) / s);
      }
      if (u.Y > f.Z)
      {
        float s = MathF.Sqrt(1f + u.Y - r.X - f.Z) * 2f;
        return new Quaternion((u.X + r.Y) / s, s * 0.25f, (f.Y + u.Z) / s, (f.X - r.Z) / s);
      }
      else
      {
        float s = MathF.Sqrt(1f + f.Z - r.X - u.Y) * 2f;
        return new Quaternion((f.X + r.Z) / s, (f.Y + u.Z) / s, s * 0.25f, (r.Y - u.X) / s);
      }
    }

    public static Quaternion LookRotation(Vec3 forward) => LookRotation(forward, Vec3.Up);

    /// the shortest rotation that turns the direction from to the direction to. neither
    ///  need be of length one
    public static Quaternion FromToRotation(Vec3 from, Vec3 to)
    {
      Vec3 a = from.Normalized();
      Vec3 b = to.Normalized();
      float cosine = Vec3.Dot(a, b);
      if (cosine >= 1f - 1e-6f)
      {
        return Identity;
      }
      if (cosine <= -1f + 1e-6f)
      {
        /// straight back: half a turn round anything that stands across from
        Vec3 across = Vec3.Cross(Vec3.UnitX, a);
        if (across.LengthSquared() <= 1e-6f)
        {
          across = Vec3.Cross(Vec3.UnitY, a);
        }
        return FromAxisAngle(across, MathF.PI);
      }
      Vec3 axis = Vec3.Cross(a, b);
      return new Quaternion(axis.X, axis.Y, axis.Z, 1f + cosine).Normalized();
    }

    /// degrees round X (pitch), round Y (yaw) and round Z (roll). the roll is made first,
    ///  then the pitch, then the yaw: so yaw turns round the world's up whatever the pitch
    ///  and runs the whole circle, and pitch is the one that stops at 90 either way
    public static Quaternion FromEuler(Vec3 degrees)
    {
      return CreateFromYawPitchRoll(degrees.Y * Mathf.kDegToRad, degrees.X * Mathf.kDegToRad, degrees.Z * Mathf.kDegToRad);
    }

    public static Quaternion FromEuler(float pitch, float yaw, float roll) => FromEuler(new Vec3(pitch, yaw, roll));

    /// as FromEuler, in radians: yaw round Y, pitch round X, roll round Z
    public static Quaternion CreateFromYawPitchRoll(float yaw, float pitch, float roll)
    {
      float cy = MathF.Cos(yaw * 0.5f);
      float sy = MathF.Sin(yaw * 0.5f);
      float cp = MathF.Cos(pitch * 0.5f);
      float sp = MathF.Sin(pitch * 0.5f);
      float cr = MathF.Cos(roll * 0.5f);
      float sr = MathF.Sin(roll * 0.5f);
      /// the product yaw * pitch * roll, written out
      return new Quaternion(
        cy * sp * cr + sy * cp * sr,
        sy * cp * cr - cy * sp * sr,
        cy * cp * sr - sy * sp * cr,
        cy * cp * cr + sy * sp * sr
      );
    }
  }
}