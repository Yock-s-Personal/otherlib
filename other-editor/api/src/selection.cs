using System;
using System.Collections.Generic;
using OtherCsBindings;

namespace Other.Editor
{
  /// what is selected in the editor. one selection for the whole editor: the hierarchy sets
  ///  it, the object editor shows it, the viewport draws it
  public static unsafe class Selection
  {
    [NativeFunction("EditorSelectionRevision")]
    internal static delegate* unmanaged[Cdecl]<ulong> NativeRevision;
    [NativeFunction("EditorSelectionActive")]
    internal static delegate* unmanaged[Cdecl]<ulong> NativeActive;
    [NativeFunction("EditorSelectionGet")]
    internal static delegate* unmanaged[Cdecl]<ulong*, int*, int, void> NativeGet;
    [NativeFunction("EditorSelectionSet")]
    internal static delegate* unmanaged[Cdecl]<ulong, void> NativeSet;
    [NativeFunction("EditorSelectionAdd")]
    internal static delegate* unmanaged[Cdecl]<ulong, void> NativeAdd;
    [NativeFunction("EditorSelectionRemove")]
    internal static delegate* unmanaged[Cdecl]<ulong, void> NativeRemove;
    [NativeFunction("EditorSelectionClear")]
    internal static delegate* unmanaged[Cdecl]<void> NativeClear;

    private const int kMaxSelected = 4096;

    private static readonly List<SceneObject> objects = new();
    private static ulong read_revision = ulong.MaxValue;

    /// goes up whenever what is selected changes, whoever changed it
    public static ulong Revision => NativeRevision();

    /// the object the object editor shows: the one selected last. null when nothing is
    public static SceneObject? Active => SceneObject.FromId(NativeActive());

    /// everything selected, the active object last. the same list each time, read again
    ///  from the editor only when the selection has changed: not to be kept across frames
    ///  by one who changes the selection meanwhile
    public static IReadOnlyList<SceneObject> Objects
    {
      get
      {
        ulong revision = NativeRevision();
        if (revision != read_revision)
        {
          read_revision = revision;
          objects.Clear();
          ulong* ids = stackalloc ulong[kMaxSelected];
          int count = 0;
          NativeGet(ids, &count, kMaxSelected);
          for (int i = 0; i < count; i++)
          {
            SceneObject? one = SceneObject.FromId(ids[i]);
            if (one != null)
            {
              objects.Add(one);
            }
          }
        }
        return objects;
      }
    }

    public static int Count => Objects.Count;

    public static bool Contains(SceneObject item)
    {
      IReadOnlyList<SceneObject> selected = Objects;
      for (int i = 0; i < selected.Count; i++)
      {
        if (ReferenceEquals(selected[i], item))
        {
          return true;
        }
      }
      return false;
    }

    /// this object and nothing else; null for nothing at all
    public static void Set(SceneObject? item)
    {
      if (item == null || !item.IsAlive)
      {
        NativeClear();
        return;
      }
      NativeSet(item.ObjectID);
    }

    /// this object as well as what is selected. it becomes the active one
    public static void Add(SceneObject item)
    {
      if (item != null && item.IsAlive)
      {
        NativeAdd(item.ObjectID);
      }
    }

    public static void Remove(SceneObject item)
    {
      if (item != null && item.IsAlive)
      {
        NativeRemove(item.ObjectID);
      }
    }

    public static void Clear()
    {
      NativeClear();
    }
  }
}
