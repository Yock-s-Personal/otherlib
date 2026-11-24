using System;
using System.Collections.Generic;
using System.Reflection;

namespace OtherCsBindings
{
  /// the script assemblies the engine has loaded, for code above the loader
  public static class ScriptAssemblies
  {
    /// raised for a script assembly just before it is unloaded (project close, or the unload
    ///  half of a hot reload). whatever holds a delegate or a type of that assembly lets go of
    ///  it here: held, they keep the assembly in memory and keep calling its old code
    public static event Action<Assembly>? Unloading;

    /// raised for a script assembly once it is loaded, before the engine has bound the
    ///  native functions it declares: a handler notes the assembly and looks into it later
    public static event Action<Assembly>? Loaded;

    /// the script assemblies that are loaded now, in no order
    public static IReadOnlyList<Assembly> All => AssemblyLoader.LoadedScriptAssemblies();

    /// the assembly the engine knows by that id; null when it is not loaded
    public static Assembly? Find(int assembly_id)
    {
      return AssemblyLoader.TryGetAssembly(assembly_id, out Assembly? assembly) ? assembly : null;
    }

    internal static void RaiseLoaded(Assembly assembly)
    {
      try
      {
        Loaded?.Invoke(assembly);
      }
      catch (Exception e)
      {
        Host.HandleException(e);
      }
    }

    internal static void RaiseUnloading(Assembly assembly)
    {
      try
      {
        Unloading?.Invoke(assembly);
      }
      catch (Exception e)
      {
        Host.HandleException(e);
      }
    }
  }
}
