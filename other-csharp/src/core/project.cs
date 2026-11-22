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

  /// why the project's C# scripts are not running their current sources
  public sealed class ScriptFailure
  {
    public string ProjectPath { get; }
    public string Reason { get; }
    /// the build's errors when a build failed, otherwise whatever else is known
    public string Details { get; }
    /// false when no rebuild can fix it: the project has to be opened again
    public bool CanRetry { get; }

    internal ScriptFailure(string project_path, string reason, string details, bool can_retry)
    {
      ProjectPath = project_path;
      Reason = reason;
      Details = details;
      CanRetry = can_retry;
    }
  }

  /// the project that is open, to read. the engine is asked once a frame whether anything
  ///  about it changed, and the strings are only read again when something did
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
    [NativeFunction("ProjectHasFileNotice")]
    internal static unsafe delegate* unmanaged[Cdecl]<NativeBool32> NativeHasFileNotice;
    [NativeFunction("ProjectGetFileNoticeError")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*> NativeGetFileNoticeError;
    [NativeFunction("ProjectGetFileNoticePartCount")]
    internal static unsafe delegate* unmanaged[Cdecl]<int> NativeGetFileNoticePartCount;
    [NativeFunction("ProjectGetFileNoticePart")]
    internal static unsafe delegate* unmanaged[Cdecl]<int, byte*> NativeGetFileNoticePart;
    [NativeFunction("ProjectGetLoadErrorCount")]
    internal static unsafe delegate* unmanaged[Cdecl]<int> NativeGetLoadErrorCount;
    [NativeFunction("ProjectGetLoadErrorSubject")]
    internal static unsafe delegate* unmanaged[Cdecl]<int, byte*> NativeGetLoadErrorSubject;
    [NativeFunction("ProjectGetLoadErrorMessage")]
    internal static unsafe delegate* unmanaged[Cdecl]<int, byte*> NativeGetLoadErrorMessage;
    [NativeFunction("ProjectGetScriptProjectPath")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*> NativeGetScriptProjectPath;
    [NativeFunction("ProjectScriptsBuilding")]
    internal static unsafe delegate* unmanaged[Cdecl]<NativeBool32> NativeScriptsBuilding;
    [NativeFunction("ProjectScriptsLoaded")]
    internal static unsafe delegate* unmanaged[Cdecl]<NativeBool32> NativeScriptsLoaded;
    [NativeFunction("ProjectHasScriptFailure")]
    internal static unsafe delegate* unmanaged[Cdecl]<NativeBool32> NativeHasScriptFailure;
    [NativeFunction("ProjectGetScriptFailureProjectPath")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*> NativeGetScriptFailureProjectPath;
    [NativeFunction("ProjectGetScriptFailureReason")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*> NativeGetScriptFailureReason;
    [NativeFunction("ProjectGetScriptFailureDetails")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*> NativeGetScriptFailureDetails;
    [NativeFunction("ProjectScriptFailureCanRetry")]
    internal static unsafe delegate* unmanaged[Cdecl]<NativeBool32> NativeScriptFailureCanRetry;
    [NativeFunction("ProjectRequestScriptRebuild")]
    internal static unsafe delegate* unmanaged[Cdecl]<void> NativeRequestScriptRebuild;

    private static readonly string[] no_parts = Array.Empty<string>();
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
    private static bool has_file_notice;
    private static string file_notice_error = "";
    private static string[] file_notice_needs_reopen = no_parts;
    private static LoadError[] load_errors = no_errors;
    private static string script_project_path = "";
    private static bool scripts_building;
    private static bool scripts_loaded;
    private static ScriptFailure? script_failure;

    /// a number that is different whenever anything below is: text made from the project
    ///  only has to be made again when this moves
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

    /// the project file changed on disk and the open project could not take all of it
    public static bool HasFileNotice { get { Refresh(); return has_file_notice; } }
    /// why the changed file could not be read. empty when it could
    public static string FileNoticeError { get { Refresh(); return file_notice_error; } }
    /// the parts of the changed file that are only read when the project is opened again
    public static IReadOnlyList<string> FileNoticeNeedsReopen { get { Refresh(); return file_notice_needs_reopen; } }

    public static IReadOnlyList<LoadError> LoadErrors { get { Refresh(); return load_errors; } }

    /// the project's C# scripts
    public static class Scripts
    {
      /// the C# project a rebuild would build. empty when the project has none
      public static string ProjectPath { get { Refresh(); return script_project_path; } }
      /// a build is asked for or running
      public static bool Building { get { Refresh(); return scripts_building; } }
      /// an assembly of the project is loaded
      public static bool Loaded { get { Refresh(); return scripts_loaded; } }
      /// null while the scripts are running their current sources
      public static ScriptFailure? Failure { get { Refresh(); return script_failure; } }

      /// the C# project is built again on the engine's next tick
      public static void RequestRebuild()
      {
        unsafe { NativeRequestScriptRebuild(); }
      }
    }

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

        has_file_notice = NativeHasFileNotice();
        file_notice_error = Text(NativeGetFileNoticeError());
        int parts = NativeGetFileNoticePartCount();
        file_notice_needs_reopen = parts == 0 ? no_parts : new string[parts];
        for (int i = 0; i < parts; i++)
        {
          file_notice_needs_reopen[i] = Text(NativeGetFileNoticePart(i));
        }

        int errors = NativeGetLoadErrorCount();
        load_errors = errors == 0 ? no_errors : new LoadError[errors];
        for (int i = 0; i < errors; i++)
        {
          string subject = Text(NativeGetLoadErrorSubject(i));
          load_errors[i] = new LoadError(subject, Text(NativeGetLoadErrorMessage(i)));
        }

        script_project_path = Text(NativeGetScriptProjectPath());
        scripts_building = NativeScriptsBuilding();
        scripts_loaded = NativeScriptsLoaded();

        script_failure = null;
        if (NativeHasScriptFailure())
        {
          string project_path = Text(NativeGetScriptFailureProjectPath());
          string reason = Text(NativeGetScriptFailureReason());
          string details = Text(NativeGetScriptFailureDetails());
          script_failure = new ScriptFailure(project_path, reason, details, NativeScriptFailureCanRetry());
        }
      }
    }
  }
}
