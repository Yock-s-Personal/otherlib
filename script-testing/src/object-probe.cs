using System;
using System.Collections.Generic;
using Other;
using Other.Core;

/// something on one object for a script on another to call
class DoorBehavior : SceneBehavior
{
  public int opened;

  public void Open()
  {
    opened++;
  }
}

/// what the suite asks about scene objects, through the API a script has. the scene it is
///  asked in has a "Hub" at (10, 0, 0) with a DoorBehavior, and a "Spoke" at (1, 2, 3).
///  an answer of 1 is yes; anything else is the place of the first thing that was not so
public static class ObjectProbe
{
  private static SceneObject held;

  private static bool Near(float a, float b) => MathF.Abs(a - b) < 0.0005f;
  private static bool Near(Vec3 v, float x, float y, float z) => Near(v.X, x) && Near(v.Y, y) && Near(v.Z, z);

  private static SceneObject Hub => SceneObject.Find("Hub");
  private static SceneObject Spoke => SceneObject.Find("Spoke");

  public static int CreateAndFind()
  {
    SceneObject made = SceneObject.Create("ProbeMade");
    if (made == null || !made.IsAlive) { return 2; }
    if (made.Name != "ProbeMade") { return 3; }
    /// what finds the object hands back the object itself, not another for it
    if (!ReferenceEquals(SceneObject.Find("ProbeMade"), made)) { return 4; }
    if (!ReferenceEquals(Scene.FindObjectByName("ProbeMade"), made)) { return 5; }
    if (made.Parent != null) { return 6; }
    if (SceneObject.Find("NoSuchObject") != null) { return 7; }

    made.Name = "ProbeRenamed";
    if (!ReferenceEquals(SceneObject.Find("ProbeRenamed"), made)) { return 8; }
    made.Visible = false;
    if (made.Visible) { return 9; }

    SceneObject placed = Scene.CreateObject("ProbePlaced", new Vec3(4f, 5f, 6f));
    return placed != null && Near(placed.Transform.WorldPosition, 4f, 5f, 6f) ? 1 : 10;
  }

  public static int CreateUnderAParent()
  {
    SceneObject hub = Hub;
    SceneObject child = SceneObject.Create("ProbeChild", hub);
    if (child == null || !ReferenceEquals(child.Parent, hub)) { return 2; }
    /// made at the parent, not at the middle of the world
    if (!Near(child.Transform.Position, 0f, 0f, 0f) || !Near(child.Transform.WorldPosition, 10f, 0f, 0f)) { return 3; }

    bool listed = false;
    foreach (SceneObject under in hub.Children)
    {
      listed |= ReferenceEquals(under, child);
    }
    return listed ? 1 : 4;
  }

  public static int Tags()
  {
    SceneObject hub = Hub;
    SceneObject spoke = Spoke;
    if (SceneObject.FindWithTag("probe-tag") != null) { return 2; }
    if (SceneObject.FindAllWithTag("probe-tag").Count != 0) { return 3; }

    hub.AddTag("probe-tag");
    if (!hub.HasTag("probe-tag") || !ReferenceEquals(SceneObject.FindWithTag("probe-tag"), hub)) { return 4; }
    spoke.AddTag("probe-tag");
    IReadOnlyList<SceneObject> tagged = SceneObject.FindAllWithTag("probe-tag");
    if (tagged.Count != 2 || !(ReferenceEquals(tagged[0], hub) || ReferenceEquals(tagged[1], hub))) { return 5; }

    hub.RemoveTag("probe-tag");
    if (hub.HasTag("probe-tag") || !ReferenceEquals(SceneObject.FindWithTag("probe-tag"), spoke)) { return 6; }
    return SceneObject.FindAllWithTag("probe-tag").Count == 1 ? 1 : 7;
  }

  public static int SetParentKeepingTheWorld()
  {
    SceneObject hub = Hub;
    SceneObject spoke = Spoke;
    if (!spoke.SetParent(hub)) { return 2; }
    if (!ReferenceEquals(spoke.Parent, hub)) { return 3; }
    /// it has not moved; what it is under has
    if (!Near(spoke.Transform.WorldPosition, 1f, 2f, 3f)) { return 4; }
    if (!Near(spoke.Transform.Position, -9f, 2f, 3f)) { return 5; }

    /// under itself, or under what is under it, is refused and changes nothing
    if (hub.SetParent(spoke) || hub.SetParent(hub)) { return 6; }
    if (hub.Parent != null) { return 7; }

    /// and back to the top of the scene
    if (!spoke.SetParent(null)) { return 8; }
    return spoke.Parent == null && Near(spoke.Transform.WorldPosition, 1f, 2f, 3f) && Near(spoke.Transform.Position, 1f, 2f, 3f) ? 1 : 9;
  }

  public static int SetParentKeepingItsPlaceUnderIt()
  {
    SceneObject hub = Hub;
    SceneObject spoke = Spoke;
    if (!spoke.SetParent(hub, false)) { return 2; }
    if (!Near(spoke.Transform.Position, 1f, 2f, 3f)) { return 3; }
    return Near(spoke.Transform.WorldPosition, 11f, 2f, 3f) ? 1 : 4;
  }

  public static int WorldPosition()
  {
    SceneObject hub = Hub;
    SceneObject spoke = Spoke;
    spoke.SetParent(hub);
    spoke.Transform.WorldPosition = new Vec3(5f, 6f, 7f);
    if (!Near(spoke.Transform.WorldPosition, 5f, 6f, 7f)) { return 2; }
    if (!Near(spoke.Transform.Position, -5f, 6f, 7f)) { return 3; }

    /// moving what it is under moves it
    hub.Transform.Position = new Vec3(20f, 0f, 0f);
    return Near(spoke.Transform.WorldPosition, 15f, 6f, 7f) ? 1 : 4;
  }

  /// asked with the hub turned a quarter of the way round its up axis and twice its size
  public static int WorldRotationAndScale()
  {
    SceneObject hub = Hub;
    SceneObject spoke = Spoke;
    spoke.SetParent(hub, false);

    Quaternion quarter = hub.Transform.WorldRotation;
    if (!Near(MathF.Abs(quarter.Y), 0.70710678f) || !Near(MathF.Abs(quarter.W), 0.70710678f)) { return 2; }
    /// not turned itself, it faces as what it is under faces
    Quaternion carried = spoke.Transform.WorldRotation;
    if (!Near(MathF.Abs(carried.Y), 0.70710678f) || !Near(MathF.Abs(carried.W), 0.70710678f)) { return 3; }

    /// facing as the world does takes the turn back out
    spoke.Transform.WorldRotation = Quaternion.Identity;
    Quaternion world = spoke.Transform.WorldRotation;
    if (!Near(MathF.Abs(world.W), 1f) || !Near(world.Y, 0f)) { return 4; }
    Quaternion own = spoke.Transform.Rotation;
    if (!Near(MathF.Abs(own.Y), 0.70710678f) || !Near(MathF.Abs(own.W), 0.70710678f)) { return 5; }

    if (!Near(hub.Transform.WorldScale, 2f, 2f, 2f)) { return 6; }
    return Near(spoke.Transform.WorldScale, 2f, 2f, 2f) && Near(spoke.Transform.Scale, 1f, 1f, 1f) ? 1 : 7;
  }

  /// how many times the hub's door has been opened, after opening it from here
  public static int OpenTheHubsDoor()
  {
    SceneObject.Find("Hub")?.GetBehavior<DoorBehavior>()?.Open();
    DoorBehavior door = SceneObject.Find("Hub")?.GetBehavior<DoorBehavior>();
    return door != null ? door.opened : -1;
  }

  public static int ADestroyedObjectIsKnown()
  {
    SceneObject doomed = SceneObject.Create("Doomed");
    SceneObject under = SceneObject.Create("DoomedChild", doomed);
    doomed.Destroy();
    if (doomed.IsAlive || under.IsAlive) { return 2; }
    if (SceneObject.Find("Doomed") != null || SceneObject.Find("DoomedChild") != null) { return 3; }
    try
    {
      string name = doomed.Name;
      return 4;
    }
    catch (DestroyedObjectException)
    {
    }
    /// and one made afterwards is not mistaken for it
    SceneObject next = SceneObject.Create("Next");
    return next.IsAlive && !doomed.IsAlive && !ReferenceEquals(next, doomed) ? 1 : 5;
  }

  public static int ComponentsAreKept()
  {
    SceneObject hub = Hub;
    if (!ReferenceEquals(hub.GetComponent<Transform>(), hub.Transform)) { return 2; }
    if (hub.HasComponent<GridComponent>()) { return 3; }
    hub.AddComponent<GridComponent>();
    GridComponent grid = hub.GetComponent<GridComponent>();
    if (!ReferenceEquals(grid, hub.GetComponent<GridComponent>())) { return 4; }
    hub.RemoveComponent<GridComponent>();
    return hub.HasComponent<GridComponent>() ? 5 : 1;
  }

  public static int HoldTheSpoke()
  {
    held = Spoke;
    return held != null ? 1 : 0;
  }

  /// after the scene has played and stopped, which gives every object another id
  public static int TheHeldSpokeIsStillTheSpoke()
  {
    if (held == null || !held.IsAlive) { return 2; }
    if (!ReferenceEquals(SceneObject.Find("Spoke"), held)) { return 3; }
    return held.Name == "Spoke" && Near(held.Transform.WorldPosition, 1f, 2f, 3f) ? 1 : 4;
  }

  /// the low bits of the id the held object has now
  public static int HeldId()
  {
    return held == null ? -1 : (int)(held.ObjectID & 0x7fffffff);
  }
}
