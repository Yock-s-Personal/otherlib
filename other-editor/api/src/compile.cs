using System.Collections.Generic;
using OtherCsBindings;

namespace Other.Editor
{
  /// \note compile_service::state natively is the same list in the same order
  public enum CompileState
  {
    NoProject = 0,
    /// a project is open and still loading: it cannot be compiled yet
    Loading = 1,
    Ready = 2,
  }

  /// \note compile_service::result natively is the same list in the same order
  public enum CompileResult
  {
    /// no compile has finished, or one is running
    None = 0,
    Packaged = 1,
    Cancelled = 2,
    Failed = 3,
  }

  /// packaging the open project: what a compile would build from, the one compile that can
  ///  run, and what came of the last. the engine keeps all of it; what is text here is read
  ///  again only when the engine says it changed (Revision). Poll is called once a frame by
  ///  whoever shows it
  public static unsafe class Compile
  {
    [NativeFunction("EditorCompilePoll")]
    internal static delegate* unmanaged[Cdecl]<ulong*, int> NativePoll;
    [NativeFunction("EditorCompileRefresh")]
    internal static delegate* unmanaged[Cdecl]<void> NativeRefresh;
    [NativeFunction("EditorCompileCount")]
    internal static delegate* unmanaged[Cdecl]<int, int> NativeCount;
    [NativeFunction("EditorCompileText")]
    internal static delegate* unmanaged[Cdecl]<int, int, byte*> NativeText;
    [NativeFunction("EditorCompileNameProblem")]
    internal static delegate* unmanaged[Cdecl]<byte*, byte*> NativeNameProblem;
    [NativeFunction("EditorCompileStart")]
    internal static delegate* unmanaged[Cdecl]<byte*, byte*, byte*, byte*, NativeBool32, byte*> NativeStart;
    [NativeFunction("EditorCompileProgress")]
    internal static delegate* unmanaged[Cdecl]<int*, int*, NativeBool32*, NativeBool32> NativeProgress;
    [NativeFunction("EditorCompileStageLabel")]
    internal static delegate* unmanaged[Cdecl]<byte*> NativeStageLabel;
    [NativeFunction("EditorCompileCancel")]
    internal static delegate* unmanaged[Cdecl]<void> NativeCancel;
    [NativeFunction("EditorCompileResult")]
    internal static delegate* unmanaged[Cdecl]<int> NativeResult;

    /// \note compile_text natively is the same list in the same order
    private enum CompileText
    {
      ProjectFile,
      PublicName,
      OutputFolder,
      StartingScene,
      MainPipeline,
      Scenes,
      Pipelines,
      VmSources,
      Hooks,
      Problems,
      LeftOut,
      LeftOutSize,
      ReportPackage,
      ReportErrors,
      ReportWarnings,
      ReportLeftOut,
      ReportLeftOutSize,
    }

    private static ulong revision;
    private static ulong read_revision = ulong.MaxValue;
    private static CompileState state;

    private static string project_file = "";
    private static string public_name = "";
    private static string output_folder = "";
    private static string starting_scene = "";
    private static string main_pipeline = "";
    private static string left_out_size = "";
    private static readonly List<string> scenes = new();
    private static readonly List<string> pipelines = new();
    private static readonly List<string> vm_sources = new();
    private static readonly List<string> hooks = new();
    private static readonly List<string> problems = new();
    private static readonly List<string> left_out = new();

    private static CompileResult result;
    private static string package = "";
    private static string report_left_out_size = "";
    private static readonly List<string> errors = new();
    private static readonly List<string> warnings = new();
    private static readonly List<string> report_left_out = new();

    private static int stage_read = -1;
    private static string stage_label = "";

    /// once a frame: a compile that finished gives its result, and a project that is
    ///  another, or whose file changed on disk, is read again
    public static CompileState Poll()
    {
      ulong now = 0;
      state = (CompileState)NativePoll(&now);
      revision = now;
      return state;
    }

    /// moves when anything below is not what it was
    public static ulong Revision => revision;

    /// reads the open project again, which reads every scene and model of it
    public static void Refresh()
    {
      NativeRefresh();
    }

    /// the project file's name
    public static string ProjectFile { get { Read(); return project_file; } }
    /// what the project file says of each choice a compile takes; empty where it says nothing
    public static string PublicName { get { Read(); return public_name; } }
    public static string StartingScene { get { Read(); return starting_scene; } }
    public static string MainPipeline { get { Read(); return main_pipeline; } }
    /// where a package of this project goes unless another folder is chosen
    public static string DefaultOutputFolder { get { Read(); return output_folder; } }

    public static IReadOnlyList<string> Scenes { get { Read(); return scenes; } }
    public static IReadOnlyList<string> Pipelines { get { Read(); return pipelines; } }
    /// what the boot program is linked from: the VM sources, then each hook's handler
    public static IReadOnlyList<string> VmSources { get { Read(); return vm_sources; } }
    public static IReadOnlyList<string> Hooks { get { Read(); return hooks; } }
    /// what stops a compile whatever is chosen
    public static IReadOnlyList<string> Problems { get { Read(); return problems; } }
    /// the files a compile would leave out because nothing in the project uses them, and
    ///  their size as a person reads it
    public static IReadOnlyList<string> LeftOut { get { Read(); return left_out; } }
    public static string LeftOutSize { get { Read(); return left_out_size; } }

    /// what came of the last compile
    public static CompileResult Result { get { Read(); return result; } }
    /// the finished package, when it was packaged
    public static string Package { get { Read(); return package; } }
    public static IReadOnlyList<string> Errors { get { Read(); return errors; } }
    public static IReadOnlyList<string> Warnings { get { Read(); return warnings; } }
    public static IReadOnlyList<string> WasLeftOut { get { Read(); return report_left_out; } }
    public static string WasLeftOutSize { get { Read(); return report_left_out_size; } }

    /// why a public name cannot be used. empty when it can
    public static string NameProblem(string name)
    {
      using var name_utf8 = new Utf8Arg(name, stackalloc byte[Utf8Arg.StackSize]);
      return Utf8Arg.Read(NativeNameProblem(name_utf8.Pointer));
    }

    /// starts a compile with those choices, which become the project file's own. it is
    ///  made from the files on disk: a scene with changes that are only in the editor is
    ///  saved first by whoever calls this. false when it did not start, and error says why
    public static bool Start(string public_name, string output_folder, string starting_scene, string main_pipeline, bool debug, out string error)
    {
      using var name_utf8 = new Utf8Arg(public_name, stackalloc byte[Utf8Arg.StackSize]);
      using var folder_utf8 = new Utf8Arg(output_folder, stackalloc byte[Utf8Arg.StackSize]);
      using var scene_utf8 = new Utf8Arg(starting_scene, stackalloc byte[Utf8Arg.StackSize]);
      using var pipeline_utf8 = new Utf8Arg(main_pipeline, stackalloc byte[Utf8Arg.StackSize]);
      error = Utf8Arg.Read(NativeStart(name_utf8.Pointer, folder_utf8.Pointer, scene_utf8.Pointer, pipeline_utf8.Pointer, debug));
      return error.Length == 0;
    }

    public static bool Running
    {
      get
      {
        int stage = 0;
        int stages = 0;
        NativeBool32 cancelling = false;
        return NativeProgress(&stage, &stages, &cancelling);
      }
    }

    /// it was told to stop and has not stopped yet
    public static bool Cancelling
    {
      get
      {
        int stage = 0;
        int stages = 0;
        NativeBool32 cancelling = false;
        return NativeProgress(&stage, &stages, &cancelling) && cancelling;
      }
    }

    /// how far the running compile has got, from 0 to 1
    public static float Fraction
    {
      get
      {
        int stage = 0;
        int stages = 0;
        NativeBool32 cancelling = false;
        return NativeProgress(&stage, &stages, &cancelling) && stages > 0 ? (float)stage / stages : 0f;
      }
    }

    /// what the running compile is doing. a string is made only when the stage is another
    public static string StageLabel
    {
      get
      {
        int stage = 0;
        int stages = 0;
        NativeBool32 cancelling = false;
        if (!NativeProgress(&stage, &stages, &cancelling))
        {
          stage_read = -1;
          return "";
        }
        if (stage != stage_read)
        {
          stage_read = stage;
          stage_label = Utf8Arg.Read(NativeStageLabel());
        }
        return stage_label;
      }
    }

    /// it stops at the next file or stage, and its result says it was cancelled
    public static void Cancel()
    {
      NativeCancel();
    }

    private static string One(CompileText what)
    {
      return Utf8Arg.Read(NativeText((int)what, 0));
    }

    private static void Many(CompileText what, List<string> into)
    {
      into.Clear();
      int count = NativeCount((int)what);
      for (int i = 0; i < count; i++)
      {
        into.Add(Utf8Arg.Read(NativeText((int)what, i)));
      }
    }

    private static void Read()
    {
      if (read_revision == revision)
      {
        return;
      }
      read_revision = revision;

      project_file = One(CompileText.ProjectFile);
      public_name = One(CompileText.PublicName);
      output_folder = One(CompileText.OutputFolder);
      starting_scene = One(CompileText.StartingScene);
      main_pipeline = One(CompileText.MainPipeline);
      left_out_size = One(CompileText.LeftOutSize);
      Many(CompileText.Scenes, scenes);
      Many(CompileText.Pipelines, pipelines);
      Many(CompileText.VmSources, vm_sources);
      Many(CompileText.Hooks, hooks);
      Many(CompileText.Problems, problems);
      Many(CompileText.LeftOut, left_out);

      result = (CompileResult)NativeResult();
      package = One(CompileText.ReportPackage);
      report_left_out_size = One(CompileText.ReportLeftOutSize);
      Many(CompileText.ReportErrors, errors);
      Many(CompileText.ReportWarnings, warnings);
      Many(CompileText.ReportLeftOut, report_left_out);
    }
  }
}
