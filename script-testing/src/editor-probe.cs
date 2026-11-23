using System;
using System.Collections.Generic;
using Other;
using Other.Editor;
using Other.UI;

/// what the suite asks of the editor's C# API. the scene it is asked in has "First" and
///  "Second" at the top and "Under" under "Second". an answer of 1 is yes; anything else
///  is the place of the first thing that was not so. nothing of Other.Editor is in a field
///  here: this assembly is loaded by tests that never load the editor's
public static class EditorProbe
{
  public static int SelectionRoundTrip()
  {
    SceneObject first = SceneObject.Find("First");
    SceneObject second = SceneObject.Find("Second");
    Selection.Clear();
    ulong at_start = Selection.Revision;
    if (Selection.Count != 0 || Selection.Active != null) { return 2; }

    Selection.Set(first);
    if (Selection.Count != 1 || !ReferenceEquals(Selection.Active, first) || Selection.Revision == at_start) { return 3; }
    /// asked again with nothing changed, it is the same list and the same count of changes
    ulong after_set = Selection.Revision;
    if (!ReferenceEquals(Selection.Objects, Selection.Objects) || Selection.Revision != after_set) { return 4; }

    Selection.Add(second);
    if (Selection.Count != 2 || !ReferenceEquals(Selection.Active, second) || !Selection.Contains(first)) { return 5; }
    /// one selected again becomes the active one and is not there twice
    Selection.Add(first);
    if (Selection.Count != 2 || !ReferenceEquals(Selection.Active, first)) { return 6; }

    Selection.Remove(first);
    if (Selection.Count != 1 || Selection.Contains(first) || !ReferenceEquals(Selection.Active, second)) { return 7; }

    Selection.Set(null);
    return Selection.Count == 0 && Selection.Active == null && Selection.Revision != after_set ? 1 : 8;
  }

  public static int SelectSecond()
  {
    Selection.Set(SceneObject.Find("Second"));
    return Selection.Count;
  }

  public static int SelectedCount() { return Selection.Count; }

  public static int AnEditIsOneStep()
  {
    if (Edits.CanUndo || Edits.CanRedo || Edits.HasUnsavedChanges) { return 2; }

    using (Edits.Begin("Rename"))
    {
      SceneObject.Find("First").Name = "Renamed";
    }
    if (!Edits.CanUndo || Edits.UndoName != "Rename" || !Edits.HasUnsavedChanges) { return 3; }

    /// an undo makes every object again: what was held before it is gone
    Edits.Undo();
    if (SceneObject.Find("Renamed") != null || SceneObject.Find("First") == null) { return 4; }
    if (Edits.CanUndo || !Edits.CanRedo) { return 5; }

    Edits.Redo();
    if (SceneObject.Find("Renamed") == null || SceneObject.Find("First") != null) { return 6; }

    /// a block inside a block is part of the one step; a block that changes nothing is none
    using (Edits.Begin("Two things"))
    {
      using (Edits.Begin("inner"))
      {
        SceneObject.Find("Second").AddTag("edited");
      }
      SceneObject.Find("Under").SetParent(null);
    }
    if (Edits.UndoName != "Two things") { return 7; }
    using (Edits.Begin("Nothing")) { }
    if (Edits.UndoName != "Two things") { return 8; }

    Edits.Undo();
    SceneObject under = SceneObject.Find("Under");
    return under != null && under.Parent != null && under.Parent.Name == "Second" && !SceneObject.Find("Second").HasTag("edited") ? 1 : 9;
  }

  /// one frame's worth of "this value is still being changed"
  public static int ChangeFirstsPlace(int x)
  {
    SceneObject.Find("First").Transform.Position = new Vec3(x, 0f, 0f);
    Edits.Changed();
    return Edits.CanUndo ? 1 : 0;
  }

  public static int FirstsPlace() { return (int)MathF.Round(SceneObject.Find("First").Transform.Position.X); }
  public static int Undo() { Edits.Undo(); return Edits.CanRedo ? 1 : 0; }

  public static int ComponentTypes()
  {
    IReadOnlyList<ComponentType> all = ComponentType.All;
    if (all.Count != 14 || all[0].Name != "Transform" || all[0].Removable || all[1].Name != "Scripts" || all[1].Removable) { return 2; }
    /// a kind's id is the id its class has, and the list is the same list each time
    ComponentType transform = ComponentType.Of<Transform>();
    ComponentType light = ComponentType.Of<PointLightComponent>();
    if (!ReferenceEquals(transform, all[0]) || !ReferenceEquals(ComponentType.All, all)) { return 3; }
    if (light == null || light.Name != "Point Light" || !light.Removable) { return 4; }

    SceneObject first = SceneObject.Find("First");
    if (!transform.IsOn(first) || light.IsOn(first)) { return 5; }
    if (!light.AddTo(first) || !light.IsOn(first) || light.AddTo(first)) { return 6; }

    var found = new List<ComponentType>();
    ComponentType.On(first, found);
    if (found.IndexOf(transform) != 0 || found.IndexOf(light) < 1) { return 7; }

    /// what every object keeps is not removed
    if (transform.RemoveFrom(first) || !transform.IsOn(first)) { return 8; }
    if (!light.RemoveFrom(first) || light.IsOn(first) || light.RemoveFrom(first)) { return 9; }
    return ComponentType.Find(12345) == null && !light.AddTo(null) ? 1 : 10;
  }

  public static int PlaybackFollows()
  {
    if (Playback.State != PlaybackState.Stopped) { return 2; }
    Playback.Play();
    if (Playback.State != PlaybackState.Playing) { return 3; }
    Playback.Pause();
    if (Playback.State != PlaybackState.Paused) { return 4; }
    Playback.Play();
    if (Playback.State != PlaybackState.Playing) { return 5; }
    Playback.Stop();
    return Playback.State == PlaybackState.Stopped ? 1 : 6;
  }

  public static int SaveScene() { return EditorScene.Save() ? 1 : 0; }

  private static int ran;

  /// leaves two commands behind, neither with keys or a menu item
  public static int CommandsRun()
  {
    ran = 0;
    bool allowed = false;
    Command count = Commands.Add("probe.count", "Count", () => ran++);
    if (count == null || !ReferenceEquals(Commands.Find("probe.count"), count) || count.Label != "Count" || !count.Chord.IsNone) { return 2; }
    if (!Commands.Run("probe.count") || ran != 1) { return 3; }
    /// a name is one command's, and a name that is none runs nothing
    if (Commands.Add("probe.count", "Again", () => ran += 10) != null || Commands.Run("probe.none") || ran != 1) { return 4; }

    Command gated = Commands.Add("probe.gated", "Gated", () => ran += 100, enabled: () => allowed);
    if (gated.Enabled || Commands.Run("probe.gated") || ran != 1) { return 5; }
    allowed = true;
    if (!gated.Run() || ran != 101) { return 6; }

    Command thrower = Commands.Add("probe.throws", "Throws", () => throw new InvalidOperationException("thrown on purpose"));
    if (thrower.Run()) { return 7; }

    /// keys that are one command's are not given to another
    var keys = new KeyChord(Key.K, KeyMods.Ctrl | KeyMods.Shift);
    Command keyed = Commands.Add("probe.keyed", "Keyed", () => { }, keys);
    Command same = Commands.Add("probe.same-keys", "Same", () => { }, keys);
    if (keyed.Chord.ToString() != "Ctrl+Shift+K" || !same.Chord.IsNone || new KeyChord().ToString() != "") { return 8; }

    thrower.Dispose();
    keyed.Dispose();
    same.Dispose();
    return Commands.Find("probe.throws") == null && Commands.All.Count == 2 ? 1 : 9;
  }

  public static int CommandCount() { return Commands.All.Count; }

  public static int SceneViewFollows()
  {
    if (SceneView.SceneName != "Editor Scene") { return 2; }
    IReadOnlyList<SceneViewItem> top = SceneView.Top;
    if (top.Count != 2 || top[0].Name != "First" || top[1].Name != "Second" || SceneView.All.Count != 3) { return 3; }
    SceneViewItem under = top[1].Children[0];
    if (under.Name != "Under" || under.Depth != 1 || !ReferenceEquals(under.Parent, top[1]) || !under.Visible) { return 4; }
    if (!ReferenceEquals(under.Object, SceneObject.Find("Under")) || !ReferenceEquals(SceneView.Find(under.Object), under)) { return 5; }

    /// nothing changed: the same items, not read again
    ulong read = SceneView.Revision;
    if (!ReferenceEquals(SceneView.Top[0], top[0]) || SceneView.Revision != read) { return 6; }
    /// moving an object is not a change to what a list shows
    top[0].Object.Transform.Position = new Vec3(4f, 5f, 6f);
    if (SceneView.Revision != read) { return 7; }

    top[0].Object.Name = "Renamed";
    if (SceneView.Revision == read || SceneView.Top[0].Name != "Renamed") { return 8; }

    under.Object.SetParent(SceneObject.Find("Renamed"));
    under.Object.Visible = false;
    SceneViewItem moved = SceneView.Top[0].Children[0];
    if (moved.Name != "Under" || moved.Visible || SceneView.Top[1].Children.Count != 0) { return 9; }

    SceneObject.Find("Second").Destroy();
    return SceneView.Top.Count == 1 && SceneView.All.Count == 2 ? 1 : 10;
  }
}
