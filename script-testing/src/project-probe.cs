using System;
using Other.Core;
using Other.Editor;

/// reads the project the suite set up, through the API a window reads it through: what
///  describes it from the engine's assembly, and how it is built from the editor's. only
///  Read, HasNoFailure and RequestRebuild name the editor's, so only they need it loaded
public static class ProjectProbe
{
  private static ulong last_revision;

  /// one bit for each part of the project that reads as the suite wrote it
  public static int Read()
  {
    int bits = 0;
    if (Project.State == ProjectState.Loaded && Project.IsOpen)
    {
      bits |= 1;
    }
    if (Project.Name == "Sandbox Game" && Project.Version == "0.1.0" && Project.Author == "Someone" && Project.Description == "a game, tested")
    {
      bits |= 2;
    }
    if (Project.File == "game/sandbox.toml" && Project.SceneCount == 2 && Project.StartingScene == "Main")
    {
      bits |= 4;
    }
    if (ProjectFile.HasNotice && ProjectFile.NoticeError.Length == 0 && ProjectFile.NoticeNeedsReopen.Count == 2 &&
        ProjectFile.NoticeNeedsReopen[0] == "the scene list" && ProjectFile.NoticeNeedsReopen[1] == "the plugins")
    {
      bits |= 8;
    }
    if (Project.LoadErrors.Count == 1 && Project.LoadErrors[0].Subject == "Second" && Project.LoadErrors[0].Message == "its file is missing")
    {
      bits |= 16;
    }
    if (ProjectScripts.ProjectPath == "game/Game.csproj" && ProjectScripts.Building && !ProjectScripts.Loaded)
    {
      bits |= 32;
    }
    ScriptFailure? failure = ProjectScripts.Failure;
    if (failure != null && failure.ProjectPath == "game/Game.csproj" && failure.Reason == "The build failed." &&
        failure.Details == "error CS0103: café does not exist" && !failure.CanRetry)
    {
      bits |= 64;
    }
    return bits;
  }

  /// which is a ProjectState by its place in the enum, so the two sides' numbers are compared
  public static int StateIs(int which)
  {
    ProjectState[] states = { ProjectState.Empty, ProjectState.Loading, ProjectState.Loaded, ProjectState.LoadFailed, ProjectState.Unloading };
    return Project.State == states[which] ? 1 : 0;
  }

  public static int NameIsRenamed() { return Project.Name == "Another Name" ? 1 : 0; }
  public static int HasNoFailure() { return ProjectScripts.Failure == null ? 1 : 0; }
  public static int IsOpen() { return Project.IsOpen ? 1 : 0; }

  /// 1 when the project's revision is not what it was at the call before
  public static int RevisionMoved()
  {
    ulong now = Project.Revision;
    bool moved = now != last_revision;
    last_revision = now;
    return moved ? 1 : 0;
  }

  public static int RequestRebuild()
  {
    ProjectScripts.RequestRebuild();
    return 1;
  }
}
