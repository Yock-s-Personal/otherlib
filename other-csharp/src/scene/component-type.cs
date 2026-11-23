using System;
using System.Collections.Generic;
using OtherCsBindings;

namespace Other
{
  /// one kind of component an object can have, known by its id and not by a class written
  ///  for it: what an inspector lists, adds and removes. a component's fields are still
  ///  reached through its class (SceneObject.GetComponent)
  public sealed class ComponentType
  {
    /// the same id Component.TypeId gives the component's class
    public ulong Id { get; }
    /// what an inspector calls it: "Graphics Object"
    public string Name { get; }
    /// false for what every object has and keeps: its transform, its scripts
    public bool Removable { get; }

    private ComponentType(ulong id, string name, bool removable)
    {
      Id = id;
      Name = name;
      Removable = removable;
    }

    private static List<ComponentType> all;

    /// every kind there is, in the order a list shows them. read from the engine once
    public static IReadOnlyList<ComponentType> All
    {
      get
      {
        if (all == null)
        {
          all = new List<ComponentType>();
          unsafe
          {
            int count = OtherABI.NativeComponentTypeCount();
            for (int i = 0; i < count; i++)
            {
              ulong id = 0;
              NativeBool32 removable = false;
              string name = Utf8Arg.Read(OtherABI.NativeComponentTypeAt(i, &id, &removable));
              if (id != 0)
              {
                all.Add(new ComponentType(id, name, removable));
              }
            }
          }
        }
        return all;
      }
    }

    /// null when there is no component of that id
    public static ComponentType Find(ulong id)
    {
      IReadOnlyList<ComponentType> types = All;
      for (int i = 0; i < types.Count; i++)
      {
        if (types[i].Id == id)
        {
          return types[i];
        }
      }
      return null;
    }

    public static ComponentType Of<T>() where T : Component => Find(Component.TypeId<T>());

    public bool IsOn(SceneObject item)
    {
      unsafe { return item != null && item.IsAlive && OtherABI.NativeHasComponent(item.ObjectID, Id); }
    }

    /// false when the object already has one, or is gone
    public bool AddTo(SceneObject item)
    {
      if (item == null || !item.IsAlive || IsOn(item))
      {
        return false;
      }
      unsafe { return OtherABI.NativeAddComponent(item.ObjectID, Id); }
    }

    /// false when the object has none, is gone, or this is a kind an object keeps
    public bool RemoveFrom(SceneObject item)
    {
      if (item == null || !item.IsAlive || !Removable || !IsOn(item))
      {
        return false;
      }
      unsafe { return OtherABI.NativeRemoveComponent(item.ObjectID, Id); }
    }

    /// the kinds the object has, in the order a list shows them, put into found (which is
    ///  emptied first) so that one who asks every frame makes no list each time
    public static void On(SceneObject item, List<ComponentType> found)
    {
      found.Clear();
      IReadOnlyList<ComponentType> types = All;
      for (int i = 0; i < types.Count; i++)
      {
        if (types[i].IsOn(item))
        {
          found.Add(types[i]);
        }
      }
    }

    public override string ToString() => Name;
  }
}
