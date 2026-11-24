using Other;
using Other.UI;

namespace Other.Editor
{
  /// a click in the Scene View selects what is drawn there. it always runs, and after the
  ///  other tools, so that a click one of them took is theirs
  [SceneTool("select", Title = "Select", Always = true, Order = 1000)]
  public class SelectTool : SceneTool
  {
    public override void OnSceneView()
    {
      if (!SceneViewport.Clicked)
      {
        return;
      }

      SceneObject? under = SceneViewport.ObjectUnderPointer;
      KeyMods mods = UIWidgets.KeyMods;
      if ((mods & KeyMods.Ctrl) != 0)
      {
        /// in or out of the selection, and the rest of it left as it is
        if (under == null)
        {
          return;
        }
        if (Selection.Contains(under))
        {
          Selection.Remove(under);
        }
        else
        {
          Selection.Add(under);
        }
      }
      else if ((mods & KeyMods.Shift) != 0)
      {
        if (under != null)
        {
          Selection.Add(under);
        }
      }
      else
      {
        /// nothing under the pointer is nothing selected
        Selection.Set(under);
      }
    }
  }
}
