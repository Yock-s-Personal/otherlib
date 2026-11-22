using System;
using System.Collections.Generic;
using Other.Core;

/// a parent for behaviours that the suite can reach from C# afterwards
class ProbeObject : OtherObject
{
  public static ProbeObject last;

  public ProbeObject() : base(IntPtr.Zero, ObjectType.SceneObject)
  {
    last = this;
  }
}

/// writes down everything it is told, in the order it is told
class RecordingBehavior : OtherBehavior
{
  protected override void Awake() { BehaviorProbe.Note("awake"); }
  protected override void Enable() { BehaviorProbe.Note("enable"); }
  protected override void Start() { BehaviorProbe.Note("start"); }
  protected override void FixedUpdate() { BehaviorProbe.Note("fixed"); }
  protected override void Update() { BehaviorProbe.Note("update"); }
  protected override void LateUpdate() { BehaviorProbe.Note("late"); }
  protected override void Render() { BehaviorProbe.Note("render"); }
  protected override void CollisionEnter(CollisionInfo info) { BehaviorProbe.Note($"hit {info.OtherObjectId} at {info.Point.Y:F0} facing {info.Normal.Z:F0}"); }
  protected override void JointBreak(float force) { BehaviorProbe.Note($"broke at {force:F0}"); }
  protected override void Disable() { BehaviorProbe.Note("disable"); }
  protected override void Destroy() { BehaviorProbe.Note("destroy"); }
}

/// has written only its update: nothing else may reach it
class UpdateOnlyBehavior : OtherBehavior
{
  public static int updates;

  protected override void Update() { updates++; }
}

/// has written nothing at all
class SilentBehavior : OtherBehavior
{
}

/// throws from its update, every time
class ThrowingBehavior : OtherBehavior
{
  public static int updates;

  protected override void Update()
  {
    updates++;
    throw new InvalidOperationException("a behaviour with a bug");
  }
}

/// switches itself off the first time it is updated
class SelfDisablingBehavior : OtherBehavior
{
  public static int updates;
  public static int disables;

  protected override void Update()
  {
    updates++;
    Enabled = false;
  }

  protected override void Disable() { disables++; }
}

/// what the suite asks about the behaviours above
public static class BehaviorProbe
{
  private static readonly List<string> notes = new();

  internal static void Note(string what) { notes.Add(what); }

  public static int Count() { return notes.Count; }

  /// FNV-1a over the notes joined by commas, the same sum the suite makes of what it expects
  public static int Hash()
  {
    uint hash = 2166136261;
    foreach (char c in string.Join(",", notes))
    {
      hash = (hash ^ c) * 16777619;
    }
    return unchecked((int)hash);
  }

  /// the notes into the log, for when the hash is not the one expected
  public static int Say()
  {
    Logger.Log("behaviour notes: " + string.Join(",", notes));
    return notes.Count;
  }

  public static int Clear()
  {
    notes.Clear();
    return 0;
  }

  public static int UpdateOnlyUpdates() { return UpdateOnlyBehavior.updates; }
  public static int ThrowingUpdates() { return ThrowingBehavior.updates; }
  public static int SelfDisablingUpdates() { return SelfDisablingBehavior.updates; }
  public static int SelfDisablingDisables() { return SelfDisablingBehavior.disables; }

  /// through the API a script uses, on the last ProbeObject made
  public static int AddRecorder() { return ProbeObject.last.AddBehavior<RecordingBehavior>() != null ? 1 : 0; }
  public static int AddRecorderTwice() { return ReferenceEquals(ProbeObject.last.AddBehavior<RecordingBehavior>(), ProbeObject.last.AddBehavior<RecordingBehavior>()) ? 1 : 0; }
  public static int RemoveRecorder() { return ProbeObject.last.RemoveBehavior<RecordingBehavior>() ? 1 : 0; }
  public static int HasRecorder() { return ProbeObject.last.GetBehavior<RecordingBehavior>() != null ? 1 : 0; }
  public static int BehaviorCount() { return ProbeObject.last.BehaviorCount; }

  public static int SetRecorderEnabled(int enabled)
  {
    ProbeObject.last.GetBehavior<RecordingBehavior>().Enabled = enabled != 0;
    return 1;
  }

  public static int RecorderEnabled() { return ProbeObject.last.GetBehavior<RecordingBehavior>().Enabled ? 1 : 0; }
}
