using System;
using System.Collections.Generic;
using Other;

/// the maths a script is given, checked against answers worked out by hand. the suite asks
///  how many checks a group has and then for each in turn; one that fails says which it
///  was on the error stream, since only a number goes back
public static class MathsProbe
{
  private struct Check
  {
    public string name;
    public Func<bool> passes;
  }

  private static readonly List<Check>[] groups = { MathfChecks(), Vec2Checks(), Vec3Checks(), Vec4Checks(), QuaternionChecks() };

  /// -1 past the last group
  public static int Count(int group) { return group >= 0 && group < groups.Length ? groups[group].Count : -1; }

  public static int Passes(int group, int index)
  {
    Check check = groups[group][index];
    bool passed;
    try
    {
      passed = check.passes();
    }
    catch (Exception e)
    {
      Console.Error.WriteLine($"maths check threw: {check.name}: {e.Message}");
      return 0;
    }
    if (!passed)
    {
      Console.Error.WriteLine($"maths check failed: {check.name}");
    }
    return passed ? 1 : 0;
  }

  public static string Name(int group, int index) { return groups[group][index].name; }

  /// what a script makes of whole degrees of pitch, yaw and roll, for the suite to hold
  ///  beside what the engine makes of them: parts 0 to 3 are the quaternion's X, Y, Z and
  ///  W in millionths, parts 4 to 6 the angles read back from it in thousandths of a degree
  public static int EulerPart(int pitch, int yaw, int roll, int part)
  {
    Quaternion made = Quaternion.FromEuler(pitch, yaw, roll);
    if (part < 4)
    {
      float[] parts = { made.X, made.Y, made.Z, made.W };
      return (int)MathF.Round(parts[part] * 1000000f);
    }
    return (int)MathF.Round(made.ToEulerAngles()[part - 4] * 1000f);
  }

  private static void Add(List<Check> to, string name, Func<bool> passes)
  {
    to.Add(new Check { name = name, passes = passes });
  }

  private static bool Near(float a, float b, float tolerance = 1e-4f) { return MathF.Abs(a - b) <= tolerance; }
  private static bool Near(Vec2 a, Vec2 b, float tolerance = 1e-4f) { return Near(a.X, b.X, tolerance) && Near(a.Y, b.Y, tolerance); }
  private static bool Near(Vec3 a, Vec3 b, float tolerance = 1e-4f) { return Near(a.X, b.X, tolerance) && Near(a.Y, b.Y, tolerance) && Near(a.Z, b.Z, tolerance); }
  private static bool Near(Vec4 a, Vec4 b, float tolerance = 1e-4f) { return Near(a.XYZ, b.XYZ, tolerance) && Near(a.W, b.W, tolerance); }

  private static bool Throws(Action action)
  {
    try
    {
      action();
    }
    catch (IndexOutOfRangeException)
    {
      return true;
    }
    return false;
  }

  private static List<Check> MathfChecks()
  {
    List<Check> checks = new List<Check>();
    Add(checks, "Mathf constants", () => Near(Mathf.kPi, 3.14159265f) && Near(Mathf.kTau, 6.2831853f) && Near(Mathf.kE, 2.7182818f) && Near(Mathf.kDegToRad * 180f, Mathf.kPi) && Near(Mathf.kRadToDeg * Mathf.kPi, 180f));
    Add(checks, "Mathf.Abs and Sign", () => Mathf.Abs(-2.5f) == 2.5f && Mathf.Sign(-3f) == -1f && Mathf.Sign(0f) == 0f && Mathf.Sign(4f) == 1f && Mathf.Sign(float.NaN) == 0f);
    Add(checks, "Mathf.Min and Max", () => Mathf.Min(2f, -1f) == -1f && Mathf.Max(2f, -1f) == 2f && Mathf.Min(3f, 1f, 2f) == 1f && Mathf.Max(3f, 7f, 2f, 5f) == 7f);
    Add(checks, "Mathf.Clamp", () => Mathf.Clamp(5f, 0f, 2f) == 2f && Mathf.Clamp(-5f, 0f, 2f) == 0f && Mathf.Clamp(1f, 0f, 2f) == 1f && Mathf.Clamp01(1.5f) == 1f && Mathf.Clamp01(-0.5f) == 0f);
    Add(checks, "Mathf whole numbers", () => Mathf.Floor(-1.5f) == -2f && Mathf.Ceil(1.2f) == 2f && Mathf.Round(2.5f) == 3f && Mathf.Round(-2.5f) == -3f && Mathf.Round(2.4f) == 2f && Mathf.Trunc(-1.7f) == -1f);
    Add(checks, "Mathf.Frac", () => Near(Mathf.Frac(1.25f), 0.25f) && Near(Mathf.Frac(-1.25f), 0.75f));
    Add(checks, "Mathf to int", () => Mathf.FloorToInt(-0.5f) == -1 && Mathf.CeilToInt(0.2f) == 1 && Mathf.RoundToInt(1.5f) == 2 && Mathf.RoundToInt(-1.5f) == -2);
    Add(checks, "Mathf powers and logs", () => Near(Mathf.Sqrt(16f), 4f) && Near(Mathf.Pow(2f, 10f), 1024f) && Near(Mathf.Exp(1f), Mathf.kE) && Near(Mathf.Log(Mathf.kE), 1f) && Near(Mathf.Log(8f, 2f), 3f) && Near(Mathf.Log2(8f), 3f) && Near(Mathf.Log10(1000f), 3f));
    Add(checks, "Mathf trigonometry", () => Near(Mathf.Sin(Mathf.kPi / 2f), 1f) && Near(Mathf.Cos(Mathf.kPi), -1f) && Near(Mathf.Tan(Mathf.kPi / 4f), 1f) && Near(Mathf.Asin(1f), Mathf.kPi / 2f) && Near(Mathf.Acos(-1f), Mathf.kPi) && Near(Mathf.Atan(1f), Mathf.kPi / 4f) && Near(Mathf.Atan2(1f, -1f), Mathf.kPi * 0.75f));
    Add(checks, "Mathf.Radians and Degrees", () => Near(Mathf.Radians(180f), Mathf.kPi) && Near(Mathf.Degrees(Mathf.kPi / 2f), 90f));
    Add(checks, "Mathf.Lerp", () => Mathf.Lerp(2f, 6f, 0.25f) == 3f && Mathf.Lerp(2f, 6f, 2f) == 6f && Mathf.Lerp(2f, 6f, -1f) == 2f && Mathf.LerpUnclamped(2f, 6f, 2f) == 10f);
    Add(checks, "Mathf.InverseLerp", () => Mathf.InverseLerp(2f, 6f, 3f) == 0.25f && Mathf.InverseLerp(2f, 6f, 9f) == 1f && Mathf.InverseLerp(2f, 2f, 5f) == 0f);
    Add(checks, "Mathf.Remap", () => Near(Mathf.Remap(5f, 0f, 10f, 100f, 200f), 150f) && Near(Mathf.Remap(15f, 0f, 10f, 100f, 200f), 250f) && Mathf.Remap(1f, 3f, 3f, 7f, 9f) == 7f);
    Add(checks, "Mathf.SmoothStep", () => Mathf.SmoothStep(0f, 10f, 0f) == 0f && Mathf.SmoothStep(0f, 10f, 1f) == 10f && Near(Mathf.SmoothStep(0f, 10f, 0.5f), 5f) && Near(Mathf.SmoothStep(0f, 1f, 0.25f), 0.15625f) && Mathf.SmoothStep(0f, 10f, 3f) == 10f);
    Add(checks, "Mathf.MoveTowards", () => Mathf.MoveTowards(0f, 10f, 3f) == 3f && Mathf.MoveTowards(9f, 10f, 3f) == 10f && Mathf.MoveTowards(0f, -10f, 3f) == -3f);
    Add(checks, "Mathf.Repeat", () => Near(Mathf.Repeat(7f, 3f), 1f) && Near(Mathf.Repeat(-1f, 3f), 2f) && Mathf.Repeat(5f, 0f) == 0f);
    Add(checks, "Mathf.PingPong", () => Near(Mathf.PingPong(1f, 3f), 1f) && Near(Mathf.PingPong(4f, 3f), 2f) && Near(Mathf.PingPong(6f, 3f), 0f) && Near(Mathf.PingPong(-1f, 3f), 1f));
    Add(checks, "Mathf.DeltaAngle", () => Near(Mathf.DeltaAngle(350f, 10f), 20f) && Near(Mathf.DeltaAngle(10f, 350f), -20f) && Near(Mathf.DeltaAngle(0f, 180f), 180f) && Near(Mathf.DeltaAngle(0f, 720f), 0f));
    Add(checks, "Mathf.LerpAngle", () => Near(Mathf.LerpAngle(350f, 10f, 0.5f), 360f) && Near(Mathf.LerpAngle(10f, 350f, 0.5f), 0f));
    Add(checks, "Mathf.MoveTowardsAngle", () => Near(Mathf.MoveTowardsAngle(350f, 10f, 5f), 355f) && Near(Mathf.MoveTowardsAngle(350f, 10f, 30f), 10f) && Near(Mathf.MoveTowardsAngle(10f, 350f, 5f), 5f));
    Add(checks, "Mathf.Approximately", () => Mathf.Approximately(0.1f + 0.2f, 0.3f) && !Mathf.Approximately(1f, 1.001f) && Mathf.Approximately(1000000f, 1000000.5f) && Mathf.Approximately(1f, 1.05f, 0.1f));
    Add(checks, "Mathf.SmoothDamp arrives and never passes", () =>
    {
      float value = 0f;
      float velocity = 0f;
      float before = value;
      for (int i = 0; i < 600; i++)
      {
        value = Mathf.SmoothDamp(value, 10f, ref velocity, 0.3f, 1f / 60f);
        if (value < before || value > 10f)
        {
          return false;
        }
        before = value;
      }
      return Near(value, 10f, 1e-3f) && Mathf.SmoothDamp(4f, 10f, ref velocity, 0.3f, 0f) == 4f;
    });
    Add(checks, "Mathf.SmoothDamp keeps to its top speed", () =>
    {
      float velocity = 0f;
      float value = 0f;
      for (int i = 0; i < 60; i++)
      {
        float next = Mathf.SmoothDamp(value, 1000f, ref velocity, 0.3f, 1f / 60f, 5f);
        if ((next - value) * 60f > 5f + 1e-3f)
        {
          return false;
        }
        value = next;
      }
      return value > 0f;
    });
    Add(checks, "Mathf for ints", () => Mathf.Abs(-3) == 3 && Mathf.Sign(-3) == -1 && Mathf.Min(2, 5) == 2 && Mathf.Max(2, 5) == 5 && Mathf.Clamp(9, 0, 4) == 4 && Mathf.Clamp(-9, 0, 4) == 0);
    Add(checks, "Mathf for doubles", () =>
    {
      double third = Mathf.Lerp(0.0, 1.0, 1.0 / 3.0);
      return third == 1.0 / 3.0 && Mathf.Clamp(5.0, 0.0, 2.0) == 2.0 && Mathf.Clamp01(-1.0) == 0.0 && Mathf.Abs(-2.0) == 2.0 && Mathf.Sign(-2.0) == -1.0 && Mathf.Min(1.0, 2.0) == 1.0 && Mathf.Max(1.0, 2.0) == 2.0
        && Mathf.LerpUnclamped(2.0, 6.0, 2.0) == 10.0 && Mathf.InverseLerp(2.0, 6.0, 3.0) == 0.25 && Mathf.Remap(5.0, 0.0, 10.0, 100.0, 200.0) == 150.0 && Mathf.SmoothStep(0.0, 1.0, 0.25) == 0.15625
        && Mathf.MoveTowards(0.0, 10.0, 3.0) == 3.0 && Mathf.Repeat(7.0, 3.0) == 1.0 && Mathf.PingPong(4.0, 3.0) == 2.0 && Mathf.DeltaAngle(350.0, 10.0) == 20.0
        && Mathf.Approximately(0.1 + 0.2, 0.3) && !Mathf.Approximately(1.0, 1.000001) && Math.Abs(Mathf.Radians(180.0) - Math.PI) < 1e-15 && Math.Abs(Mathf.Degrees(Math.PI) - 180.0) < 1e-12;
    });
    return checks;
  }

  private static List<Check> Vec2Checks()
  {
    List<Check> checks = new List<Check>();
    Vec2 a = new Vec2(3f, 4f);
    Vec2 b = new Vec2(-1f, 2f);
    Add(checks, "Vec2 arithmetic", () => a + b == new Vec2(2f, 6f) && a - b == new Vec2(4f, 2f) && a * b == new Vec2(-3f, 8f) && a / new Vec2(3f, 2f) == new Vec2(1f, 2f) && a * 2f == new Vec2(6f, 8f) && 2f * a == new Vec2(6f, 8f) && a / 2f == new Vec2(1.5f, 2f) && -a == new Vec2(-3f, -4f));
    Add(checks, "Vec2 by index", () =>
    {
      Vec2 v = a;
      v[1] = 9f;
      return a[0] == 3f && a[1] == 4f && v == new Vec2(3f, 9f) && Throws(() => { float none = a[2]; });
    });
    Add(checks, "Vec2 equality, text and parts", () =>
    {
      (float x, float y) = a;
      return a.Equals(new Vec2(3f, 4f)) && a != b && a.GetHashCode() == new Vec2(3f, 4f).GetHashCode() && a.ToString() == "(3, 4)" && new Vec2(0.5f, -1.25f).ToString() == "(0.5, -1.25)" && x == 3f && y == 4f;
    });
    Add(checks, "Vec2 length", () => a.LengthSquared() == 25f && a.Length() == 5f && Near(a.Normalized(), new Vec2(0.6f, 0.8f)) && Vec2.Zero.Normalized() == Vec2.Zero && Vec2.Distance(a, b) == MathF.Sqrt(20f) && Vec2.DistanceSquared(a, b) == 20f);
    Add(checks, "Vec2.Dot and Cross", () => Vec2.Dot(a, b) == 5f && Vec2.Cross(Vec2.UnitX, Vec2.UnitY) == 1f && Vec2.Cross(Vec2.UnitY, Vec2.UnitX) == -1f);
    Add(checks, "Vec2.Lerp and MoveTowards", () => Vec2.Lerp(a, b, 0.5f) == new Vec2(1f, 3f) && Vec2.Lerp(a, b, 2f) == b && Vec2.LerpUnclamped(a, b, 2f) == new Vec2(-5f, 0f) && Near(Vec2.MoveTowards(Vec2.Zero, a, 2.5f), new Vec2(1.5f, 2f)) && Vec2.MoveTowards(Vec2.Zero, a, 50f) == a);
    Add(checks, "Vec2 by component", () => Vec2.Min(a, b) == new Vec2(-1f, 2f) && Vec2.Max(a, b) == new Vec2(3f, 4f) && Vec2.Abs(b) == new Vec2(1f, 2f) && Vec2.Clamp(new Vec2(5f, -5f), Vec2.Zero, Vec2.One) == new Vec2(1f, 0f));
    Add(checks, "Vec2.ClampLength", () => Near(Vec2.ClampLength(a, 2.5f), new Vec2(1.5f, 2f)) && Vec2.ClampLength(a, 10f) == a);
    Add(checks, "Vec2.Reflect, Project and Perpendicular", () => Vec2.Reflect(new Vec2(1f, -1f), Vec2.UnitY) == new Vec2(1f, 1f) && Vec2.Project(a, new Vec2(2f, 0f)) == new Vec2(3f, 0f) && Vec2.Project(a, Vec2.Zero) == Vec2.Zero && Vec2.Perpendicular(Vec2.UnitX) == Vec2.UnitY);
    Add(checks, "Vec2 angles", () => Near(Vec2.Angle(Vec2.UnitX, Vec2.UnitY), 90f) && Near(Vec2.Angle(Vec2.UnitX, new Vec2(-2f, 0f)), 180f, 1e-2f) && Vec2.Angle(Vec2.Zero, Vec2.UnitX) == 0f && Near(Vec2.SignedAngle(Vec2.UnitX, Vec2.UnitY), 90f) && Near(Vec2.SignedAngle(Vec2.UnitY, Vec2.UnitX), -90f));
    Add(checks, "Vec2.Approximately", () => Vec2.Approximately(new Vec2(0.1f + 0.2f, 1f), new Vec2(0.3f, 1f)) && !Vec2.Approximately(a, b));
    Add(checks, "Vec2 by name", () => Vec2.Zero == new Vec2(0f, 0f) && Vec2.One == new Vec2(1f, 1f) && Vec2.UnitX == new Vec2(1f, 0f) && Vec2.UnitY == new Vec2(0f, 1f) && Vec2.Up == Vec2.UnitY && Vec2.Down == -Vec2.UnitY && Vec2.Right == Vec2.UnitX && Vec2.Left == -Vec2.UnitX);
    return checks;
  }

  private static List<Check> Vec3Checks()
  {
    List<Check> checks = new List<Check>();
    Vec3 a = new Vec3(1f, 2f, 2f);
    Vec3 b = new Vec3(-2f, 0f, 4f);
    Add(checks, "Vec3 arithmetic", () => a + b == new Vec3(-1f, 2f, 6f) && a - b == new Vec3(3f, 2f, -2f) && a * b == new Vec3(-2f, 0f, 8f) && b / a == new Vec3(-2f, 0f, 2f) && a * 2f == new Vec3(2f, 4f, 4f) && 2f * a == new Vec3(2f, 4f, 4f) && a / 2f == new Vec3(0.5f, 1f, 1f) && -a == new Vec3(-1f, -2f, -2f));
    Add(checks, "Vec3 by index", () =>
    {
      Vec3 v = a;
      v[2] = 9f;
      return a[0] == 1f && a[1] == 2f && a[2] == 2f && v == new Vec3(1f, 2f, 9f) && Throws(() => { float none = a[3]; }) && Throws(() => { float none = a[-1]; });
    });
    Add(checks, "Vec3 equality, text and parts", () =>
    {
      (float x, float y, float z) = b;
      return a.Equals(new Vec3(1f, 2f, 2f)) && a != b && a.ToString() == "(1, 2, 2)" && x == -2f && y == 0f && z == 4f && new Vec3(new Vec2(7f, 8f), 9f) == new Vec3(7f, 8f, 9f) && a.XY == new Vec2(1f, 2f);
    });
    Add(checks, "Vec3 length", () => a.LengthSquared() == 9f && a.Length() == 3f && Near(a.Normalized(), new Vec3(1f / 3f, 2f / 3f, 2f / 3f)) && Vec3.Zero.Normalized() == Vec3.Zero && Vec3.DistanceSquared(a, b) == 17f && Near(Vec3.Distance(a, b), MathF.Sqrt(17f)));
    Add(checks, "Vec3.Dot and Cross", () => Vec3.Dot(a, b) == 6f && Vec3.Cross(Vec3.UnitX, Vec3.UnitY) == Vec3.UnitZ && Vec3.Cross(Vec3.UnitY, Vec3.UnitZ) == Vec3.UnitX && Vec3.Cross(Vec3.UnitZ, Vec3.UnitX) == Vec3.UnitY);
    Add(checks, "Vec3.Lerp and MoveTowards", () => Vec3.Lerp(a, b, 0.5f) == new Vec3(-0.5f, 1f, 3f) && Vec3.Lerp(a, b, -1f) == a && Vec3.LerpUnclamped(a, b, 2f) == new Vec3(-5f, -2f, 6f) && Near(Vec3.MoveTowards(Vec3.Zero, a, 1.5f), new Vec3(0.5f, 1f, 1f)) && Vec3.MoveTowards(Vec3.Zero, a, 3f) == a && Vec3.MoveTowards(a, a, 1f) == a);
    Add(checks, "Vec3 by component", () => Vec3.Min(a, b) == new Vec3(-2f, 0f, 2f) && Vec3.Max(a, b) == new Vec3(1f, 2f, 4f) && Vec3.Abs(b) == new Vec3(2f, 0f, 4f) && Vec3.Clamp(new Vec3(5f, -5f, 0.5f), Vec3.Zero, Vec3.One) == new Vec3(1f, 0f, 0.5f));
    Add(checks, "Vec3.ClampLength", () => Near(Vec3.ClampLength(a, 1.5f), new Vec3(0.5f, 1f, 1f)) && Vec3.ClampLength(a, 3f) == a);
    Add(checks, "Vec3.Reflect and Project", () => Vec3.Reflect(new Vec3(1f, -1f, 0f), Vec3.Up) == new Vec3(1f, 1f, 0f) && Vec3.Project(a, new Vec3(0f, 5f, 0f)) == new Vec3(0f, 2f, 0f) && Vec3.Project(a, Vec3.Zero) == Vec3.Zero && Vec3.ProjectOnPlane(a, Vec3.Up) == new Vec3(1f, 0f, 2f));
    Add(checks, "Vec3 angles", () => Near(Vec3.Angle(Vec3.UnitX, Vec3.UnitY), 90f) && Near(Vec3.Angle(Vec3.UnitX, new Vec3(1f, 1f, 0f)), 45f) && Vec3.Angle(Vec3.Zero, Vec3.UnitX) == 0f
      && Near(Vec3.SignedAngle(Vec3.UnitX, Vec3.UnitZ, Vec3.Up), -90f) && Near(Vec3.SignedAngle(Vec3.UnitZ, Vec3.UnitX, Vec3.Up), 90f));
    Add(checks, "Vec3.Approximately", () => Vec3.Approximately(new Vec3(0.1f + 0.2f, 1f, 2f), new Vec3(0.3f, 1f, 2f)) && !Vec3.Approximately(a, b));
    Add(checks, "Vec3 by name", () => Vec3.Zero == new Vec3(0f, 0f, 0f) && Vec3.One == new Vec3(1f, 1f, 1f) && Vec3.UnitX == new Vec3(1f, 0f, 0f) && Vec3.UnitY == new Vec3(0f, 1f, 0f) && Vec3.UnitZ == new Vec3(0f, 0f, 1f)
      && Vec3.Up == Vec3.UnitY && Vec3.Down == -Vec3.UnitY && Vec3.Right == Vec3.UnitX && Vec3.Left == -Vec3.UnitX && Vec3.Forward == Vec3.UnitZ && Vec3.Backward == -Vec3.UnitZ);
    return checks;
  }

  private static List<Check> Vec4Checks()
  {
    List<Check> checks = new List<Check>();
    Vec4 a = new Vec4(1f, 2f, 2f, 4f);
    Vec4 b = new Vec4(-1f, 0f, 4f, 2f);
    Add(checks, "Vec4 arithmetic", () => a + b == new Vec4(0f, 2f, 6f, 6f) && a - b == new Vec4(2f, 2f, -2f, 2f) && a * b == new Vec4(-1f, 0f, 8f, 8f) && b / a == new Vec4(-1f, 0f, 2f, 0.5f) && a * 2f == new Vec4(2f, 4f, 4f, 8f) && 2f * a == a * 2f && a / 2f == new Vec4(0.5f, 1f, 1f, 2f) && -a == new Vec4(-1f, -2f, -2f, -4f));
    Add(checks, "Vec4 by index", () =>
    {
      Vec4 v = a;
      v[3] = 9f;
      return a[0] == 1f && a[1] == 2f && a[2] == 2f && a[3] == 4f && v == new Vec4(1f, 2f, 2f, 9f) && Throws(() => { float none = a[4]; });
    });
    Add(checks, "Vec4 equality, text and parts", () =>
    {
      (float x, float y, float z, float w) = b;
      return a.Equals(new Vec4(1f, 2f, 2f, 4f)) && a != b && a.ToString() == "(1, 2, 2, 4)" && x == -1f && y == 0f && z == 4f && w == 2f && new Vec4(new Vec3(7f, 8f, 9f), 1f) == new Vec4(7f, 8f, 9f, 1f) && a.XYZ == new Vec3(1f, 2f, 2f) && a.XY == new Vec2(1f, 2f);
    });
    Add(checks, "Vec4 length", () => a.LengthSquared() == 25f && a.Length() == 5f && Near(a.Normalized(), new Vec4(0.2f, 0.4f, 0.4f, 0.8f)) && Vec4.Zero.Normalized() == Vec4.Zero && Vec4.Dot(a, b) == 15f && Vec4.DistanceSquared(a, b) == 16f && Vec4.Distance(a, b) == 4f);
    Add(checks, "Vec4.Lerp", () => Vec4.Lerp(a, b, 0.5f) == new Vec4(0f, 1f, 3f, 3f) && Vec4.Lerp(a, b, 5f) == b && Vec4.LerpUnclamped(a, b, 2f) == new Vec4(-3f, -2f, 6f, 0f));
    Add(checks, "Vec4 by component", () => Vec4.Min(a, b) == new Vec4(-1f, 0f, 2f, 2f) && Vec4.Max(a, b) == new Vec4(1f, 2f, 4f, 4f) && Vec4.Abs(-a) == a && Vec4.Clamp(new Vec4(5f, -5f, 0.5f, 2f), Vec4.Zero, Vec4.One) == new Vec4(1f, 0f, 0.5f, 1f));
    Add(checks, "Vec4.Approximately", () => Vec4.Approximately(new Vec4(0.1f + 0.2f, 1f, 2f, 3f), new Vec4(0.3f, 1f, 2f, 3f)) && !Vec4.Approximately(a, b));
    Add(checks, "Vec4 by name", () => Vec4.Zero == new Vec4(0f, 0f, 0f, 0f) && Vec4.One == new Vec4(1f, 1f, 1f, 1f) && Vec4.UnitX == new Vec4(1f, 0f, 0f, 0f) && Vec4.UnitY == new Vec4(0f, 1f, 0f, 0f) && Vec4.UnitZ == new Vec4(0f, 0f, 1f, 0f) && Vec4.UnitW == new Vec4(0f, 0f, 0f, 1f));
    return checks;
  }

  private static List<Check> QuaternionChecks()
  {
    List<Check> checks = new List<Check>();
    float quarter = MathF.PI / 2f;
    Quaternion about_y = Quaternion.FromAxisAngle(Vec3.UnitY, quarter);
    Quaternion about_x = Quaternion.FromAxisAngle(Vec3.UnitX, quarter);
    Add(checks, "Quaternion turns counter-clockwise seen from where the axis points", () => Near(about_y * Vec3.UnitZ, Vec3.UnitX) && Near(about_x * Vec3.UnitY, Vec3.UnitZ) && Near(Quaternion.FromAxisAngle(Vec3.UnitZ, quarter) * Vec3.UnitX, Vec3.UnitY));
    Add(checks, "Quaternion product turns by the right, then by the left", () => Near((about_y * about_x) * Vec3.UnitY, Vec3.UnitX) && Near((about_x * about_y) * Vec3.UnitY, Vec3.UnitZ));
    Add(checks, "Quaternion.Inverse undoes", () => Near((about_y.Inverse() * about_y) * new Vec3(1f, 2f, 3f), new Vec3(1f, 2f, 3f)) && Near(about_y.Conjugate() * Vec3.UnitX, Vec3.UnitZ) && Quaternion.Approximately(about_y / about_y, Quaternion.Identity));
    Add(checks, "Quaternion negated is the same rotation", () => Near(-about_y * Vec3.UnitZ, Vec3.UnitX) && Quaternion.Approximately(about_y, -about_y) && !Quaternion.Approximately(about_y, about_x) && -about_y != about_y);
    Add(checks, "Quaternion text", () => Quaternion.Identity.ToString() == "(0, 0, 0, 1)");
    Add(checks, "Quaternion.ToAxisAngle gives back what FromAxisAngle was given", () =>
    {
      Quaternion.FromAxisAngle(new Vec3(0f, 0f, 3f), 1.25f).ToAxisAngle(out Vec3 axis, out float radians);
      (-about_y).ToAxisAngle(out Vec3 same_axis, out float same_radians);
      Quaternion.Identity.ToAxisAngle(out Vec3 no_axis, out float none);
      return Near(axis, Vec3.UnitZ) && Near(radians, 1.25f) && Near(same_axis, Vec3.UnitY) && Near(same_radians, quarter) && no_axis == Vec3.UnitX && none == 0f;
    });
    Add(checks, "Quaternion.Angle", () => Near(Quaternion.Angle(Quaternion.Identity, about_y), 90f, 1e-2f) && Near(Quaternion.Angle(about_y, -about_y), 0f, 1e-1f) && Near(Quaternion.Angle(about_x, about_y), 120f, 1e-2f));
    Add(checks, "Quaternion.Slerp goes at an even speed the shortest way", () =>
    {
      Quaternion third = Quaternion.Slerp(Quaternion.Identity, about_y, 1f / 3f);
      Quaternion short_way = Quaternion.Slerp(Quaternion.Identity, -about_y, 0.5f);
      return Near(Quaternion.Angle(Quaternion.Identity, third), 30f, 1e-2f) && Near(third * Vec3.UnitZ, new Vec3(0.5f, 0f, MathF.Sqrt(3f) / 2f)) && Near(Quaternion.Angle(Quaternion.Identity, short_way), 45f, 1e-2f)
        && Quaternion.Approximately(Quaternion.Slerp(about_x, about_y, 0f), about_x) && Quaternion.Approximately(Quaternion.Slerp(about_x, about_y, 7f), about_y) && Quaternion.Approximately(Quaternion.Slerp(about_y, about_y, 0.5f), about_y);
    });
    Add(checks, "Quaternion.Lerp ends where Slerp ends and stays of length one", () =>
    {
      Quaternion half = Quaternion.Lerp(Quaternion.Identity, about_y, 0.5f);
      return Near(half.Length(), 1f) && Near(Quaternion.Angle(Quaternion.Identity, half), 45f, 1e-2f) && Quaternion.Approximately(Quaternion.Lerp(about_x, about_y, 1f), about_y) && Quaternion.Approximately(Quaternion.Lerp(about_x, -about_y, 0f), about_x);
    });
    Add(checks, "Quaternion.RotateTowards", () => Near(Quaternion.Angle(Quaternion.Identity, Quaternion.RotateTowards(Quaternion.Identity, about_y, 30f)), 30f, 1e-2f) && Quaternion.Approximately(Quaternion.RotateTowards(Quaternion.Identity, about_y, 500f), about_y) && Quaternion.Approximately(Quaternion.RotateTowards(about_y, about_y, 1f), about_y));
    Add(checks, "Quaternion.LookRotation", () =>
    {
      Vec3[] ways = { new Vec3(1f, 0f, 0f), new Vec3(0f, 0f, -1f), new Vec3(-1f, 0f, 0f), new Vec3(1f, 2f, 3f), new Vec3(-3f, -1f, 0.5f), new Vec3(0f, 1f, 0f), new Vec3(0f, -1f, 0f), new Vec3(0f, 0.2f, -5f) };
      foreach (Vec3 way in ways)
      {
        Quaternion look = Quaternion.LookRotation(way);
        Vec3 right = look * Vec3.Right;
        Vec3 up = look * Vec3.Up;
        /// it looks along the way and is still a rotation
        if (!Near(look * Vec3.Forward, way.Normalized()) || !Near(look.Length(), 1f) || !Near(Vec3.Dot(right, way), 0f))
        {
          return false;
        }
        /// and unless the way is straight up or down, its right is level and its up leans
        ///  upwards
        bool upright = MathF.Abs(way.Normalized().Y) < 0.99f;
        if (upright && (!Near(right.Y, 0f) || up.Y <= 0f))
        {
          return false;
        }
      }
      return Quaternion.Approximately(Quaternion.LookRotation(Vec3.Forward), Quaternion.Identity) && Quaternion.LookRotation(Vec3.Zero) == Quaternion.Identity && Near(Quaternion.LookRotation(Vec3.Forward, Vec3.Right) * Vec3.Up, Vec3.Right);
    });
    Add(checks, "Quaternion.FromEuler turns each angle round its own axis", () => Near(Quaternion.FromEuler(90f, 0f, 0f) * Vec3.UnitY, Vec3.UnitZ) && Near(Quaternion.FromEuler(0f, 90f, 0f) * Vec3.UnitZ, Vec3.UnitX) && Near(Quaternion.FromEuler(0f, 0f, 90f) * Vec3.UnitX, Vec3.UnitY));
    Add(checks, "Quaternion.FromEuler makes the roll first, then the pitch, then the yaw", () =>
    {
      Quaternion by_hand = Quaternion.FromAxisAngle(Vec3.UnitY, Mathf.Radians(40f)) * Quaternion.FromAxisAngle(Vec3.UnitX, Mathf.Radians(30f)) * Quaternion.FromAxisAngle(Vec3.UnitZ, Mathf.Radians(50f));
      return Quaternion.Approximately(Quaternion.FromEuler(30f, 40f, 50f), by_hand) && Quaternion.Approximately(Quaternion.FromEuler(new Vec3(30f, 40f, 50f)), by_hand)
        && Quaternion.Approximately(Quaternion.CreateFromYawPitchRoll(Mathf.Radians(40f), Mathf.Radians(30f), Mathf.Radians(50f)), by_hand) && Near(Quaternion.FromEuler(-45f, 90f, 0f) * Vec3.UnitZ, new Vec3(1f, 1f, 0f).Normalized());
    });
    Add(checks, "Quaternion.ToEulerAngles gives back what FromEuler was given", () =>
    {
      foreach (float pitch in new[] { -80f, -30f, 0f, 45f, 85f })
      {
        foreach (float yaw in new[] { -170f, -95f, -10f, 0f, 60f, 90f, 135f, 179f })
        {
          foreach (float roll in new[] { -120f, 0f, 15f, 170f })
          {
            Vec3 angles = new Vec3(pitch, yaw, roll);
            if (!Near(Quaternion.FromEuler(angles).ToEulerAngles(), angles, 2e-3f) || !Near((-Quaternion.FromEuler(angles)).ToEulerAngles(), angles, 2e-3f))
            {
              return false;
            }
          }
        }
      }
      return true;
    });
    Add(checks, "Quaternion yaw can be added to for ever", () =>
    {
      /// what a script does that spins a thing by adding to its yaw each frame
      Quaternion rotation = Quaternion.FromEuler(20f, 0f, 0f);
      for (int frame = 0; frame < 400; frame++)
      {
        rotation = Quaternion.FromEuler(rotation.ToEulerAngles() + Vec3.Up * 3f);
      }
      /// 1200 degrees is three turns and 120 over
      return Quaternion.Approximately(rotation, Quaternion.FromEuler(20f, 120f, 0f), 1e-5f);
    });
    Add(checks, "Quaternion straight up and down keep the rotation", () =>
    {
      Vec3[] angles = { new Vec3(90f, 30f, 0f), new Vec3(-90f, -140f, 0f), new Vec3(90f, 30f, 50f), new Vec3(-90f, 10f, -70f) };
      foreach (Vec3 given in angles)
      {
        Quaternion rotation = Quaternion.FromEuler(given);
        Vec3 read = rotation.ToEulerAngles();
        if (!Near(read.X, given.X, 0.1f) || read.Z != 0f || !Quaternion.Approximately(Quaternion.FromEuler(read), rotation, 1e-4f))
        {
          return false;
        }
      }
      return true;
    });
    Add(checks, "Quaternion of all zeroes reads as no rotation", () => new Quaternion(0f, 0f, 0f, 0f).ToEulerAngles() == Vec3.Zero && Near((Quaternion.FromEuler(10f, 20f, 30f) * 4f).ToEulerAngles(), new Vec3(10f, 20f, 30f), 2e-3f));
    Add(checks, "Quaternion.FromToRotation", () =>
    {
      Vec3 from = new Vec3(1f, 2f, 3f);
      Vec3 to = new Vec3(-2f, 0.5f, 1f);
      return Near(Quaternion.FromToRotation(from, to) * from.Normalized(), to.Normalized()) && Near(Quaternion.FromToRotation(Vec3.UnitX, Vec3.UnitY) * Vec3.UnitZ, Vec3.UnitZ) && Quaternion.FromToRotation(from, from * 2f) == Quaternion.Identity
        && Near(Quaternion.FromToRotation(Vec3.UnitX, -Vec3.UnitX) * Vec3.UnitX, -Vec3.UnitX) && Near(Quaternion.FromToRotation(from, -from) * from, -from, 1e-3f);
    });
    return checks;
  }
}
