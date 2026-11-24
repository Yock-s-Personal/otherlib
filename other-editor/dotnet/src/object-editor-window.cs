using System;
using System.Collections.Generic;
using Other.UI;

namespace Other.Editor
{
  /// the selected object: its name, a section for each component it has, its scripts, and a
  ///  way to add a component. the sections and their rows are the inspector's own widgets;
  ///  this window puts them in order and tells the editor what was changed
  [EditorWindow("object-editor", Title = "Object Editor", DefaultWidth = 420f, DefaultHeight = 700f)]
  public class ObjectEditorWindow : UIWindow
  {
    private const int kSearchLength = 64;

    /// the object shown and its name, kept: asking for a name makes a string
    private SceneObject? shown;
    private ulong shown_revision;
    private string name = "";

    private readonly List<ComponentType> components = new();
    private string search = "";
    private int counted = -1;
    private string counted_text = "";

    protected override void OnRenderBody()
    {
      if (Scene.Revision == 0)
      {
        UIWidgets.TextColored(Theme.Color("core.text_friendly_alert"), "No active scene.");
        return;
      }

      int selected = Selection.Count;
      SceneObject? item = selected == 1 ? Selection.Active : null;
      if (selected > 1)
      {
        if (selected != counted)
        {
          counted = selected;
          counted_text = $"Multiple objects selected ({selected}).";
        }
        UIWidgets.TextColored(Theme.Color("core.text_bright"), counted_text);
        return;
      }
      if (item == null || !item.IsAlive)
      {
        UIWidgets.TextColored(Theme.Color("core.text_friendly_alert"), "No object selected.");
        return;
      }

      if (!ReferenceEquals(item, shown) || SceneView.Revision != shown_revision)
      {
        shown = item;
        shown_revision = SceneView.Revision;
        name = SceneView.Find(item)?.Name ?? item.Name;
      }
      if (Widgets.ObjectHeader(ref name, item.ObjectID) && name.Length > 0)
      {
        using (Edits.Begin("Rename"))
        {
          item.Name = name;
        }
      }
      UIWidgets.Separator();

      ComponentType? scripts = ComponentType.Scripts;
      ComponentType? removed = null;
      ComponentType.On(item, components);
      for (int i = 0; i < components.Count; i++)
      {
        ComponentType type = components[i];
        if (Widgets.BeginComponentSection(type, out bool remove))
        {
          /// a value that is dragged changes every frame: it is one undo step once it stops
          bool changed = ReferenceEquals(type, scripts) ? Widgets.Behaviors(item) : Widgets.ComponentFields(item, type);
          if (changed)
          {
            Edits.Changed();
          }
        }
        Widgets.EndComponentSection();
        if (remove)
        {
          removed = type;
        }
      }

      if (removed != null)
      {
        using (Edits.Begin("Remove Component"))
        {
          removed.RemoveFrom(item);
        }
      }

      DrawAddComponent(item);
    }

    private void DrawAddComponent(SceneObject item)
    {
      if (Widgets.AddComponentButton("Add Component"))
      {
        search = "";
        UIWidgets.OpenPopup("##add-component");
      }

      UIWidgets.SetNextWindowSize(300f, 0f);
      if (!UIWidgets.BeginPopup("##add-component"))
      {
        return;
      }

      if (UIWidgets.IsWindowAppearing())
      {
        UIWidgets.SetKeyboardFocusHere();
      }
      UIWidgets.SetNextItemWidth(UIWidgets.GetContentRegionAvail().X);
      UIWidgets.InputTextWithHint("##search", "search components...", ref search, kSearchLength);
      UIWidgets.Separator();

      if (UIWidgets.BeginChild("##list", 0f, 220f))
      {
        int listed = 0;
        IReadOnlyList<ComponentType> all = ComponentType.All;
        for (int i = 0; i < all.Count; i++)
        {
          ComponentType type = all[i];
          if (type.IsOn(item) || (search.Length > 0 && !type.Name.Contains(search, StringComparison.OrdinalIgnoreCase)))
          {
            continue;
          }
          listed++;
          if (UIWidgets.Selectable(type.Name))
          {
            using (Edits.Begin("Add Component"))
            {
              type.AddTo(item);
            }
            UIWidgets.CloseCurrentPopup();
          }
        }
        if (listed == 0)
        {
          UIWidgets.TextDisabled("no matching components");
        }
      }
      UIWidgets.EndChild();
      UIWidgets.EndPopup();
    }
  }
}
