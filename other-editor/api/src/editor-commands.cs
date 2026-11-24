using System;
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
      /// the Scene View's tools. their keys are letters, so they are listened for only
      ///  while the pointer is over the view, nothing is being typed, and the right button
      ///  is not held: with it held the same letters fly the camera
      Func<bool> over_view = () => SceneViewport.Hovered && !UIWidgets.WantTextInput && !UIWidgets.IsMouseDown(MouseButton.Right) && !Handles.AnyActive;
      Commands.Add("tool.select", "Select Tool", () => SceneTools.Activate("select"), new KeyChord(Key.Q), "Edit")?.KeyWhen(over_view);
      Commands.Add("tool.move", "Move Tool", () => SceneTools.Activate("move"), new KeyChord(Key.W), "Edit")?.KeyWhen(over_view);
      Commands.Add("tool.rotate", "Rotate Tool", () => SceneTools.Activate("rotate"), new KeyChord(Key.E), "Edit")?.KeyWhen(over_view);
      Commands.Add("tool.scale", "Scale Tool", () => SceneTools.Activate("scale"), new KeyChord(Key.R), "Edit")?.KeyWhen(over_view);
      Commands.Add("tool.toggle-axes", "Local or World Axes", () => SceneTools.WorldAxes = !SceneTools.WorldAxes, new KeyChord(Key.X), "Edit")?.KeyWhen(over_view);
      /// the three before the tools have buttons in the menu bar
      PlaybackBar.Shown = true;
      /// the Scene View's tools are run from here on
      SceneTools.Host();
    }
  }
}
