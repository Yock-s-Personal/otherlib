using System;
using System.Collections.Generic;

namespace Other.Editor
{
  /// one object as a list of the scene shows it
  public sealed class SceneViewItem
  {
    public SceneObject Object { get; internal set; } = null!;
    public string Name { get; internal set; } = "";
    public bool Visible { get; internal set; }
    public SceneViewItem? Parent { get; internal set; }
    /// how far under the top of the scene it is; 0 for an object at the top
    public int Depth { get; internal set; }
    /// a number for its place in the tree, made of the names from the top down to it. an
    ///  undo or a stop makes every object again under another id, and this stays what it
    ///  was: it is what a window keeps something about an object by (whether it is open).
    ///  two objects of one name under one parent share it
    public ulong PathKey { get; internal set; }

    internal readonly List<SceneViewItem> children = new();
    public IReadOnlyList<SceneViewItem> Children => children;
  }

  /// the scene's objects as a window draws them, kept. asking the engine for every object's
  ///  name and children makes a string and an array each time, which for a window that draws
  ///  every frame is a great deal of garbage; this asks once and again only when the scene
  ///  says it has changed (Scene.Revision)
  public static class SceneView
  {
    private static readonly List<SceneViewItem> top = new();
    private static readonly List<SceneViewItem> all = new();
    private static ulong read_revision = ulong.MaxValue;
    private static string scene_name = "";

    /// goes up when what is kept here was read again
    public static ulong Revision
    {
      get
      {
        Refresh();
        return read_revision;
      }
    }

    /// empty while there is no scene
    public static string SceneName
    {
      get
      {
        Refresh();
        return scene_name;
      }
    }

    /// the objects at the top of the scene, in its order
    public static IReadOnlyList<SceneViewItem> Top
    {
      get
      {
        Refresh();
        return top;
      }
    }

    /// every object, a parent before what is under it
    public static IReadOnlyList<SceneViewItem> All
    {
      get
      {
        Refresh();
        return all;
      }
    }

    public static SceneViewItem? Find(SceneObject item)
    {
      Refresh();
      for (int i = 0; i < all.Count; i++)
      {
        if (ReferenceEquals(all[i].Object, item))
        {
          return all[i];
        }
      }
      return null;
    }

    private static void Refresh()
    {
      ulong revision = Scene.Revision;
      if (revision == read_revision)
      {
        return;
      }
      read_revision = revision;
      top.Clear();
      all.Clear();
      scene_name = revision == 0 ? "" : Scene.Name;
      if (revision == 0)
      {
        return;
      }

      foreach (ulong id in Scene.GetTopObjectIds())
      {
        SceneViewItem? item = Read(id, null, 0);
        if (item != null)
        {
          top.Add(item);
        }
      }
    }

    /// FNV-1a over the name, begun from the parent's key
    private static ulong PathKeyOf(SceneViewItem? parent, string name)
    {
      ulong hash = parent != null ? parent.PathKey ^ 0x9E3779B97F4A7C15UL : 14695981039346656037UL;
      foreach (char c in name)
      {
        hash = (hash ^ c) * 1099511628211UL;
      }
      return hash;
    }

    private static SceneViewItem? Read(ulong id, SceneViewItem? parent, int depth)
    {
      SceneObject? found = SceneObject.FromId(id);
      if (found == null)
      {
        return null;
      }

      var item = new SceneViewItem
      {
        Object = found,
        Name = Scene.GetObjectName(id),
        Visible = Scene.GetObjectVisible(id),
        Parent = parent,
        Depth = depth,
      };
      item.PathKey = PathKeyOf(parent, item.Name);
      all.Add(item);
      foreach (ulong child_id in Scene.GetChildrenIds(id))
      {
        SceneViewItem? child = Read(child_id, item, depth + 1);
        if (child != null)
        {
          item.children.Add(child);
        }
      }
      return item;
    }
  }
}
