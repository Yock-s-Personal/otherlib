using System;
using Other.Core;

namespace Other.Core
{
  /// contact data for collision/trigger callbacks; Point and Normal are world space and the
  /// normal points from this object toward the other. exits carry no point/normal
  public struct CollisionInfo
  {
    public UInt64 OtherObjectId;
    public Vec3 Point;
    public Vec3 Normal;

    public CollisionInfo(UInt64 other_id, Vec3 point, Vec3 normal)
    {
      OtherObjectId = other_id;
      Point = point;
      Normal = normal;
    }
  }

  /// a script on an object. the engine keeps its life and calls what is written here: a
  ///  callback that is left as it is costs nothing
  public abstract class OtherBehavior
  {
  #nullable enable
    OtherObject? parent_object = null;
  #nullable disable

    /// what the engine calls this behaviour, told when it joins its object
    internal string engine_name = "";
    /// the engine's switch, kept here to be read. changed through Enabled
    internal bool enabled = true;

    public UInt64 ObjectID
    {
      get
      {
        if (parent_object == null)
        {
          return 0;
        }
        return parent_object!.ObjectID;
      }
    }

    /// false stops this behaviour's updates, what physics tells it and its render callback,
    ///  until it is true again. a behaviour runs while its scene plays and this is true:
    ///  Enable and Disable are called as that starts and stops
    public bool Enabled
    {
      get => enabled;
      set
      {
        if (enabled == value)
        {
          return;
        }
        /// the engine sets the field back through the host; one that is on no object has
        ///  nobody to ask
        if (parent_object == null || !BehaviorHost.SetEnabled(parent_object, engine_name, value))
        {
          enabled = value;
        }
      }
    }

    public OtherObject ParentObject => parent_object;

    internal void Bind(OtherObject obj)
    {
      parent_object = obj;
    }

    /// the host's way in: the callbacks themselves are for the behaviour's own class
    internal void CallAwake() { Awake(); }
    internal void CallEnable() { Enable(); }
    internal void CallStart() { Start(); }
    internal void CallFixedUpdate() { FixedUpdate(); }
    internal void CallUpdate() { Update(); }
    internal void CallLateUpdate() { LateUpdate(); }
    internal void CallRender() { Render(); }
    internal void CallCollisionEnter(CollisionInfo info) { CollisionEnter(info); }
    internal void CallCollisionExit(CollisionInfo info) { CollisionExit(info); }
    internal void CallTriggerEnter(CollisionInfo info) { TriggerEnter(info); }
    internal void CallTriggerExit(CollisionInfo info) { TriggerExit(info); }
    internal void CallJointBreak(float force) { JointBreak(force); }
    internal void CallDisable() { Disable(); }
    internal void CallDestroy() { Destroy(); }

    /// once, when the behaviour is made: after its scene is loaded whole, or when it is added
    protected virtual void Awake() {}
    /// when it begins running, and when it stops: its scene plays or stops, or Enabled changes
    protected virtual void Enable() {}
    protected virtual void Disable() {}
    /// once after it begins running, before the first update it gets
    protected virtual void Start() {}
    /// while it runs
    protected virtual void Update() {}
    protected virtual void LateUpdate() {}
    protected virtual void FixedUpdate() {}
    /// when it goes: it was removed, its object was destroyed or its scene was unloaded
    protected virtual void Destroy() {}

    /// physics callbacks, while it runs. an exit is not guaranteed if either body was
    /// destroyed the same tick
    protected virtual void CollisionEnter(CollisionInfo info) {}
    protected virtual void CollisionExit(CollisionInfo info) {}
    protected virtual void TriggerEnter(CollisionInfo info) {}
    protected virtual void TriggerExit(CollisionInfo info) {}
    protected virtual void JointBreak(float force) {}
    /// per-frame draw hook, called while Enabled is true whether or not the scene is playing
    protected virtual void Render() {}
  }
}