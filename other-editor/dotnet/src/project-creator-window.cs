using System.IO;
using Other.Core;
using Other.UI;

namespace Other.Editor
{
  /// makes a project: a name, the folder it goes in, who made it and what it is. what is
  ///  made is what `oecli create` makes. the editor opens it when it has no project open
  ///  and the project was made in the folder the editor works from
  [EditorWindow("project-creator", Title = "Project Creator", DefaultWidth = 520f, DefaultHeight = 260f)]
  public class ProjectCreatorWindow : UIWindow
  {
    private const int kPathLength = 512;

    private string name = "";
    private string folder = "";
    private string author = "";
    private string description = "";
    /// why the last attempt made nothing; empty when it made the project
    private string error = "";
    private string made = "";

    protected override void OnInitialize()
    {
      /// the folder this editor works from: a project made there is one it can open
      folder = Directory.GetCurrentDirectory();
    }

    protected override void OnRenderBody()
    {
      UIWidgets.SeparatorText("New Project");
      Widgets.PropertyText("Name", ref name);

      Widgets.BeginPropertyRow("Folder");
      UIWidgets.SetNextItemWidth(UIWidgets.GetContentRegionAvail().X - 90f);
      UIWidgets.InputText("##folder", ref folder, kPathLength);
      UIWidgets.SameLine();
      if (UIWidgets.Button("Browse..."))
      {
        Dialogs.PickFolder(picked => folder = picked);
      }
      Widgets.EndPropertyRow();

      Widgets.PropertyText("Author", ref author);
      Widgets.PropertyText("Description", ref description);
      UIWidgets.TextDisabled("The project is made in a folder of its name inside that folder.");

      UIWidgets.Spacing();
      UIWidgets.BeginDisabled(name.Length == 0 || folder.Length == 0);
      if (UIWidgets.Button("Create"))
      {
        made = "";
        if (Projects.Create(name, folder, author, description, out error))
        {
          string why = Project.IsOpen ? "a project is open here" : "it is not in the folder this editor works from";
          made = Projects.OpeningLastCreated
            ? $"Made {Projects.LastCreated}. The editor is opening it."
            : $"Made {Projects.LastCreated}. It was not opened, since {why}. From the folder it is in, oecli open opens it.";
        }
      }
      UIWidgets.EndDisabled();

      if (error.Length > 0)
      {
        UIWidgets.PushStyleColor(StyleColor.Text, Theme.Color("core.friendly_error_red"));
        UIWidgets.TextWrapped(error);
        UIWidgets.PopStyleColor();
      }
      else if (made.Length > 0)
      {
        UIWidgets.TextWrapped(made);
      }
    }
  }
}
