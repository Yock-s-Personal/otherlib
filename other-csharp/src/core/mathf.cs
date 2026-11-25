using System;

namespace Other
{
  /// the float and double utilities a script reaches for. named Mathf so that it does not
  ///  hide System.Math. the trigonometry is in radians; a function with Angle in its name
  ///  is in degrees
  public static class Mathf
  {
    public const float kPi = MathF.PI;
    public const float kTau = MathF.PI * 2f;
    public const float kE = MathF.E;
    public const float kDegToRad = MathF.PI / 180f;
    public const float kRadToDeg = 180f / MathF.PI;
    public const float kInfinity = float.PositiveInfinity;
    /// how near two floats of about one are for Approximately to call them the same
    public const float kEpsilon = 1e-6f;
    /// the same for doubles
    public const double kEpsilonDouble = 1e-12;

    public static float Abs(float value) => MathF.Abs(value);
    /// -1, 0 or 1. zero for what is not a number, where System.MathF.Sign throws
    public static float Sign(float value) => value > 0f ? 1f : (value < 0f ? -1f : 0f);
    public static float Min(float a, float b) => a < b ? a : b;
    public static float Max(float a, float b) => a > b ? a : b;

    public static float Min(params float[] values)
    {
      if (values.Length == 0)
      {
        return 0f;
      }
      float least = values[0];
      for (int i = 1; i < values.Length; i++)
      {
        least = Min(least, values[i]);
      }
      return least;
    }

    public static float Max(params float[] values)
    {
      if (values.Length == 0)
      {
        return 0f;
      }
      float most = values[0];
      for (int i = 1; i < values.Length; i++)
      {
        most = Max(most, values[i]);
      }
      return most;
    }

    public static float Clamp(float value, float min, float max)
    {
      if (value < min)
      {
        return min;
      }
      if (value > max)
      {
        return max;
      }
      return value;
    }

    public static float Clamp01(float value) => Clamp(value, 0f, 1f);

    public static float Floor(float value) => MathF.Floor(value);
    public static float Ceil(float value) => MathF.Ceiling(value);
    /// to the nearest whole number, a half going away from zero: 2.5 to 3 and -2.5 to -3
    public static float Round(float value) => MathF.Round(value, MidpointRounding.AwayFromZero);
    /// the whole part, towards zero
    public static float Trunc(float value) => MathF.Truncate(value);
    /// what is left above the floor, never negative: 0.25 for 1.25 and 0.75 for -1.25
    public static float Frac(float value) => value - MathF.Floor(value);
    public static int FloorToInt(float value) => (int)MathF.Floor(value);
    public static int CeilToInt(float value) => (int)MathF.Ceiling(value);
    public static int RoundToInt(float value) => (int)Round(value);

    public static float Sqrt(float value) => MathF.Sqrt(value);
    public static float Pow(float value, float power) => MathF.Pow(value, power);
    public static float Exp(float power) => MathF.Exp(power);
    public static float Log(float value) => MathF.Log(value);
    public static float Log(float value, float new_base) => MathF.Log(value, new_base);
    public static float Log2(float value) => MathF.Log2(value);
    public static float Log10(float value) => MathF.Log10(value);

    public static float Sin(float radians) => MathF.Sin(radians);
    public static float Cos(float radians) => MathF.Cos(radians);
    public static float Tan(float radians) => MathF.Tan(radians);
    public static float Asin(float value) => MathF.Asin(value);
    public static float Acos(float value) => MathF.Acos(value);
    public static float Atan(float value) => MathF.Atan(value);
    public static float Atan2(float y, float x) => MathF.Atan2(y, x);
    public static float Radians(float degrees) => degrees * kDegToRad;
    public static float Degrees(float radians) => radians * kRadToDeg;

    /// from a at t = 0 to b at t = 1, t held between them
    public static float Lerp(float a, float b, float t) => a + (b - a) * Clamp01(t);
    public static float LerpUnclamped(float a, float b, float t) => a + (b - a) * t;
    /// where value is between a and b, from 0 to 1: the t that Lerp would have been given
    public static float InverseLerp(float a, float b, float value) => a == b ? 0f : Clamp01((value - a) / (b - a));

    /// value, which is somewhere on from_min to from_max, at the same place on to_min to
    ///  to_max. it is not held inside the range
    public static float Remap(float value, float from_min, float from_max, float to_min, float to_max)
    {
      if (from_min == from_max)
      {
        return to_min;
      }
      return to_min + (value - from_min) / (from_max - from_min) * (to_max - to_min);
    }

    /// as Lerp, but slow to leave a and slow to arrive at b
    public static float SmoothStep(float a, float b, float t)
    {
      t = Clamp01(t);
      return LerpUnclamped(a, b, t * t * (3f - 2f * t));
    }

    /// current, moved towards target by no more than max_delta and never past it
    public static float MoveTowards(float current, float target, float max_delta)
    {
      if (Abs(target - current) <= max_delta)
      {
        return target;
      }
      return current + Sign(target - current) * max_delta;
    }

    /// t wrapped into 0 up to length, as a clock's hand is
    public static float Repeat(float t, float length)
    {
      if (length == 0f)
      {
        return 0f;
      }
      return Clamp(t - MathF.Floor(t / length) * length, 0f, length);
    }

    /// t going from 0 up to length and back down again, over and over
    public static float PingPong(float t, float length) => length - Abs(Repeat(t, length * 2f) - length);

    /// the shortest way round from one angle to another in degrees, from -180 to 180
    public static float DeltaAngle(float current, float target)
    {
      float delta = Repeat(target - current, 360f);
      return delta > 180f ? delta - 360f : delta;
    }

    /// as Lerp between two angles in degrees, going the shortest way round
    public static float LerpAngle(float a, float b, float t) => a + DeltaAngle(a, b) * Clamp01(t);

    /// as MoveTowards between two angles in degrees, going the shortest way round
    public static float MoveTowardsAngle(float current, float target, float max_delta)
    {
      float delta = DeltaAngle(current, target);
      if (Abs(delta) <= max_delta)
      {
        return target;
      }
      return current + Sign(delta) * max_delta;
    }

    /// the same but for what arithmetic loses: within tolerance of each other, or of the
    ///  larger of the two when they are large
    public static bool Approximately(float a, float b, float tolerance = kEpsilon)
    {
      return Abs(a - b) <= tolerance * Max(1f, Max(Abs(a), Abs(b)));
    }

    /// current, eased towards target as a spring with no bounce would bring it: quick
    ///  when far, slow when near. velocity is the caller's to keep between calls and
    ///  smooth_time is about how long it takes to arrive
    public static float SmoothDamp(float current, float target, ref float velocity, float smooth_time, float delta_time, float max_speed = kInfinity)
    {
      if (delta_time <= 0f)
      {
        return current;
      }
      smooth_time = Max(0.0001f, smooth_time);
      float omega = 2f / smooth_time;
      float x = omega * delta_time;
      float decay = 1f / (1f + x + 0.48f * x * x + 0.235f * x * x * x);
      float max_change = max_speed * smooth_time;
      float change = Clamp(current - target, -max_change, max_change);
      float aimed_at = current - change;
      float step = (velocity + omega * change) * delta_time;
      velocity = (velocity - omega * step) * decay;
      float result = aimed_at + (change + step) * decay;

      /// it never goes past where it was sent
      if ((target - current > 0f) == (result > target))
      {
        result = target;
        velocity = 0f;
      }
      return result;
    }

    public static int Abs(int value) => value < 0 ? -value : value;
    public static int Sign(int value) => value > 0 ? 1 : (value < 0 ? -1 : 0);
    public static int Min(int a, int b) => a < b ? a : b;
    public static int Max(int a, int b) => a > b ? a : b;
    public static int Clamp(int value, int min, int max) => value < min ? min : (value > max ? max : value);

    public static double Abs(double value) => Math.Abs(value);
    public static double Sign(double value) => value > 0.0 ? 1.0 : (value < 0.0 ? -1.0 : 0.0);
    public static double Min(double a, double b) => a < b ? a : b;
    public static double Max(double a, double b) => a > b ? a : b;
    public static double Clamp(double value, double min, double max) => value < min ? min : (value > max ? max : value);
    public static double Clamp01(double value) => Clamp(value, 0.0, 1.0);
    public static double Radians(double degrees) => degrees * (Math.PI / 180.0);
    public static double Degrees(double radians) => radians * (180.0 / Math.PI);
    public static double Lerp(double a, double b, double t) => a + (b - a) * Clamp01(t);
    public static double LerpUnclamped(double a, double b, double t) => a + (b - a) * t;
    public static double InverseLerp(double a, double b, double value) => a == b ? 0.0 : Clamp01((value - a) / (b - a));

    public static double Remap(double value, double from_min, double from_max, double to_min, double to_max)
    {
      if (from_min == from_max)
      {
        return to_min;
      }
      return to_min + (value - from_min) / (from_max - from_min) * (to_max - to_min);
    }

    public static double SmoothStep(double a, double b, double t)
    {
      t = Clamp01(t);
      return LerpUnclamped(a, b, t * t * (3.0 - 2.0 * t));
    }

    public static double MoveTowards(double current, double target, double max_delta)
    {
      if (Abs(target - current) <= max_delta)
      {
        return target;
      }
      return current + Sign(target - current) * max_delta;
    }

    public static double Repeat(double t, double length)
    {
      if (length == 0.0)
      {
        return 0.0;
      }
      return Clamp(t - Math.Floor(t / length) * length, 0.0, length);
    }

    public static double PingPong(double t, double length) => length - Abs(Repeat(t, length * 2.0) - length);

    public static double DeltaAngle(double current, double target)
    {
      double delta = Repeat(target - current, 360.0);
      return delta > 180.0 ? delta - 360.0 : delta;
    }

    public static bool Approximately(double a, double b, double tolerance = kEpsilonDouble)
    {
      return Abs(a - b) <= tolerance * Max(1.0, Max(Abs(a), Abs(b)));
    }
  }
}
