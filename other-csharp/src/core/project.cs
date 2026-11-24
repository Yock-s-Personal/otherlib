using OtherCsBindings;
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace Other.Core
{
  /// project::state on the native side, value for value
  public enum ProjectState
  {
    Empty = 0,
    Loading = 1,
    Loaded = 2,
    LoadFailed = 3,
    Unloading = 4,
  }

  /// something the project named that was left out when it loaded, and why
  public readonly record struct LoadError(string Subject, string Message);

  /// the project that is open, to read. the engine is asked once a frame whether anything
  ///  about it changed, and the strings are only read again when something did. what is
  ///  here describes the project; how its scripts are built, and what happened to its file
  ///  on disk, is the editor's to show (Other.Editor.ProjectScripts, ProjectFile)
  public static class Project
  {
    [NativeFunction("ProjectChangeStamp")]
    internal static unsafe delegate* unmanaged[Cdecl]<ulong> NativeChangeStamp;
    [NativeFunction("ProjectGetState")]
    internal static unsafe delegate* unmanaged[Cdecl]<int> NativeGetState;
    [NativeFunction("ProjectGetName")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*> NativeGetName;
    [NativeFunction("ProjectGetVersion")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*> NativeGetVersion;
    [NativeFunction("ProjectGetAuthor")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*> NativeGetAuthor;
    [NativeFunction("ProjectGetDescription")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*> NativeGetDescription;
    [NativeFunction("ProjectGetFile")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*> NativeGetFile;
    [NativeFunction("ProjectGetSceneCount")]
    internal static unsafe delegate* unmanaged[Cdecl]<int> NativeGetSceneCount;
    [NativeFunction("ProjectGetStartingScene")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*> NativeGetStartingScene;
    [NativeFunction("ProjectGetLoadErrorCount")]
    internal static unsafe delegate* unmanaged[Cdecl]<int> NativeGetLoadErrorCount;
    [NativeFunction("ProjectGetLoadErrorSubject")]
    internal static unsafe delegate* unmanaged[Cdecl]<int, byte*> NativeGetLoadErrorSubject;
    [NativeFunction("ProjectGetLoadErrorMessage")]
    internal static unsafe delegate* unmanaged[Cdecl]<int, byte*> NativeGetLoadErrorMessage;

    private static readonly LoadError[] no_errors = Array.Empty<LoadError>();

    private static bool read;
    private static ulong stamp;

    private static ProjectState state = ProjectState.Empty;
    private static string name = "";
    private static string version = "";
    private static string author = "";
    private static string description = "";
    private static string file = "";
    private static int scene_count;
    private static string starting_scene = "";
    private static LoadError[] load_errors = no_errors;

    /// a number that is different whenever anything about the project is, what only an
    ///  editor shows too: text made from the project only has to be made again when this moves
    public static ulong Revision { get { Refresh(); return stamp; } }

    public static ProjectState State { get { Refresh(); return state; } }
    public static bool IsOpen => State != ProjectState.Empty;
    public static string Name { get { Refresh(); return name; } }
    public static string Version { get { Refresh(); return version; } }
    public static string Author { get { Refresh(); return author; } }
    public static string Description { get { Refresh(); return description; } }
    /// the project file it was opened from. empty when that is not known
    public static string File { get { Refresh(); return file; } }
    public static int SceneCount { get { Refresh(); return scene_count; } }
    /// the name of the scene the project starts in. empty when there is none
    public static string StartingScene { get { Refresh(); return starting_scene; } }

    public static IReadOnlyList<LoadError> LoadErrors { get { Refresh(); return load_errors; } }

    private static unsafe string Text(byte* text)
    {
      return Marshal.PtrToStringUTF8((IntPtr)text) ?? string.Empty;
    }

    private static void Refresh()
    {
      unsafe
      {
        ulong now = NativeChangeStamp();
        if (read && now == stamp)
        {
          return;
        }
        read = true;
        stamp = now;

        state = (ProjectState)NativeGetState();
        name = Text(NativeGetName());
        version = Text(NativeGetVersion());
        author = Text(NativeGetAuthor());
        description = Text(NativeGetDescription());
        file = Text(NativeGetFile());
        scene_count = NativeGetSceneCount();
        starting_scene = Text(NativeGetStartingScene());

        int errors = NativeGetLoadErrorCount();
        load_errors = errors == 0 ? no_errors : new LoadError[errors];
        for (int i = 0; i < errors; i++)
        {
          string subject = Text(NativeGetLoadErrorSubject(i));
          load_errors[i] = new LoadError(subject, Text(NativeGetLoadErrorMessage(i)));
        }
      }
    }
  }
}
