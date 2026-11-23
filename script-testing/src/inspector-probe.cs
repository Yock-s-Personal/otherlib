using System.Collections.Generic;
using Other;
using Other.UI;

/// what the suite asks of the inspector's widgets, drawn with nobody at the mouse. the
///  scene has an object named "Drawn"
public static class InspectorProbe
{
  /// the rows of the object's behaviours. 1 when they say something was changed
  public static int DrawBehaviors()
  {
    return Widgets.Behaviors(SceneObject.Find("Drawn")) ? 1 : 0;
  }

  /// every other widget of the inspector: how many said something changed, and 100 more
  ///  for each value that came back other than it went in
  public static int DrawWidgets()
  {
    SceneObject drawn = SceneObject.Find("Drawn");
    int changed = 0;

    string name = "Drawn";
    if (Widgets.ObjectHeader(ref name, drawn.ObjectID)) { changed++; }
    if (name != "Drawn") { changed += 100; }

    IReadOnlyList<ComponentType> types = ComponentType.All;
    for (int i = 0; i < types.Count; i++)
    {
      if (Widgets.BeginComponentSection(types[i], out bool remove))
      {
        /// with no driver there is nothing to draw a component's rows with: none are drawn
        if (Widgets.ComponentFields(drawn, types[i])) { changed++; }
      }
      Widgets.EndComponentSection();
      if (remove) { changed++; }
    }

    bool flag = true;
    float speed = 1.5f;
    int whole = -3;
    uint uwhole = 3;
    long big = -4;
    ulong ubig = 4;
    var two = new Vec2(1f, 2f);
    var three = new Vec3(1f, 2f, 3f);
    var four = new Vec4(1f, 2f, 3f, 4f);
    string text = "some text";
    if (Widgets.PropertyBool("flag", ref flag)) { changed++; }
    if (Widgets.PropertyFloat("speed", ref speed)) { changed++; }
    if (Widgets.PropertyInt("whole", ref whole)) { changed++; }
    if (Widgets.PropertyUInt("uwhole", ref uwhole)) { changed++; }
    if (Widgets.PropertyLong("big", ref big)) { changed++; }
    if (Widgets.PropertyULong("ubig", ref ubig)) { changed++; }
    if (Widgets.PropertyVec2("two", ref two)) { changed++; }
    if (Widgets.PropertyVec3("three", ref three)) { changed++; }
    if (Widgets.PropertyVec4("four", ref four)) { changed++; }
    if (Widgets.PropertyText("text", ref text)) { changed++; }
    if (!flag || speed != 1.5f || whole != -3 || uwhole != 3 || big != -4 || ubig != 4) { changed += 100; }
    if (two.Y != 2f || three.Z != 3f || four.W != 4f || text != "some text") { changed += 100; }

    if (Widgets.AddComponentButton("Add Component")) { changed++; }
    if (Widgets.AssetDropTarget() != null) { changed++; }
    return changed;
  }

  /// the kinds an object has, as an inspector lists them, with what each is called natively
  public static int ScriptsKind()
  {
    ComponentType scripts = ComponentType.Scripts;
    if (scripts == null || scripts.Name != "Scripts" || scripts.NativeName != "script_component" || scripts.Removable) { return 2; }
    ComponentType transform = ComponentType.Of<Transform>();
    if (transform.NativeName != "transform" || transform.Color.W <= 0f || transform.Color.Equals(scripts.Color)) { return 3; }
    return scripts.IsOn(SceneObject.Find("Drawn")) ? 1 : 4;
  }
}
