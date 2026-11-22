using System;
using Other.Core;

/// reads the project the suite set up, through the API a window reads it through
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
    if (Project.HasFileNotice && Project.FileNoticeError.Length == 0 && Project.FileNoticeNeedsReopen.Count == 2 &&
        Project.FileNoticeNeedsReopen[0] == "the scene list" && Project.FileNoticeNeedsReopen[1] == "the plugins")
    {
      bits |= 8;
    }
    if (Project.LoadErrors.Count == 1 && Project.LoadErrors[0].Subject == "Second" && Project.LoadErrors[0].Message == "its file is missing")
    {
      bits |= 16;
    }
    if (Project.Scripts.ProjectPath == "game/Game.csproj" && Project.Scripts.Building && !Project.Scripts.Loaded)
    {
      bits |= 32;
    }
    ScriptFailure? failure = Project.Scripts.Failure;
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
  public static int HasNoFailure() { return Project.Scripts.Failure == null ? 1 : 0; }
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
    Project.Scripts.RequestRebuild();
    return 1;
  }
}
