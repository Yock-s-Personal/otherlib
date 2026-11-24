using Other.UI;

namespace Other.Editor
{
  /// the buttons at the top left of the Scene View: which tool is the active one, and
  ///  which axes its handles are along. the keys for the same are the editor's commands
  [SceneTool("toolbar", Title = "Toolbar", Always = true, Order = -100)]
  public class SceneToolbar : SceneTool
  {
    private static readonly Vec4 kChosen = new Vec4(0.93f, 0.62f, 0.14f, 0.85f);
    private static readonly string[] kTools = { "select", "move", "rotate", "scale" };
    private static readonly string[] kLabels = { "Select", "Move", "Rotate", "Scale" };
    private static readonly string[] kKeys = { "Q", "W", "E", "R" };

    public override void OnSceneView()
    {
      Vec2 at = SceneViewport.Position;
      UIWidgets.CursorScreenPos = new Vec2(at.X + 8f, at.Y + 8f);

      string active = SceneTools.Active?.Name ?? "select";
      for (int i = 0; i < kTools.Length; i++)
      {
        if (i > 0)
        {
          UIWidgets.SameLine(0f, 4f);
        }
        bool chosen = kTools[i] == active;
        if (chosen)
        {
          UIWidgets.PushStyleColor(StyleColor.Button, kChosen);
        }
        if (UIWidgets.Button(kLabels[i]))
        {
          SceneTools.Activate(kTools[i]);
        }
        if (chosen)
        {
          UIWidgets.PopStyleColor();
        }
        if (UIWidgets.IsItemHovered())
        {
          UIWidgets.SetTooltip($"{kLabels[i]} ({kKeys[i]})");
        }
      }

      UIWidgets.SameLine(0f, 12f);
      if (UIWidgets.Button(SceneTools.WorldAxes ? "World" : "Local"))
      {
        SceneTools.WorldAxes = !SceneTools.WorldAxes;
      }
      if (UIWidgets.IsItemHovered())
      {
        UIWidgets.SetTooltip("The handles are along the world's axes, or the active object's own (X). Hold Ctrl while dragging to go in steps.");
      }
    }
  }
}
