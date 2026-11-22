using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using OtherCsBindings;

namespace Other.Core
{
  public abstract class OtherObject
  {
    public enum ObjectType
    {
      SceneObject = 0,
      // EditorObject, UIObject, etc...
    }

    private ObjectType object_type;
    public ObjectType Type => object_type;

    private struct InternalHandles
    {
      public UInt64 object_id;
      public IntPtr native_handle;
    }
    private InternalHandles internal_handles;
    public IntPtr NativeHandle => internal_handles.native_handle;


    [NativeFunction("GetObjectID")]
    internal static unsafe delegate* unmanaged[Cdecl]<nint, UInt64*, void> NativeGetObjectID;

    public UInt64 ObjectID
    {
      get {
        if (internal_handles.native_handle == IntPtr.Zero)
        {
          return 0;
        }
        if (internal_handles.object_id != 0)
        {
          return internal_handles.object_id;
        }
        unsafe { 
          if (NativeGetObjectID == null)
          {
            throw new InvalidOperationException("Native function GetObjectID is not initialized.");
          } 
          UInt64 ret = 0;
          NativeGetObjectID(internal_handles.native_handle, &ret);
          return ret;
        }
      }
    }

    private List<OtherBehavior> behaviors;
    public int BehaviorCount => behaviors.Count;

    public OtherObject(IntPtr native_handle, ObjectType type)
    {
      object_type = type;

      behaviors = new List<OtherBehavior>();
      internal_handles = new InternalHandles
      {
        object_id = 0,
        native_handle = native_handle
      };
      internal_handles.object_id = ObjectID;
    }

    /// which script object the engine knows this object as. -1 while it knows it as none
    internal long script_object_id = -1;

    /// the engine's record of an object's behaviours is the one that counts. these two are
    ///  the host keeping this list the same as it
    internal void HostAdd(OtherBehavior behavior)
    {
      if (!behaviors.Contains(behavior))
      {
        behaviors.Add(behavior);
      }
    }

    internal void HostRemove(OtherBehavior behavior)
    {
      behaviors.Remove(behavior);
    }

    /// the behaviour of that type on this object, added if it has none: there is one of a
    ///  type on an object. null when the engine could not make it
    public T AddBehavior<T>() where T : OtherBehavior
    {
      T existing = GetBehavior<T>();
      return existing != null ? existing : BehaviorHost.Add(this, typeof(T)) as T;
    }

    /// false when this object has no behaviour of that type
    public bool RemoveBehavior<T>() where T : OtherBehavior
    {
      T existing = GetBehavior<T>();
      return existing != null && BehaviorHost.Remove(this, existing.engine_name);
    }

    /// play/stop keeps managed instances alive while the native scene rebuilds; the native id
    /// is reset at the end of the disable pass and rebound once the restored object exists
    public void ResetNativeHandle()
    {
      internal_handles.object_id = 0;
      internal_handles.native_handle = IntPtr.Zero;
    }

    public void RebindNativeHandle(IntPtr native_handle)
    {
      internal_handles.native_handle = native_handle;
      internal_handles.object_id = 0;
      internal_handles.object_id = ObjectID;
      OnNativeRebind();
    }

    protected virtual void OnNativeRebind() {}

    public T GetBehavior<T>() where T : OtherBehavior
    {
      for (int i = 0; i < behaviors.Count; i++)
      {
        if (behaviors[i] is T typed)
        {
          return typed;
        }
      }
      return null;
    }

    public bool HasBehavior<T>() where T : OtherBehavior
    {
      for (int i = 0; i < behaviors.Count; i++)
      {
        if (behaviors[i] is T)
        {
          return true;
        }
      }
      return false;
    }

    public bool HasBehavior(string class_name)
    {
      for (int i = 0; i < behaviors.Count; i++)
      {
        if (behaviors[i].GetType().Name == class_name || behaviors[i].GetType().FullName == class_name)
        {
          return true;
        }
      }
      return false;
    }

    public string[] GetBehaviorTypeNames()
    {
      string[] names = new string[behaviors.Count];
      for (int i = 0; i < behaviors.Count; i++)
      {
        names[i] = behaviors[i].GetType().FullName;
      }
      return names;
    }

  }
}