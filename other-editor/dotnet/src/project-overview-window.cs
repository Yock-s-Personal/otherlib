using System;
using System.Collections.Generic;
using System.IO;
using Other.Core;
using Other.UI;

namespace Other.Editor
{
  /// project metadata, the state of the project's C# scripts and a way to rebuild them
  /// \todo frame and render timings once the profiler exposes data the editor can read
  [EditorWindow("project-overview", Title = "Project", DefaultWidth = 520f, DefaultHeight = 620f)]
  public class ProjectOverviewWindow : UIWindow
  {
    private const float kDetailListHeight = 160f;
    private const ChildFlags kDetailListFlags = ChildFlags.Borders | ChildFlags.ResizeY;
    private static readonly Vec4 kErrorRed = new Vec4(230f / 255f, 51f / 255f, 51f / 255f, 1f);

    /// text made from the project, made again only when the project changes
    private bool read;
    private ulong shown_revision;
    private string scenes_text = "0";
    private string notice_text = "";
    private readonly List<string> load_error_lines = new();
    private readonly List<string> detail_lines = new();

    protected override void OnRenderBody()
    {
      if (!Project.IsOpen)
      {
        UIBindings.TextDisabled("No project is open.");
        return;
      }

      if (!read || Project.Revision != shown_revision)
      {
        read = true;
        shown_revision = Project.Revision;
        RefreshText();
      }

      RenderMetadata();
      RenderScripts();
      RenderLoadErrors();
    }

    private static string StateLabel(ProjectState state)
    {
      switch (state)
      {
        case ProjectState.Empty: return "Empty";
        case ProjectState.Loading: return "Loading";
        case ProjectState.Loaded: return "Loaded";
        case ProjectState.LoadFailed: return "Load failed";
        case ProjectState.Unloading: return "Unloading";
        default: return "Unknown";
      }
    }

    private void RenderMetadata()
    {
      UIBindings.SeparatorText("Project");
      Widgets.PropertyDisplay("Name", Project.Name);
      Widgets.PropertyDisplay("Version", Project.Version);
      Widgets.PropertyDisplay("Author", Project.Author);
      Widgets.PropertyDisplay("Description", Project.Description);
      if (Project.File.Length > 0)
      {
        Widgets.PropertyDisplay("File", Project.File);
      }
      Widgets.PropertyDisplay("State", StateLabel(Project.State));
      Widgets.PropertyDisplay("Scenes", scenes_text);

      /// a dropped starting scene leaves none: nothing is activated until the user picks one
      if (Project.StartingScene.Length > 0)
      {
        Widgets.PropertyDisplay("Start scene", Project.StartingScene);
      }
      else
      {
        Widgets.PropertyDisplay("Start scene", "none", kErrorRed);
      }

      /// the file changed on disk and the open project could not take all of it
      if (!Project.HasFileNotice)
      {
        return;
      }
      UIBindings.PushStyleColor(StyleColor.Text, kErrorRed);
      UIBindings.TextWrapped(notice_text);
      UIBindings.PopStyleColor();
    }

    private void RenderScripts()
    {
      UIBindings.SeparatorText("C# Scripts");

      ScriptFailure? failure = Project.Scripts.Failure;
      string csproj = Project.Scripts.ProjectPath;
      if (csproj.Length == 0 && failure == null)
      {
        UIBindings.TextDisabled("This project has no C# project.");
        return;
      }

      bool building = Project.Scripts.Building;
      bool mounted = Project.Scripts.Loaded;
      Widgets.PropertyDisplay("Project", csproj);

      if (building)
      {
        Widgets.PropertyDisplay("Status", "Building...");
      }
      else if (failure != null && mounted)
      {
        /// a rebuild failed after load: the assembly from the last good build is still running
        Widgets.PropertyDisplay("Status", "Build failed, running the last good build", kErrorRed);
      }
      else if (failure != null)
      {
        Widgets.PropertyDisplay("Status", "Not loaded", kErrorRed);
      }
      else if (mounted)
      {
        Widgets.PropertyDisplay("Status", "Loaded");
      }
      else
      {
        Widgets.PropertyDisplay("Status", "Not loaded");
      }

      bool can_rebuild = !building && Project.State == ProjectState.Loaded && (failure == null || failure.CanRetry);
      UIBindings.BeginDisabled(!can_rebuild);
      if (UIBindings.Button(failure != null ? "Retry build" : "Rebuild"))
      {
        Project.Scripts.RequestRebuild();
      }
      UIBindings.EndDisabled();

      if (failure == null)
      {
        return;
      }

      UIBindings.PushStyleColor(StyleColor.Text, kErrorRed);
      UIBindings.TextWrapped(failure.Reason);
      UIBindings.PopStyleColor();
      if (!failure.CanRetry)
      {
        UIBindings.TextWrapped("Restore the file and reopen the project.");
        return;
      }

      if (detail_lines.Count == 0)
      {
        UIBindings.TextDisabled("Nothing more was captured. The editor log has the full output.");
        return;
      }

      if (UIBindings.SmallButton("Copy"))
      {
        UIBindings.ClipboardText = failure.Details;
      }
      if (UIBindings.BeginChild("##script-failure-details", 0f, kDetailListHeight, kDetailListFlags))
      {
        foreach (string line in detail_lines)
        {
          UIBindings.TextWrapped(line);
        }
      }
      UIBindings.EndChild();
    }

    private void RenderLoadErrors()
    {
      if (load_error_lines.Count == 0)
      {
        return;
      }

      UIBindings.SeparatorText("Load Errors");
      foreach (string line in load_error_lines)
      {
        UIBindings.Bullet();
        UIBindings.TextWrapped(line);
      }
    }

    private void RefreshText()
    {
      scenes_text = Project.SceneCount.ToString();

      notice_text = "";
      if (Project.HasFileNotice)
      {
        notice_text = Project.FileNoticeError.Length > 0
          ? $"The project file changed on disk and was not read: {Project.FileNoticeError}"
          : $"The project file changed on disk. Open the project again to apply: {string.Join(", ", Project.FileNoticeNeedsReopen)}.";
      }

      load_error_lines.Clear();
      foreach (LoadError error in Project.LoadErrors)
      {
        load_error_lines.Add($"{error.Subject}: {error.Message}");
      }

      detail_lines.Clear();
      ScriptFailure? failure = Project.Scripts.Failure;
      if (failure == null)
      {
        return;
      }

      string project_dir = ProjectDirectory(failure.ProjectPath);
      foreach (string raw in failure.Details.Split('\n'))
      {
        string tidy = TidyBuildLine(raw.AsSpan().TrimEnd('\r'), project_dir);
        if (tidy.Length > 0)
        {
          detail_lines.Add(tidy);
        }
      }
    }

    /// the folder of the C# project with its closing separator, as build output spells it.
    ///  empty when the path cannot be made absolute
    private static string ProjectDirectory(string csproj)
    {
      try
      {
        string? directory = Path.GetDirectoryName(Path.GetFullPath(csproj));
        return string.IsNullOrEmpty(directory) ? "" : directory + Path.DirectorySeparatorChar;
      }
      catch (Exception)
      {
        return "";
      }
    }

    /// msbuild starts each diagnostic with its node and project number ("  1:7>"), writes the
    ///  source as an absolute path and ends with " [<csproj>]"; all three repeat on every
    ///  line and push the message itself out of a narrow panel
    private static string TidyBuildLine(ReadOnlySpan<char> line, string project_dir)
    {
      line = line.TrimStart(" \t");
      int marker = line.IndexOf('>');
      if (marker >= 0 && line.Slice(0, marker).IndexOfAnyExcept("0123456789:") < 0)
      {
        line = line.Slice(marker + 1);
      }

      if (line.EndsWith("]"))
      {
        int open = line.LastIndexOf(" [");
        if (open >= 0)
        {
          line = line.Slice(0, open);
        }
      }
      if (project_dir.Length > 0 && line.StartsWith(project_dir))
      {
        line = line.Slice(project_dir.Length);
      }
      return line.ToString();
    }
  }
}
