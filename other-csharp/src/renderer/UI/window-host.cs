using OtherCsBindings;
using System;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Other.UI
{
  /// keeps the windows declared with [EditorWindow]. each is one instance here and one
  ///  native window in the driver's registry; the native window owns everything about it
  ///  but its body, which it asks this class to draw
  internal static class WindowHost
  {
    private sealed class Entry
    {
      public EditorWindowAttribute declared = null!;
      public Assembly owner = null!;
      public UIWindow window = null!;
      /// 0 while the window is not in a native registry
      public ulong id;
    }

    private static readonly List<Entry> windows = new();
    private static readonly Dictionary<ulong, Entry> registered = new();
    /// names that were open when their assembly went: opened again if it comes back with them
    private static readonly HashSet<string> reopen = new();
    private static bool attached;
    private static bool watching_unloads;

    /// called by the engine, by name, for an assembly whose windows it hosts: every class of
    ///  it that is declared a window gets one instance. assemblies are loaded before the UI
    ///  exists, so the windows wait here until Attach
    internal static void AssemblyLoaded(int assembly_id)
    {
      Assembly? assembly = ScriptAssemblies.Find(assembly_id);
      if (assembly == null)
      {
        Core.Logger.LogError($"No windows were looked for in assembly {assembly_id}: it is not loaded.");
        return;
      }

      if (!watching_unloads)
      {
        ScriptAssemblies.Unloading += AssemblyUnloading;
        watching_unloads = true;
      }

      foreach (Type type in TypesOf(assembly))
      {
        var declared = type.GetCustomAttribute<EditorWindowAttribute>();
        if (declared == null)
        {
          continue;
        }

        Entry? entry = Make(type, declared, assembly);
        if (entry == null)
        {
          continue;
        }

        windows.Add(entry);
        if (attached)
        {
          Register(entry);
        }
      }
    }

    /// called by the engine, by name, once there is a registry for windows to go in
    internal static void Attach()
    {
      attached = true;
      unsafe
      {
        Windows.NativeSetWindowDrawFunction((delegate* unmanaged[Cdecl]<ulong, uint>)&Draw);
      }
      foreach (Entry entry in windows)
      {
        Register(entry);
      }
    }

    /// called by the engine, by name, before that registry goes
    internal static void Detach()
    {
      foreach (Entry entry in windows)
      {
        Unregister(entry);
      }
      reopen.Clear();
      attached = false;
      Menus.Detach();
    }

    private static Type[] TypesOf(Assembly assembly)
    {
      try
      {
        return assembly.GetTypes();
      }
      catch (ReflectionTypeLoadException e)
      {
        Core.Logger.LogError($"Some types of '{assembly.GetName().Name}' could not be loaded, so its windows may be missing: {e.Message}");
        return Array.FindAll(e.Types, type => type != null)!;
      }
    }

    private static Entry? Make(Type type, EditorWindowAttribute declared, Assembly assembly)
    {
      if (type.IsAbstract || !typeof(UIWindow).IsAssignableFrom(type))
      {
        Core.Logger.LogError($"'{type.FullName}' is declared the window '{declared.Name}' but is not a class that derives UIWindow.");
        return null;
      }
      if (windows.Exists(other => other.declared.Name == declared.Name))
      {
        Core.Logger.LogError($"'{type.FullName}' is declared the window '{declared.Name}', which another class already is.");
        return null;
      }

      try
      {
        var window = (UIWindow)Activator.CreateInstance(type)!;
        window.Name = declared.Name;
        return new Entry { declared = declared, owner = assembly, window = window };
      }
      catch (Exception e)
      {
        Core.Logger.LogError($"The window '{declared.Name}' could not be made from '{type.FullName}', which needs a constructor that takes nothing: {e.Message}");
        return null;
      }
    }

    private static void Register(Entry entry)
    {
      string name = entry.declared.Name;
      string title = entry.declared.Title.Length > 0 ? entry.declared.Title : name;
      unsafe
      {
        using var name_utf8 = new Utf8Arg(name, stackalloc byte[Utf8Arg.StackSize]);
        using var title_utf8 = new Utf8Arg(title, stackalloc byte[Utf8Arg.StackSize]);
        entry.id = Windows.NativeRegisterWindow(name_utf8.Pointer, title_utf8.Pointer, entry.declared.DefaultWidth, entry.declared.DefaultHeight, (int)entry.declared.Flags);
      }
      if (entry.id == 0)
      {
        Core.Logger.LogError($"The window '{name}' declared by '{entry.window.GetType().FullName}' was not registered: a window of that name already is.");
        return;
      }

      registered[entry.id] = entry;
      try
      {
        entry.window.Initialize();
      }
      catch (Exception e)
      {
        Core.Logger.LogError($"The window '{name}' threw in OnInitialize: {e}");
      }

      if (reopen.Remove(name))
      {
        Windows.Open(name);
      }
    }

    private static void Unregister(Entry entry)
    {
      if (entry.id == 0)
      {
        return;
      }

      try
      {
        entry.window.Shutdown();
      }
      catch (Exception e)
      {
        Core.Logger.LogError($"The window '{entry.declared.Name}' threw in OnShutdown: {e}");
      }

      unsafe { Windows.NativeUnregisterWindow(entry.id); }
      registered.Remove(entry.id);
      entry.id = 0;
    }

    /// a window is code of its assembly: it cannot outlive it
    private static void AssemblyUnloading(Assembly assembly)
    {
      for (int i = windows.Count - 1; i >= 0; i--)
      {
        Entry entry = windows[i];
        if (entry.owner != assembly)
        {
          continue;
        }

        if (entry.id != 0 && Windows.IsOpen(entry.declared.Name))
        {
          reopen.Add(entry.declared.Name);
        }
        Unregister(entry);
        windows.RemoveAt(i);
      }

      if (assembly == typeof(WindowHost).Assembly)
      {
        /// this code is going too: the engine must not call into it again
        unsafe { Windows.NativeSetWindowDrawFunction(null); }
        ScriptAssemblies.Unloading -= AssemblyUnloading;
        watching_unloads = false;
        attached = false;
      }
    }

    /// what a native window calls for its body, only while it is open and on screen. 0 when
    ///  the window threw: the engine closes what it left open and does not ask it again
    ///  until someone says to, so this is said once
    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    private static uint Draw(ulong id)
    {
      if (!registered.TryGetValue(id, out Entry? entry))
      {
        return 0;
      }

      try
      {
        entry.window.Draw();
        return 1;
      }
      catch (Exception e)
      {
        Core.Logger.LogError($"The window '{entry.declared.Name}' threw while drawing and is stopped until it is asked to draw again: {e}");
        return 0;
      }
    }
  }
}
