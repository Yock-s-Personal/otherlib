using OtherCsBindings;
using System;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Other.UI
{
  /// an item in the main menu bar. it is there until it is disposed, or the assembly its
  ///  handler was written in is unloaded
  public sealed class MenuItem : IDisposable
  {
    internal ulong id;

    internal MenuItem(ulong id)
    {
      this.id = id;
    }

    public void Dispose()
    {
      Menus.Remove(this);
    }
  }

  /// the driver's main menu bar, to add items to from C#
  public static class Menus
  {
    [NativeFunction("UIAddMenuItem")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, byte*, ulong> NativeAddMenuItem;
    [NativeFunction("UIRemoveMenuItem")]
    internal static unsafe delegate* unmanaged[Cdecl]<ulong, void> NativeRemoveMenuItem;
    [NativeFunction("UISetMenuItemClickFunction")]
    internal static unsafe delegate* unmanaged[Cdecl]<void*, void> NativeSetMenuItemClickFunction;

    private sealed class Entry
    {
      public string label = "";
      public Action on_click = null!;
      public Assembly? owner;
    }

    private static readonly Dictionary<ulong, Entry> items = new();
    private static bool watching_unloads;

    /// path names the menu and any menus inside it, separated by '/': "File/Project". a menu
    ///  that is not there yet is made. on_click is called on the main thread
    public static MenuItem Add(string path, string label, Action on_click)
    {
      if (!watching_unloads)
      {
        ScriptAssemblies.Unloading += DropItemsOf;
        watching_unloads = true;
      }

      ulong id;
      unsafe
      {
        NativeSetMenuItemClickFunction((delegate* unmanaged[Cdecl]<ulong, void>)&Clicked);
        using var path_utf8 = new Utf8Arg(path, stackalloc byte[Utf8Arg.StackSize]);
        using var label_utf8 = new Utf8Arg(label, stackalloc byte[Utf8Arg.StackSize]);
        id = NativeAddMenuItem(path_utf8.Pointer, label_utf8.Pointer);
      }
      if (id == 0)
      {
        Core.Logger.LogError($"The menu item '{label}' was not added to '{path}': there is no menu bar, or the path names no menu.");
        return new MenuItem(0);
      }

      items[id] = new Entry { label = label, on_click = on_click, owner = on_click.Method.DeclaringType?.Assembly };
      return new MenuItem(id);
    }

    internal static void Remove(MenuItem item)
    {
      if (item.id == 0)
      {
        return;
      }
      items.Remove(item.id);
      unsafe { NativeRemoveMenuItem(item.id); }
      item.id = 0;
    }

    /// the menu bar is going, and every item with it
    internal static void Detach()
    {
      items.Clear();
    }

    /// a handler is code of the assembly it was written in: when that assembly goes, so do
    ///  its items
    private static void DropItemsOf(Assembly assembly)
    {
      var gone = new List<ulong>();
      foreach (var entry in items)
      {
        if (entry.Value.owner == assembly)
        {
          gone.Add(entry.Key);
        }
      }
      foreach (ulong id in gone)
      {
        items.Remove(id);
        unsafe { NativeRemoveMenuItem(id); }
      }

      if (assembly == typeof(Menus).Assembly)
      {
        /// this code is going too: the engine must not call into it again
        unsafe { NativeSetMenuItemClickFunction(null); }
        ScriptAssemblies.Unloading -= DropItemsOf;
        watching_unloads = false;
      }
    }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    private static void Clicked(ulong id)
    {
      if (!items.TryGetValue(id, out Entry? entry))
      {
        return;
      }

      try
      {
        entry.on_click();
      }
      catch (Exception e)
      {
        Core.Logger.LogError($"The menu item '{entry.label}' threw when it was clicked: {e}");
      }
    }
  }
}
