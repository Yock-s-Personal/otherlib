using System;
using OtherCsBindings;

namespace Other
{
  public static class Scene
  {
    private const int kMaxObjectQueryCount = 4096;

    [NativeFunction("SceneCreateObject")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, float, float, float, UInt64> NativeCreateObject;
    [NativeFunction("SceneDestroyObject")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, void> NativeDestroyObject;
    [NativeFunction("SceneHasObject")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, NativeBool32> NativeHasObject;
    [NativeFunction("SceneGetObjectName")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, byte*> NativeGetObjectName;
    [NativeFunction("SceneSetObjectName")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, byte*, void> NativeSetObjectName;
    [NativeFunction("SceneGetObjectIds")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64*, int*, int, void> NativeGetObjectIds;
    [NativeFunction("SceneGetObjectCount")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64> NativeGetObjectCount;
    [NativeFunction("SceneFindObjectByName")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, UInt64> NativeFindObjectByName;
    
    
    [NativeFunction("SceneGetParentId")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, UInt64> NativeGetParentId;
    [NativeFunction("SceneGetChildrenIds")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, UInt64*, int*, int, void> NativeGetChildrenIds;

    [NativeFunction("SceneObjectHasTag")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, byte*, NativeBool32> NativeObjectHasTag;
    [NativeFunction("SceneAddObjectTag")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, byte*, void> NativeAddObjectTag;
    [NativeFunction("SceneRemoveObjectTag")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, byte*, void> NativeRemoveObjectTag;

    [NativeFunction("SceneGetObjectVisible")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, NativeBool32> NativeGetObjectVisible;
    [NativeFunction("SceneSetObjectVisible")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, NativeBool32, void> NativeSetObjectVisible;

    public static ulong CreateObject(string name, Vec3 position = default)
    {
      unsafe
      {
        using var name_utf8 = new Utf8Arg(name, stackalloc byte[Utf8Arg.StackSize]);
        return NativeCreateObject(name_utf8.Pointer, position.X, position.Y, position.Z);
      }
    }

    public static void DestroyObject(ulong id)
    {
      unsafe { NativeDestroyObject(id); }
    }

    public static bool HasObject(ulong id)
    {
      unsafe { return NativeHasObject(id); }
    }

    public static string GetObjectName(ulong id)
    {
      unsafe
      {
        return Utf8Arg.Read(NativeGetObjectName(id));
      }
    }

    public static void SetObjectName(ulong id, string name)
    {
      unsafe
      {
        using var name_utf8 = new Utf8Arg(name, stackalloc byte[Utf8Arg.StackSize]);
        NativeSetObjectName(id, name_utf8.Pointer);
      }
    }

    public static ulong GetObjectCount()
    {
      unsafe { return NativeGetObjectCount(); }
    }

    public static ulong FindObjectByName(string name)
    {
      unsafe
      {
        using var name_utf8 = new Utf8Arg(name, stackalloc byte[Utf8Arg.StackSize]);
        return NativeFindObjectByName(name_utf8.Pointer);
      }
    }

        public static ulong[] GetAllObjectIds()
    {
      unsafe
      {
        UInt64* buffer = stackalloc UInt64[kMaxObjectQueryCount];
        int count = 0;
        NativeGetObjectIds(buffer, &count, kMaxObjectQueryCount);
        ulong[] result = new ulong[count];
        for (int i = 0; i < count; i++)
          result[i] = buffer[i];
        return result;
      }
    }

    public static ulong GetParentId(ulong id)
    {
      unsafe { return NativeGetParentId(id); }
    }

    public static ulong[] GetChildrenIds(ulong id)
    {
      unsafe
      {
        UInt64* buffer = stackalloc UInt64[kMaxObjectQueryCount];
        int count = 0;
        NativeGetChildrenIds(id, buffer, &count, kMaxObjectQueryCount);
        ulong[] result = new ulong[count];
        for (int i = 0; i < count; i++)
          result[i] = buffer[i];
        return result;
      }
    }

    // -- Tags

    public static bool ObjectHasTag(ulong id, string tag)
    {
      unsafe
      {
        using var tag_utf8 = new Utf8Arg(tag, stackalloc byte[Utf8Arg.StackSize]);
        return NativeObjectHasTag(id, tag_utf8.Pointer);
      }
    }
    public static void AddObjectTag(ulong id, string tag)
    {
      unsafe
      {
        using var tag_utf8 = new Utf8Arg(tag, stackalloc byte[Utf8Arg.StackSize]);
        NativeAddObjectTag(id, tag_utf8.Pointer);
      }
    }
    public static void RemoveObjectTag(ulong id, string tag)
    {
      unsafe
      {
        using var tag_utf8 = new Utf8Arg(tag, stackalloc byte[Utf8Arg.StackSize]);
        NativeRemoveObjectTag(id, tag_utf8.Pointer);
      }
    }

    // -- Visibility

    public static bool GetObjectVisible(ulong id) { unsafe { return NativeGetObjectVisible(id); } }
    public static void SetObjectVisible(ulong id, bool visible) { unsafe { NativeSetObjectVisible(id, visible); } }
  }
}