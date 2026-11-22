using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using OtherCsBindings;

namespace Other
{
  /// an object of the scene, and the one way a script holds one. the engine makes one for
  ///  each object; whatever finds or creates an object hands back that one, so two
  ///  references to an object are the same reference
  public sealed class SceneObject : Core.OtherObject
  {
    [NativeFunction("GetWorldMatrix")]
    unsafe private static delegate* unmanaged[Cdecl]<UInt64, float*, void> NativeGetWorldMatrix;
    [NativeFunction("SceneObjectInstance")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, void*> NativeObjectInstance;
    [NativeFunction("SceneSetParent")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, UInt64, NativeBool32, NativeBool32> NativeSetParent;
    [NativeFunction("SceneFindObjectWithTag")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, UInt64> NativeFindObjectWithTag;
    [NativeFunction("SceneFindObjectsWithTag")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, UInt64*, int*, int, void> NativeFindObjectsWithTag;

    private Transform transform;
    /// one wrapper for each kind of component, made when it is first asked for
    private Dictionary<Type, Component> components;

    public SceneObject(IntPtr native_handle)
      : base(native_handle, ObjectType.SceneObject)
    {
    }

    /// the object the engine knows by that id; null when it knows none
    internal static SceneObject FromId(ulong id)
    {
      if (id == 0)
      {
        return null;
      }
      unsafe
      {
        void* instance = NativeObjectInstance(id);
        return instance == null ? null : GCHandle.FromIntPtr((IntPtr)instance).Target as SceneObject;
      }
    }

    /// a new object, under parent when one is given and at the top of the scene otherwise
    public static SceneObject Create(string name, SceneObject parent = null)
    {
      SceneObject made = Scene.CreateObject(name);
      if (made != null && parent != null)
      {
        made.SetParent(parent, false);
      }
      return made;
    }

    /// null when the scene has no object of that name
    public static SceneObject Find(string name) => Scene.FindObjectByName(name);

    /// one object that has the tag; null when none has
    public static SceneObject FindWithTag(string tag)
    {
      unsafe
      {
        using var tag_utf8 = new Utf8Arg(tag, stackalloc byte[Utf8Arg.StackSize]);
        return FromId(NativeFindObjectWithTag(tag_utf8.Pointer));
      }
    }

    public static IReadOnlyList<SceneObject> FindAllWithTag(string tag)
    {
      unsafe
      {
        UInt64* ids = stackalloc UInt64[Scene.kMaxObjectQueryCount];
        int count = 0;
        using var tag_utf8 = new Utf8Arg(tag, stackalloc byte[Utf8Arg.StackSize]);
        NativeFindObjectsWithTag(tag_utf8.Pointer, ids, &count, Scene.kMaxObjectQueryCount);
        return FromIds(ids, count);
      }
    }

    internal static unsafe List<SceneObject> FromIds(UInt64* ids, int count)
    {
      var found = new List<SceneObject>(count);
      for (int i = 0; i < count; i++)
      {
        SceneObject one = FromId(ids[i]);
        if (one != null)
        {
          found.Add(one);
        }
      }
      return found;
    }

    /// false once the object has been destroyed. a reference to it can still be held, and
    ///  asking it anything else throws
    public bool IsAlive => ObjectID != 0;

    /// the id the engine knows the object by, for what has to be asked of it
    private ulong Live()
    {
      ulong id = ObjectID;
      if (id == 0)
      {
        throw new DestroyedObjectException("The scene object has been destroyed. Check IsAlive before using an object that may be gone.");
      }
      return id;
    }

    public string Name
    {
      get => Scene.GetObjectName(Live());
      set => Scene.SetObjectName(Live(), value);
    }

    public bool Visible
    {
      get => Scene.GetObjectVisible(Live());
      set => Scene.SetObjectVisible(Live(), value);
    }

    /// null for an object at the top of the scene
    public SceneObject Parent => FromId(Scene.GetParentId(Live()));

    public IReadOnlyList<SceneObject> Children
    {
      get
      {
        unsafe
        {
          UInt64* ids = stackalloc UInt64[Scene.kMaxObjectQueryCount];
          int count = 0;
          Scene.NativeGetChildrenIds(Live(), ids, &count, Scene.kMaxObjectQueryCount);
          return FromIds(ids, count);
        }
      }
    }

    /// puts the object under another, or at the top of the scene for null. keep_world
    ///  leaves it where it is in the world; without it, where it was under its old parent
    ///  is where it is under the new one. false when the parent is the object itself or
    ///  one under it
    public bool SetParent(SceneObject parent, bool keep_world = true)
    {
      ulong parent_id = parent != null ? parent.Live() : 0;
      unsafe { return NativeSetParent(Live(), parent_id, keep_world); }
    }

    public Transform Transform
    {
      get
      {
        if (transform == null)
        {
          transform = new Transform(ObjectID);
          transform.BindOwner(this);
        }
        return transform;
      }
    }

    public bool HasTag(string tag) => Scene.ObjectHasTag(Live(), tag);
    public void AddTag(string tag) => Scene.AddObjectTag(Live(), tag);
    public void RemoveTag(string tag) => Scene.RemoveObjectTag(Live(), tag);

    /// the object, what is under it and their behaviours go. IsAlive is false afterwards
    public void Destroy()
    {
      Scene.DestroyObject(Live());
    }

    public Mat4 WorldMatrix
    {
      get
      {
        unsafe
        {
          float* ptr = stackalloc float[16];
          NativeGetWorldMatrix(Live(), ptr);
          return new Mat4(
            ptr[0], ptr[1], ptr[2], ptr[3],
            ptr[4], ptr[5], ptr[6], ptr[7],
            ptr[8], ptr[9], ptr[10], ptr[11],
            ptr[12], ptr[13], ptr[14], ptr[15]
          );
        }
      }
    }

    /// the same wrapper each time it is asked for, and one that follows the object
    public T GetComponent<T>()
      where T : Component
    {
      if (!HasComponent<T>())
      {
        throw new MissingComponentException($"SceneObject does not have component of type {typeof(T).Name}");
      }
      if (typeof(T) == typeof(Transform))
      {
        return (T)(Component)Transform;
      }

      components ??= new Dictionary<Type, Component>();
      if (!components.TryGetValue(typeof(T), out Component found))
      {
        found = (Activator.CreateInstance(typeof(T), ObjectID) as T)!;
        found.BindOwner(this);
        components[typeof(T)] = found;
      }
      return (T)found;
    }

    public bool HasComponent<T>()
      where T : Component
    {
      unsafe
      {
        return OtherABI.NativeHasComponent(Live(), Component.TypeId<T>());
      }
    }

    public void AddComponent<T>()
      where T : Component
    {
      unsafe
      {
        OtherABI.NativeAddComponent(Live(), Component.TypeId<T>());
      }
    }

    public void RemoveComponent<T>()
      where T : Component
    {
      unsafe
      {
        OtherABI.NativeRemoveComponent(Live(), Component.TypeId<T>());
      }
      components?.Remove(typeof(T));
    }

    public override string ToString() => IsAlive ? $"SceneObject({ObjectID}, \"{Name}\")" : "SceneObject(destroyed)";
  }
}
