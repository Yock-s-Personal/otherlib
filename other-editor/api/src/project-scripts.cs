using System;
using System.Collections.Generic;
using Other.Core;
using OtherCsBindings;

namespace Other.Editor
{
  /// why the project's C# scripts are not running their current sources
  public sealed class ScriptFailure
  {
    /// the C# project that failed
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

  /// what the engine keeps about the open project that only an editor shows: the build of
  ///  its C# scripts, and what happened when its file changed on disk. read again only
  ///  when the project says something changed (Project.Revision)
  internal static unsafe class ProjectDetails
  {
    [NativeFunction("EditorProjectHasFileNotice")]
    internal static delegate* unmanaged[Cdecl]<NativeBool32> NativeHasFileNotice;
    [NativeFunction("EditorProjectGetFileNoticeError")]
    internal static delegate* unmanaged[Cdecl]<byte*> NativeGetFileNoticeError;
    [NativeFunction("EditorProjectGetFileNoticePartCount")]
    internal static delegate* unmanaged[Cdecl]<int> NativeGetFileNoticePartCount;
    [NativeFunction("EditorProjectGetFileNoticePart")]
    internal static delegate* unmanaged[Cdecl]<int, byte*> NativeGetFileNoticePart;
    [NativeFunction("EditorProjectGetScriptProjectPath")]
    internal static delegate* unmanaged[Cdecl]<byte*> NativeGetScriptProjectPath;
    [NativeFunction("EditorProjectScriptsBuilding")]
    internal static delegate* unmanaged[Cdecl]<NativeBool32> NativeScriptsBuilding;
    [NativeFunction("EditorProjectScriptsLoaded")]
    internal static delegate* unmanaged[Cdecl]<NativeBool32> NativeScriptsLoaded;
    [NativeFunction("EditorProjectHasScriptFailure")]
    internal static delegate* unmanaged[Cdecl]<NativeBool32> NativeHasScriptFailure;
    [NativeFunction("EditorProjectGetScriptFailureProjectPath")]
    internal static delegate* unmanaged[Cdecl]<byte*> NativeGetScriptFailureProjectPath;
    [NativeFunction("EditorProjectGetScriptFailureReason")]
    internal static delegate* unmanaged[Cdecl]<byte*> NativeGetScriptFailureReason;
    [NativeFunction("EditorProjectGetScriptFailureDetails")]
    internal static delegate* unmanaged[Cdecl]<byte*> NativeGetScriptFailureDetails;
    [NativeFunction("EditorProjectScriptFailureCanRetry")]
    internal static delegate* unmanaged[Cdecl]<NativeBool32> NativeScriptFailureCanRetry;
    [NativeFunction("EditorProjectRequestScriptRebuild")]
    internal static delegate* unmanaged[Cdecl]<void> NativeRequestScriptRebuild;

    private static readonly string[] no_parts = Array.Empty<string>();
    private static bool read;
    private static ulong stamp;

    internal static bool has_file_notice;
    internal static string file_notice_error = "";
    internal static string[] file_notice_needs_reopen = no_parts;
    internal static string script_project_path = "";
    internal static bool scripts_building;
    internal static bool scripts_loaded;
    internal static ScriptFailure? script_failure;

    internal static void Refresh()
    {
      /// asking the project for its revision is what has the engine take its copy
      ulong now = Project.Revision;
      if (read && now == stamp)
      {
        return;
      }
      read = true;
      stamp = now;

      has_file_notice = NativeHasFileNotice();
      file_notice_error = Utf8Arg.Read(NativeGetFileNoticeError());
      int parts = NativeGetFileNoticePartCount();
      file_notice_needs_reopen = parts == 0 ? no_parts : new string[parts];
      for (int i = 0; i < parts; i++)
      {
        file_notice_needs_reopen[i] = Utf8Arg.Read(NativeGetFileNoticePart(i));
      }

      script_project_path = Utf8Arg.Read(NativeGetScriptProjectPath());
      scripts_building = NativeScriptsBuilding();
      scripts_loaded = NativeScriptsLoaded();
      script_failure = null;
      if (NativeHasScriptFailure())
      {
        string project_path = Utf8Arg.Read(NativeGetScriptFailureProjectPath());
        string reason = Utf8Arg.Read(NativeGetScriptFailureReason());
        string details = Utf8Arg.Read(NativeGetScriptFailureDetails());
        script_failure = new ScriptFailure(project_path, reason, details, NativeScriptFailureCanRetry());
      }
    }
  }

  /// the open project's C# scripts, as something that is built
  public static class ProjectScripts
  {
    /// the C# project a rebuild would build. empty when the project has none
    public static string ProjectPath { get { ProjectDetails.Refresh(); return ProjectDetails.script_project_path; } }
    /// a build is asked for or running
    public static bool Building { get { ProjectDetails.Refresh(); return ProjectDetails.scripts_building; } }
    /// an assembly of the project is loaded
    public static bool Loaded { get { ProjectDetails.Refresh(); return ProjectDetails.scripts_loaded; } }
    /// null while the scripts are running their current sources
    public static ScriptFailure? Failure { get { ProjectDetails.Refresh(); return ProjectDetails.script_failure; } }

    /// the C# project is built again on the engine's next tick
    public static void RequestRebuild()
    {
      unsafe { ProjectDetails.NativeRequestScriptRebuild(); }
    }
  }

  /// the open project's file, when it changed on disk under the open project
  public static class ProjectFile
  {
    /// the file changed and the open project could not take all of it
    public static bool HasNotice { get { ProjectDetails.Refresh(); return ProjectDetails.has_file_notice; } }
    /// why the changed file could not be read. empty when it could
    public static string NoticeError { get { ProjectDetails.Refresh(); return ProjectDetails.file_notice_error; } }
    /// the parts of the changed file that are only read when the project is opened again
    public static IReadOnlyList<string> NoticeNeedsReopen { get { ProjectDetails.Refresh(); return ProjectDetails.file_notice_needs_reopen; } }
  }
}
