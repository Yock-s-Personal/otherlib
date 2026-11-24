using System.Collections.Generic;
using Other.Core;
using Other.UI;

namespace Other.Editor
{
  /// the project's scenes, one a row. a click on one makes it the scene in the editor
  [EditorWindow("scene-graph-editor", Title = "Scene Graph", DefaultWidth = 420f, DefaultHeight = 300f)]
  public class SceneListWindow : UIWindow
  {
    /// a row's text, made again only when the list changes
    private readonly List<string> labels = new();
    private bool read;
    private ulong shown_revision;

    protected override void OnRenderBody()
    {
      /// a scene can only be made the active one once its document has loaded
      bool loading = Project.State == ProjectState.Loading;
      if (loading)
      {
        UIWidgets.TextDisabled("Project is loading...");
      }

      IReadOnlyList<ProjectScene> scenes = ProjectScenes.All;
      if (!read || ProjectScenes.Revision != shown_revision)
      {
        read = true;
        shown_revision = ProjectScenes.Revision;
        RefreshLabels(scenes);
      }

      UIWidgets.BeginDisabled(loading);
      for (int i = 0; i < scenes.Count; i++)
      {
        if (UIWidgets.Selectable(labels[i], scenes[i].IsActive) && !scenes[i].IsActive)
        {
          ProjectScenes.Activate(scenes[i]);
        }
      }
      UIWidgets.EndDisabled();

      if (scenes.Count == 0)
      {
        UIWidgets.TextDisabled("No scenes in the scene graph.");
      }
    }

    private void RefreshLabels(IReadOnlyList<ProjectScene> scenes)
    {
      labels.Clear();
      for (int i = 0; i < scenes.Count; i++)
      {
        ProjectScene scene = scenes[i];
        /// the project names a scene apart from its file: both are shown when they differ
        string label = scene.Name == scene.SceneName ? scene.Name : $"{scene.Name} ({scene.SceneName})";
        if (scene.IsActive)
        {
          label += "  [active]";
        }
        if (scene.IsStarting)
        {
          label += "  [start]";
        }
        /// what follows ### is the row's id, the same while the tags in its text change
        labels.Add($"{label}###scene-{scene.Id}");
      }
    }
  }
}
