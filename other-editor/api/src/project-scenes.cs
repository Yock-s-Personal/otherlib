using System.Collections.Generic;
using Other.Core;
using OtherCsBindings;

namespace Other.Editor
{
  /// one scene of the project as a list of them shows it
  public sealed class ProjectScene
  {
    public ulong Id { get; internal set; }
    /// what the project calls it
    public string Name { get; internal set; } = "";
    /// what the scene calls itself, which is its file's name
    public string SceneName { get; internal set; } = "";
    /// it is the scene in the editor now
    public bool IsActive { get; internal set; }
    /// it is the scene the project starts in
    public bool IsStarting { get; internal set; }
  }

  /// the scenes of the project that are loaded, kept: read again only when the project or
  ///  the scene in the editor changes
  public static unsafe class ProjectScenes
  {
    [NativeFunction("EditorScenesRead")]
    internal static delegate* unmanaged[Cdecl]<int> NativeRead;
    [NativeFunction("EditorScenesAt")]
    internal static delegate* unmanaged[Cdecl]<int, ulong*, NativeBool32*, NativeBool32*, byte*> NativeAt;
    [NativeFunction("EditorScenesSceneName")]
    internal static delegate* unmanaged[Cdecl]<int, byte*> NativeSceneName;
    [NativeFunction("EditorScenesActivate")]
    internal static delegate* unmanaged[Cdecl]<ulong, void> NativeActivate;

    private static readonly List<ProjectScene> all = new();
    private static ulong revision;
    private static bool read;
    private static ulong read_project;
    private static ulong read_scene;

    /// goes up when what is kept here was read again
    public static ulong Revision
    {
      get
      {
        Refresh();
        return revision;
      }
    }

    /// in the order the project has them
    public static IReadOnlyList<ProjectScene> All
    {
      get
      {
        Refresh();
        return all;
      }
    }

    /// asks for that scene to be the one in the editor. it is after the frame, when no
    ///  window is in the middle of drawing the scene that goes; what is selected is not
    ///  carried over
    public static void Activate(ProjectScene scene)
    {
      NativeActivate(scene.Id);
    }

    private static void Refresh()
    {
      /// another scene in the editor is another revision: no two scenes share one
      ulong project = Project.Revision;
      ulong scene = Scene.Revision;
      if (read && project == read_project && scene == read_scene)
      {
        return;
      }
      read = true;
      read_project = project;
      read_scene = scene;
      revision++;

      all.Clear();
      int count = NativeRead();
      for (int i = 0; i < count; i++)
      {
        ulong id = 0;
        NativeBool32 active = false;
        NativeBool32 starting = false;
        string name = Utf8Arg.Read(NativeAt(i, &id, &active, &starting));
        all.Add(new ProjectScene { Id = id, Name = name, SceneName = Utf8Arg.Read(NativeSceneName(i)), IsActive = active, IsStarting = starting });
      }
    }
  }
}
