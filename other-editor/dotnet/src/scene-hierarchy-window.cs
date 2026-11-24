using System;
using System.Collections.Generic;
using Other.UI;

namespace Other.Editor
{
  /// the scene's objects as a tree. a click selects, ctrl-click adds to what is selected, a
  ///  double click or F2 renames, Del deletes, a row dragged onto another goes under it (onto
  ///  the scene's own row: to the top), the eye hides, and a right click has the rest
  [EditorWindow("scene-hierarchy", Title = "Scene Hierarchy", DefaultWidth = 300f, DefaultHeight = 500f)]
  public class SceneHierarchyWindow : UIWindow
  {
    private const int kNameLength = 128;
    private const float kEyeWidth = 18f;
    private const float kIndent = 12f;
    private const TreeNodeFlags kRowFlags = TreeNodeFlags.OpenOnArrow | TreeNodeFlags.SpanAvailWidth | TreeNodeFlags.AllowOverlap | TreeNodeFlags.DrawLinesToNodes;

    private string filter = "";
    /// what is drawn while there is a filter: an object whose name has it, and what such an
    ///  object is under. made again only when the filter or the scene changes
    private readonly HashSet<SceneViewItem> shown = new();
    private string shown_filter = "";
    private ulong shown_revision;

    /// what is open, by an object's place in the tree and not by its id: an undo or a stop
    ///  makes every object again under another id, and the tree should not shut for it
    private readonly HashSet<ulong> open = new();

    /// the selection as it was last looked at, and as this window last left it: a change
    ///  from anywhere else is scrolled to
    private ulong seen_selection;
    private ulong own_selection;
    private SceneObject? scroll_to;

    /// what a row asked for. it is done after the tree is drawn: doing it at once would
    ///  change the list that is being walked
    private enum Act
    {
      None,
      Select,
      ToggleSelected,
      MoveUnder,
      ToggleVisible,
      Create,
      Delete,
      DeleteSelected,
      Rename,
    }

    private Act asked;
    private SceneObject? asked_of;
    private SceneObject? asked_with;

    private SceneObject? renaming;
    private string rename_text = "";
    private bool open_rename;
    private readonly List<SceneObject> doomed = new();

    private Vec4 eye_visible;
    private Vec4 eye_hidden;
    private Vec4 eye_hovered;
    private Vec4 text_hidden;

    protected override void OnRenderBody()
    {
      if (Scene.Revision == 0)
      {
        UIWidgets.TextDisabled("No active scene");
        return;
      }

      eye_visible = Theme.Color("hierarchy.icon_visible");
      eye_hidden = Theme.Color("hierarchy.icon_hidden");
      eye_hovered = Theme.Color("core.text_bright");
      text_hidden = Theme.Color("hierarchy.item_text_disabled");

      UIWidgets.SetNextItemWidth(UIWidgets.GetContentRegionAvail().X);
      UIWidgets.InputTextWithHint("##filter", "filter objects...", ref filter, kNameLength);
      RefreshShown();
      RevealSelection();

      UIWidgets.PushStyleColor(StyleColor.Header, Theme.Color("hierarchy.item_selected"));
      UIWidgets.PushStyleColor(StyleColor.HeaderHovered, Theme.Color("hierarchy.item_hover"));
      UIWidgets.PushStyleColor(StyleColor.HeaderActive, Theme.Color("hierarchy.item_selected"));
      UIWidgets.PushStyleVar(StyleVar.IndentSpacing, kIndent);
      UIWidgets.PushStyleVar(StyleVar.ItemSpacing, new Vec2(4f, 1f));

      /// the scene's own row: what is dropped on it goes to the top of the scene
      string scene_name = SceneView.SceneName;
      bool scene_open = UIWidgets.TreeNodeEx(scene_name.Length > 0 ? scene_name : "(scene)", TreeNodeFlags.DefaultOpen | TreeNodeFlags.OpenOnArrow | TreeNodeFlags.SpanAvailWidth | TreeNodeFlags.DrawLinesToNodes);
      SceneObject? to_top = Widgets.ObjectDropTarget();
      if (to_top != null)
      {
        Ask(Act.MoveUnder, to_top, null);
      }
      if (scene_open)
      {
        IReadOnlyList<SceneViewItem> top = SceneView.Top;
        for (int i = 0; i < top.Count; i++)
        {
          DrawItem(top[i]);
        }
        UIWidgets.TreePop();
      }

      UIWidgets.PopStyleVar(2);
      UIWidgets.PopStyleColor(3);
      scroll_to = null;

      if (UIWidgets.BeginPopupContextWindow("##hierarchy-menu", PopupFlags.MouseButtonRight | PopupFlags.NoOpenOverItems))
      {
        if (UIWidgets.MenuItem("Create Empty Object"))
        {
          Ask(Act.Create, null, null);
        }
        UIWidgets.EndPopup();
      }

      /// only while this window has the focus, and not while a name is being typed
      if (renaming == null && !UIWidgets.WantTextInput && Selection.Count > 0)
      {
        if (UIWidgets.Shortcut(Key.Delete))
        {
          Ask(Act.DeleteSelected, null, null);
        }
        else if (UIWidgets.Shortcut(Key.F2))
        {
          Ask(Act.Rename, Selection.Active, null);
        }
      }

      DrawRename();
      Do();
    }

    private void Ask(Act what, SceneObject? of, SceneObject? with)
    {
      asked = what;
      asked_of = of;
      asked_with = with;
    }

    private void DrawItem(SceneViewItem item)
    {
      if (filter.Length > 0 && !shown.Contains(item))
      {
        return;
      }

      SceneObject target = item.Object;
      bool has_children = item.Children.Count > 0;
      bool selected = Selection.Contains(target);
      TreeNodeFlags flags = kRowFlags;
      if (!has_children)
      {
        flags |= TreeNodeFlags.Leaf | TreeNodeFlags.NoTreePushOnOpen;
      }
      if (selected)
      {
        flags |= TreeNodeFlags.Selected;
      }
      if (has_children)
      {
        /// a filter opens everything, so that what it found under a shut row shows
        UIWidgets.SetNextItemOpen(filter.Length > 0 || open.Contains(item.PathKey), Condition.Always);
      }

      float row_right = UIWidgets.CursorPos.X + UIWidgets.GetContentRegionAvail().X;
      UIWidgets.PushID((int)target.ObjectID);
      if (!item.Visible)
      {
        UIWidgets.PushStyleColor(StyleColor.Text, text_hidden);
      }
      bool is_open = UIWidgets.TreeNodeEx(item.Name, flags);
      if (!item.Visible)
      {
        UIWidgets.PopStyleColor();
      }
      float row_height = UIWidgets.ItemRectSize.Y;

      if (scroll_to != null && ReferenceEquals(scroll_to, target))
      {
        UIWidgets.SetScrollHereY();
        scroll_to = null;
      }

      if (has_children && UIWidgets.IsItemToggledOpen())
      {
        if (!open.Remove(item.PathKey))
        {
          open.Add(item.PathKey);
        }
      }
      else if (UIWidgets.IsItemClicked())
      {
        Ask((UIWidgets.KeyMods & KeyMods.Ctrl) != 0 ? Act.ToggleSelected : Act.Select, target, null);
      }
      if (UIWidgets.IsItemHovered() && UIWidgets.IsMouseDoubleClicked())
      {
        Ask(Act.Rename, target, null);
      }
      if (UIWidgets.IsItemClicked(MouseButton.Right) && !selected)
      {
        Ask(Act.Select, target, null);
      }

      /// these three are about the row, and the row has to be the last thing drawn for each
      SceneObject? dropped = Widgets.ObjectDropTarget();
      if (dropped != null && !ReferenceEquals(dropped, target))
      {
        Ask(Act.MoveUnder, dropped, target);
      }
      Widgets.ObjectDragSource(target);
      if (UIWidgets.BeginPopupContextItem("##object-menu"))
      {
        UIWidgets.TextDisabled(item.Name);
        UIWidgets.Separator();
        if (UIWidgets.MenuItem("Create Child"))
        {
          Ask(Act.Create, target, null);
        }
        if (UIWidgets.MenuItem("Rename", "F2"))
        {
          Ask(Act.Rename, target, null);
        }
        if (UIWidgets.MenuItem("Delete", "Del"))
        {
          Ask(Act.Delete, target, null);
        }
        UIWidgets.EndPopup();
      }

      /// the eye, over the right end of the row
      UIWidgets.SameLine(row_right - kEyeWidth);
      if (UIWidgets.InvisibleButton("##eye", kEyeWidth, row_height))
      {
        Ask(Act.ToggleVisible, target, null);
      }
      DrawEye(UIWidgets.ItemRectMin, UIWidgets.ItemRectMax, item.Visible, UIWidgets.IsItemHovered());

      if (has_children && is_open)
      {
        IReadOnlyList<SceneViewItem> children = item.Children;
        for (int i = 0; i < children.Count; i++)
        {
          DrawItem(children[i]);
        }
        UIWidgets.TreePop();
      }
      UIWidgets.PopID();
    }

    private void DrawEye(Vec2 min, Vec2 max, bool visible, bool hovered)
    {
      Vec2 middle = new Vec2((min.X + max.X) * 0.5f, (min.Y + max.Y) * 0.5f);
      Vec4 color = hovered ? eye_hovered : visible ? eye_visible : eye_hidden;
      if (!visible)
      {
        DrawList.Line(new Vec2(middle.X - 4f, middle.Y), new Vec2(middle.X + 4f, middle.Y), color, 1.2f);
        return;
      }

      Vec2 left = new Vec2(middle.X - 6f, middle.Y);
      Vec2 right = new Vec2(middle.X + 6f, middle.Y);
      DrawList.BezierCubic(left, new Vec2(middle.X - 3f, middle.Y - 5f), new Vec2(middle.X + 3f, middle.Y - 5f), right, color, 1.2f);
      DrawList.BezierCubic(left, new Vec2(middle.X - 3f, middle.Y + 5f), new Vec2(middle.X + 3f, middle.Y + 5f), right, color, 1.2f);
      DrawList.CircleFilled(middle, 1.8f, color);
    }

    private void RefreshShown()
    {
      if (filter.Length == 0)
      {
        if (shown_filter.Length > 0)
        {
          shown.Clear();
          shown_filter = "";
        }
        return;
      }

      ulong revision = SceneView.Revision;
      if (filter == shown_filter && revision == shown_revision)
      {
        return;
      }
      shown.Clear();
      shown_filter = filter;
      shown_revision = revision;
      IReadOnlyList<SceneViewItem> top = SceneView.Top;
      for (int i = 0; i < top.Count; i++)
      {
        Matches(top[i]);
      }
    }

    private bool Matches(SceneViewItem item)
    {
      bool any = item.Name.Contains(filter, StringComparison.OrdinalIgnoreCase);
      IReadOnlyList<SceneViewItem> children = item.Children;
      for (int i = 0; i < children.Count; i++)
      {
        any |= Matches(children[i]);
      }
      if (any)
      {
        shown.Add(item);
      }
      return any;
    }

    /// when what is selected changes, the rows over the active object open; and it is
    ///  scrolled to when the change was made somewhere else
    private void RevealSelection()
    {
      ulong revision = Selection.Revision;
      if (revision == seen_selection)
      {
        return;
      }
      seen_selection = revision;

      SceneObject? active = Selection.Active;
      SceneViewItem? item = active != null ? SceneView.Find(active) : null;
      if (item == null)
      {
        return;
      }
      for (SceneViewItem? over = item.Parent; over != null; over = over.Parent)
      {
        open.Add(over.PathKey);
      }
      if (revision != own_selection)
      {
        scroll_to = active;
      }
    }

    private void OpenRow(SceneObject? item)
    {
      SceneViewItem? row = item != null ? SceneView.Find(item) : null;
      if (row != null)
      {
        open.Add(row.PathKey);
      }
    }

    private void Do()
    {
      Act what = asked;
      SceneObject? of = asked_of;
      SceneObject? with = asked_with;
      Ask(Act.None, null, null);
      if (what == Act.None || (of != null && !of.IsAlive) || (with != null && !with.IsAlive))
      {
        return;
      }

      switch (what)
      {
        case Act.Select:
          Selection.Set(of);
          own_selection = Selection.Revision;
          break;

        case Act.ToggleSelected:
          if (Selection.Contains(of!))
          {
            Selection.Remove(of!);
          }
          else
          {
            Selection.Add(of!);
          }
          own_selection = Selection.Revision;
          break;

        case Act.MoveUnder:
        {
          bool moved;
          using (Edits.Begin("Move"))
          {
            moved = of!.SetParent(with);
          }
          if (moved)
          {
            OpenRow(with);
          }
          break;
        }

        case Act.ToggleVisible:
          using (Edits.Begin(of!.Visible ? "Hide" : "Show"))
          {
            of.Visible = !of.Visible;
          }
          break;

        case Act.Create:
        {
          SceneObject made;
          using (Edits.Begin("Create Object"))
          {
            made = SceneObject.Create("New Object", of);
          }
          OpenRow(of);
          Selection.Set(made);
          own_selection = Selection.Revision;
          break;
        }

        case Act.Delete:
          using (Edits.Begin("Delete"))
          {
            Selection.Remove(of!);
            of!.Destroy();
          }
          break;

        case Act.DeleteSelected:
        {
          doomed.Clear();
          IReadOnlyList<SceneObject> selected = Selection.Objects;
          for (int i = 0; i < selected.Count; i++)
          {
            doomed.Add(selected[i]);
          }
          using (Edits.Begin("Delete"))
          {
            Selection.Clear();
            for (int i = 0; i < doomed.Count; i++)
            {
              /// one that was under another of them went with it
              if (doomed[i].IsAlive)
              {
                doomed[i].Destroy();
              }
            }
          }
          doomed.Clear();
          break;
        }

        case Act.Rename:
          if (of != null)
          {
            renaming = of;
            rename_text = of.Name;
            open_rename = true;
          }
          break;

        default:
          break;
      }
    }

    /// the name is typed in a popup at the mouse. it is opened here, at the window's own
    ///  level: opened from inside the menu that asked for it, it would shut with that menu
    private void DrawRename()
    {
      if (open_rename)
      {
        UIWidgets.OpenPopup("##rename");
        open_rename = false;
      }
      if (renaming == null)
      {
        return;
      }
      if (!renaming.IsAlive)
      {
        renaming = null;
        return;
      }

      UIWidgets.SetNextWindowPos(UIWidgets.MousePos, Condition.Appearing);
      if (!UIWidgets.BeginPopup("##rename"))
      {
        /// shut by a click somewhere else
        renaming = null;
        return;
      }

      if (UIWidgets.IsWindowAppearing())
      {
        UIWidgets.SetKeyboardFocusHere();
      }
      if (UIWidgets.InputText("##name", ref rename_text, kNameLength, InputTextFlags.EnterReturnsTrue | InputTextFlags.AutoSelectAll))
      {
        if (rename_text.Length > 0 && rename_text != renaming.Name)
        {
          using (Edits.Begin("Rename"))
          {
            renaming.Name = rename_text;
          }
        }
        renaming = null;
        UIWidgets.CloseCurrentPopup();
      }
      else if (UIWidgets.IsKeyPressed(Key.Escape, false))
      {
        renaming = null;
        UIWidgets.CloseCurrentPopup();
      }
      UIWidgets.EndPopup();
    }
  }
}
