using System;
using System.Reflection;

namespace OtherCsBindings
{
  /// raised for a script assembly just before it is unloaded (project close, or the unload
  ///  half of a hot reload). whatever holds a delegate or a type of that assembly lets go of
  ///  it here: held, they keep the assembly in memory and keep calling its old code
  public static class AssemblyUnload
  {
    public static event Action<Assembly>? Unloading;

    internal static void Raise(Assembly assembly)
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
