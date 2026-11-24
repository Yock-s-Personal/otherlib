using System;
using System.Reflection;
using Other.UI;
using OtherCsBindings;

namespace Other.Editor
{
  /// the system's own dialogs. a dialog answers on a thread of its own and C# is called on
  ///  the main thread only, so the answer is kept by the editor and handed over on a later
  ///  frame
  public static unsafe class Dialogs
  {
    [NativeFunction("EditorPickFolder")]
    internal static delegate* unmanaged[Cdecl]<NativeBool32> NativePickFolder;
    [NativeFunction("EditorTakePickedFolder")]
    internal static delegate* unmanaged[Cdecl]<byte*, int, NativeBool32> NativeTakePickedFolder;

    private const int kPathSize = 1024;
    private static readonly byte[] path = new byte[kPathSize];
    private static Action<string>? waiting;
    private static bool watching_unloads;

    /// asks for a folder. picked is called with it on the main thread, on a later frame;
    ///  it is not called when the dialog is cancelled. one who asks while another is still
    ///  waiting takes its place. false when there is nothing to show a dialog with
    public static bool PickFolder(Action<string> picked)
    {
      if (!NativePickFolder())
      {
        return false;
      }
      /// the answer is handed over by what the editor calls each frame
      Commands.Host();
      if (!watching_unloads)
      {
        watching_unloads = true;
        ScriptAssemblies.Unloading += Forget;
      }
      waiting = picked;
      return true;
    }

    /// what waits is code of the assembly it was written in: when that goes, so does it
    private static void Forget(Assembly assembly)
    {
      if (waiting != null && (waiting.Method.DeclaringType?.Assembly == assembly || assembly == typeof(Dialogs).Assembly))
      {
        waiting = null;
      }
      if (assembly == typeof(Dialogs).Assembly)
      {
        ScriptAssemblies.Unloading -= Forget;
        watching_unloads = false;
      }
    }

    /// once a frame, by the editor's frame function
    internal static void Frame()
    {
      if (waiting == null)
      {
        return;
      }

      fixed (byte* buffer = path)
      {
        if (!NativeTakePickedFolder(buffer, kPathSize))
        {
          return;
        }
        Action<string> picked = waiting;
        waiting = null;
        try
        {
          picked(Utf8Arg.Read(buffer));
        }
        catch (Exception e)
        {
          Core.Logger.LogError($"What was to be done with a picked folder threw: {e}");
        }
      }
    }
  }
}
