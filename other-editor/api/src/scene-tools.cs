using System;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using OtherCsBindings;

namespace Other.Editor
{
  /// declares a class a tool of the Scene View. the class derives SceneTool and has a
  ///  constructor that takes nothing; there is one of it for as long as its assembly is
  ///  loaded
  [AttributeUsage(AttributeTargets.Class, Inherited = false)]
  public sealed class SceneToolAttribute : Attribute
  {
    /// what the tool is found by: "move"
    public string Name { get; }
    /// what a toolbar or a menu calls it. its name when empty
    public string Title { get; set; } = "";
    /// run every frame, whichever tool is the active one
    public bool Always { get; set; }
    /// tools are run from the lowest to the highest
    public int Order { get; set; }

    public SceneToolAttribute(string name)
    {
      Name = name;
    }
  }

  /// something that works in the Scene View: it is run while the view's window is drawn,
  ///  after its picture, so it may draw over the picture with DrawList, use any widget,
  ///  and ask SceneViewport where the pointer is and what is under it
  public abstract class SceneTool
  {
    public string Name { get; internal set; } = "";
    public string Title { get; internal set; } = "";
    public bool Always { get; internal set; }
    internal int Order;

    /// true once the tool threw: it is not run again until this is made false
    public bool Stopped { get; set; }

    /// called each frame the Scene View is on the screen, when this is the active tool or
    ///  one that always runs
    public abstract void OnSceneView();
  }

  /// the tools of the Scene View: every class declared with [SceneTool] in an assembly
  ///  written against this one
  public static unsafe class SceneTools
  {
    [NativeFunction("EditorSetSceneViewFunction")]
    internal static delegate* unmanaged[Cdecl]<void*, void> NativeSetSceneViewFunction;

    private sealed class Entry
    {
      public SceneTool tool = null!;
      public Assembly owner = null!;
    }

    private static readonly List<Entry> entries = new();
    private static readonly List<SceneTool> tools = new();
    /// loaded, and not yet looked into for tools: an assembly is looked into the next
    ///  time the tools are run, when the engine has finished loading it
    private static readonly List<Assembly> unread = new();
    private static SceneTool? active;
    /// the active tool went with its assembly: the one of this name is active when it is back
    private static string active_name = "";
    private static bool hosted;

    public static IReadOnlyList<SceneTool> All
    {
      get
      {
        ReadAssemblies();
        return tools;
      }
    }

    /// the one tool, of those that do not always run, that is run. null for none
    public static SceneTool? Active
    {
      get
      {
        ReadAssemblies();
        return active;
      }
      set
      {
        active = value;
        active_name = value?.Name ?? "";
      }
    }

    public static SceneTool? Find(string name)
    {
      ReadAssemblies();
      for (int i = 0; i < tools.Count; i++)
      {
        if (tools[i].Name == name)
        {
          return tools[i];
        }
      }
      return null;
    }

    /// called by the editor's commands when they are added, once: the tools are run from
    ///  then on
    internal static void Host()
    {
      if (hosted)
      {
        return;
      }
      hosted = true;
      unread.AddRange(ScriptAssemblies.All);
      ScriptAssemblies.Loaded += AssemblyLoaded;
      ScriptAssemblies.Unloading += AssemblyUnloading;
      NativeSetSceneViewFunction((delegate* unmanaged[Cdecl]<void>)&Run);
    }

    private static void AssemblyLoaded(Assembly assembly)
    {
      unread.Add(assembly);
    }

    /// a tool is code of its assembly: it cannot outlive it
    private static void AssemblyUnloading(Assembly assembly)
    {
      unread.Remove(assembly);
      for (int i = entries.Count - 1; i >= 0; i--)
      {
        if (entries[i].owner != assembly)
        {
          continue;
        }
        if (ReferenceEquals(entries[i].tool, active))
        {
          /// its name is kept, for the one that may come back
          active = null;
        }
        tools.Remove(entries[i].tool);
        entries.RemoveAt(i);
      }

      if (assembly == typeof(SceneTools).Assembly)
      {
        /// this code is going too: the editor must not call into it again
        NativeSetSceneViewFunction(null);
        ScriptAssemblies.Loaded -= AssemblyLoaded;
        ScriptAssemblies.Unloading -= AssemblyUnloading;
        unread.Clear();
        hosted = false;
      }
    }

    private static void ReadAssemblies()
    {
      if (unread.Count == 0)
      {
        return;
      }

      for (int i = 0; i < unread.Count; i++)
      {
        Read(unread[i]);
      }
      unread.Clear();
      entries.Sort((a, b) => a.tool.Order.CompareTo(b.tool.Order));
      tools.Clear();
      for (int i = 0; i < entries.Count; i++)
      {
        tools.Add(entries[i].tool);
      }
      if (active == null && active_name.Length > 0)
      {
        active = tools.Find(tool => tool.Name == active_name);
      }
    }

    private static void Read(Assembly assembly)
    {
      /// only an assembly written against this one can declare a tool
      string api = typeof(SceneTools).Assembly.GetName().Name!;
      if (assembly != typeof(SceneTools).Assembly && !Array.Exists(assembly.GetReferencedAssemblies(), name => name.Name == api))
      {
        return;
      }

      Type[] types;
      try
      {
        types = assembly.GetTypes();
      }
      catch (ReflectionTypeLoadException e)
      {
        Core.Logger.LogError($"Some types of '{assembly.GetName().Name}' could not be loaded, so its scene tools may be missing: {e.Message}");
        types = Array.FindAll(e.Types, type => type != null)!;
      }

      foreach (Type type in types)
      {
        var declared = type.GetCustomAttribute<SceneToolAttribute>();
        if (declared == null)
        {
          continue;
        }
        if (type.IsAbstract || !typeof(SceneTool).IsAssignableFrom(type))
        {
          Core.Logger.LogError($"'{type.FullName}' is declared the scene tool '{declared.Name}' but is not a class that derives SceneTool.");
          continue;
        }
        if (entries.Exists(other => other.tool.Name == declared.Name))
        {
          Core.Logger.LogError($"'{type.FullName}' is declared the scene tool '{declared.Name}', which another class already is.");
          continue;
        }

        try
        {
          var tool = (SceneTool)Activator.CreateInstance(type)!;
          tool.Name = declared.Name;
          tool.Title = declared.Title.Length > 0 ? declared.Title : declared.Name;
          tool.Always = declared.Always;
          tool.Order = declared.Order;
          entries.Add(new Entry { tool = tool, owner = assembly });
        }
        catch (Exception e)
        {
          Core.Logger.LogError($"The scene tool '{declared.Name}' could not be made from '{type.FullName}', which needs a constructor that takes nothing: {e.Message}");
        }
      }
    }

    /// what the Scene View's window calls once it has drawn its picture. a tool that
    ///  throws is stopped and said once, and the others are run all the same
    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    private static void Run()
    {
      try
      {
        ReadAssemblies();
      }
      catch (Exception e)
      {
        Core.Logger.LogError($"The scene tools could not be read: {e}");
        unread.Clear();
      }

      for (int i = 0; i < tools.Count; i++)
      {
        SceneTool tool = tools[i];
        if (tool.Stopped || !(tool.Always || ReferenceEquals(tool, active)))
        {
          continue;
        }

        try
        {
          tool.OnSceneView();
        }
        catch (Exception e)
        {
          tool.Stopped = true;
          Core.Logger.LogError($"The scene tool '{tool.Name}' threw and is stopped until its Stopped is made false: {e}");
        }
      }
    }
  }
}
