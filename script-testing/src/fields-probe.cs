using System;
using System.Collections.Generic;
using Other;
using Other.Core;

public enum ProbeGait
{
  Walk,
  Run,
  Sprint,
}

public struct ProbeInner
{
  public int Count;
  public Vec2 Span;
  public AssetRef Skin;
}

public struct ProbeLimits
{
  public float Min;
  public float Max;
  public ProbeInner Inner;
}

/// something for another behaviour's field to hold
class FriendBehavior : SceneBehavior
{
  public int number = 1;
}

/// a field of every kind that is saved, and one of every sort that is not
class FieldsBehavior : SceneBehavior
{
  public static FieldsBehavior last;

  public bool flag;
  public sbyte tiny = -1;
  public short small = -2;
  public int whole = 3;
  public long big = 4;
  public byte utiny = 1;
  public ushort usmall = 2;
  public uint uwhole = 3;
  public ulong ubig = 4;
  public float speed = 1.5f;
  public double precise = 0.25;
  public string label = "default";
  public char letter = 'a';
  public ProbeGait gait = ProbeGait.Walk;
  public AssetRef icon;
  public Vec2 two;
  public Vec3 spawn;
  public Vec4 tint;
  public Quaternion turn = new Quaternion(0f, 0f, 0f, 1f);
  public ProbeLimits limits = new ProbeLimits { Min = 0f, Max = 1f };
  public SceneObject target;
  public FriendBehavior friend;
  [SerializableField] private int hidden = 7;

  [NonSerialized] public int scratch = 9;
  public static int shared = 1;
  public readonly int fixed_value = 5;
  public int Property { get; set; } = 11;
  private int never = 13;
  public List<int> list = new();

  public FieldsBehavior()
  {
    last = this;
  }

  public int Hidden { get => hidden; set => hidden = value; }
  public int Never { get => never; set => never = value; }
}

/// fields marked for the inspector in every way one can be
class DescribedBehavior : SceneBehavior
{
  [InspectorField("Top Speed", Tooltip = "how fast it goes")]
  [Range(0f, 10f)]
  public float speed = 1f;

  [InspectorGroup("Looks")]
  [ColorField]
  public Vec3 tint;
  public ProbeGait gait;

  [InspectorGroup("")]
  [InspectorSeparator]
  [InspectorField(ReadOnly = true)]
  public int count;
  public ProbeLimits limits;
}

/// what the suite asks about the last FieldsBehavior made. a check answers 0 when all is as
///  it should be, and otherwise the place of the first field that is not
public static class FieldsProbe
{
  public static int SetAll()
  {
    FieldsBehavior b = FieldsBehavior.last;
    b.flag = true;
    b.tiny = -100;
    b.small = -30000;
    b.whole = -2000000000;
    b.big = -9000000000000000000;
    b.utiny = 200;
    b.usmall = 60000;
    b.uwhole = 4000000000;
    b.ubig = ulong.MaxValue - 5;
    b.speed = 0.1f;
    b.precise = 0.1;
    b.label = "héro \"quoted\"";
    b.letter = 'Z';
    b.gait = ProbeGait.Sprint;
    b.icon = new AssetRef("textures/floor.png");
    b.two = new Vec2(1f, 2f);
    b.spawn = new Vec3(0f, 1.5f, -3f);
    b.tint = new Vec4(0.25f, 0.5f, 0.75f, 1f);
    b.turn = new Quaternion(0.5f, 0.5f, 0.5f, 0.5f);
    b.limits = new ProbeLimits { Min = -2f, Max = 8f, Inner = new ProbeInner { Count = 6, Span = new Vec2(3f, 4f), Skin = new AssetRef("textures/skin.png") } };
    b.Hidden = 70;
    return 0;
  }

  public static int CheckAll()
  {
    FieldsBehavior b = FieldsBehavior.last;
    if (!b.flag) { return 1; }
    if (b.tiny != -100) { return 2; }
    if (b.small != -30000) { return 3; }
    if (b.whole != -2000000000) { return 4; }
    if (b.big != -9000000000000000000) { return 5; }
    if (b.utiny != 200) { return 6; }
    if (b.usmall != 60000) { return 7; }
    if (b.uwhole != 4000000000) { return 8; }
    if (b.ubig != ulong.MaxValue - 5) { return 9; }
    if (b.speed != 0.1f) { return 10; }
    if (b.precise != 0.1) { return 11; }
    if (b.label != "héro \"quoted\"") { return 12; }
    if (b.letter != 'Z') { return 13; }
    if (b.gait != ProbeGait.Sprint) { return 14; }
    if (b.icon.Path != "textures/floor.png") { return 15; }
    if (b.two.X != 1f || b.two.Y != 2f) { return 16; }
    if (b.spawn.X != 0f || b.spawn.Y != 1.5f || b.spawn.Z != -3f) { return 17; }
    if (b.tint.X != 0.25f || b.tint.Y != 0.5f || b.tint.Z != 0.75f || b.tint.W != 1f) { return 18; }
    if (b.turn.X != 0.5f || b.turn.Y != 0.5f || b.turn.Z != 0.5f || b.turn.W != 0.5f) { return 19; }
    if (b.limits.Min != -2f || b.limits.Max != 8f) { return 20; }
    if (b.limits.Inner.Count != 6 || b.limits.Inner.Span.X != 3f || b.limits.Inner.Span.Y != 4f || b.limits.Inner.Skin.Path != "textures/skin.png") { return 21; }
    if (b.Hidden != 70) { return 22; }
    return 0;
  }

  public static int CheckDefaults()
  {
    FieldsBehavior b = FieldsBehavior.last;
    if (b.flag) { return 1; }
    if (b.tiny != -1 || b.small != -2 || b.whole != 3 || b.big != 4) { return 2; }
    if (b.utiny != 1 || b.usmall != 2 || b.uwhole != 3 || b.ubig != 4) { return 3; }
    if (b.speed != 1.5f || b.precise != 0.25) { return 4; }
    if (b.label != "default" || b.letter != 'a') { return 5; }
    if (b.gait != ProbeGait.Walk) { return 6; }
    if (!b.icon.IsEmpty) { return 7; }
    if (b.two.X != 0f || b.spawn.Y != 0f || b.tint.W != 0f || b.turn.W != 1f) { return 8; }
    if (b.limits.Min != 0f || b.limits.Max != 1f || b.limits.Inner.Count != 0) { return 9; }
    if (b.target != null || b.friend != null) { return 10; }
    if (b.Hidden != 7) { return 11; }
    return 0;
  }

  /// what is not saved is given values a save must not carry
  public static int SetUnsaved()
  {
    FieldsBehavior b = FieldsBehavior.last;
    b.scratch = 99;
    b.Property = 98;
    b.Never = 97;
    FieldsBehavior.shared = 96;
    return 0;
  }

  public static int CheckUnsavedAsMade()
  {
    FieldsBehavior b = FieldsBehavior.last;
    return b.scratch == 9 && b.Property == 11 && b.Never == 13 && b.fixed_value == 5 ? 0 : 1;
  }

  public static int Whole() { return FieldsBehavior.last.whole; }
  public static int Tiny() { return FieldsBehavior.last.tiny; }
  public static int SpeedTimesTen() { return (int)MathF.Round(FieldsBehavior.last.speed * 10f); }
  public static int Gait() { return (int)FieldsBehavior.last.gait; }
  public static int LimitsMaxTimesTen() { return (int)MathF.Round(FieldsBehavior.last.limits.Max * 10f); }
  public static int IsEnabled() { return FieldsBehavior.last.Enabled ? 1 : 0; }

  /// the low bits of the id of the object the field holds; -1 when it holds none
  public static int TargetId() { return FieldsBehavior.last.target == null ? -1 : (int)(FieldsBehavior.last.target.ObjectID & 0x7fffffff); }
  /// what the behaviour the field holds has in its own field; -1 when it holds none
  public static int FriendNumber() { return FieldsBehavior.last.friend == null ? -1 : FieldsBehavior.last.friend.number; }
  /// the target is the object the friend is on
  public static int FriendIsOnTarget() { return FieldsBehavior.last.friend != null && ReferenceEquals(FieldsBehavior.last.friend.SceneObject, FieldsBehavior.last.target) ? 1 : 0; }

  public static int ChangeWhilePlaying()
  {
    FieldsBehavior b = FieldsBehavior.last;
    b.speed = 99f;
    b.whole = 12345;
    b.label = "changed in play";
    b.target = null;
    b.limits.Max = 77f;
    return 0;
  }
}
