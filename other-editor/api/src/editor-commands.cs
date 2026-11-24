using Other.UI;

namespace Other.Editor
{
  /// the editor's own commands: what it answers to out of the box. they are here, in the
  ///  assembly that is loaded once, and not with the editor's windows, whose assembly is
  ///  loaded again when it is rebuilt and would take them with it
  internal static class EditorCommands
  {
    /// called by the editor, by name, once there is a menu bar
    internal static void Add()
    {
      Commands.Add("scene.save", "Save Scene", () => EditorScene.Save(), new KeyChord(Key.S, KeyMods.Ctrl), "File");
      Commands.Add("edit.undo", "Undo", () => Edits.Undo(), new KeyChord(Key.Z, KeyMods.Ctrl), "Edit", () => Edits.CanUndo);
      Commands.Add("edit.redo", "Redo", () => Edits.Redo(), new KeyChord(Key.Y, KeyMods.Ctrl), "Edit", () => Edits.CanRedo);
      Commands.Add("playback.play", "Play", () => Playback.Play(), enabled: () => Playback.State != PlaybackState.Playing);
      Commands.Add("playback.pause", "Pause", () => Playback.Pause(), enabled: () => Playback.State == PlaybackState.Playing);
      Commands.Add("playback.stop", "Stop", () => Playback.Stop(), enabled: () => Playback.State != PlaybackState.Stopped);
      Commands.Add("view.focus-selection", "Focus on Selection", () => SceneViewport.FocusOnSelection(), new KeyChord(Key.F), "View", () => Selection.Count > 0);
      /// the three before it have buttons in the menu bar
      PlaybackBar.Shown = true;
    }
  }
}
