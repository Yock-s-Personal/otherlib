using System.Collections.Generic;
using Other.UI;

namespace Other.Editor
{
  /// packages the open project: the choices a compile takes, what it will build from, a
  ///  button that runs it, and what came of it. the compile itself is the editor's
  ///  (Compile); this window is what is drawn over it
  [EditorWindow("project-compiler", Title = "Compile Project", DefaultWidth = 560f, DefaultHeight = 680f)]
  public class ProjectCompilerWindow : UIWindow
  {
    private const int kNameLength = 128;
    private const int kPathLength = 512;
    private const float kListHeight = 110f;
    private const string kUnsavedPrompt = "Unsaved Scene";
    private const ChildFlags kListFlags = ChildFlags.Borders | ChildFlags.ResizeY;

    /// what is typed and chosen here. it is filled from the project when another project
    ///  is shown, and kept while the same one is read again
    private string shown_project = "";
    private string public_name = "";
    private string output_folder = "";
    private string starting_scene = "";
    private string main_pipeline = "";
    private bool debug_package;

    /// text made from the compile, made again only when the compile says it changed
    private bool read;
    private ulong shown_revision;
    private string left_out_text = "";
    private string was_left_out_text = "";
    private string packaged_text = "";

    /// why Compile is not offered, worked out again only when what it depends on changes
    private string reason = "";
    private string reason_name = "";
    private string name_problem = "";
    /// why a compile could not be started; cleared by the next attempt
    private string start_error = "";

    private Vec4 error_red;

    protected override void OnRenderBody()
    {
      CompileState state = Compile.Poll();
      if (state == CompileState.NoProject)
      {
        UIBindings.TextDisabled("No project is open.");
        return;
      }
      if (state == CompileState.Loading)
      {
        UIBindings.TextDisabled("The project is still loading.");
        return;
      }

      error_red = Theme.Color("core.friendly_error_red");
      if (!read || Compile.Revision != shown_revision)
      {
        read = true;
        shown_revision = Compile.Revision;
        Refresh();
      }

      DrawChoices();
      DrawContents();
      DrawCompile();
      DrawResult();
    }

    private void Refresh()
    {
      /// what was typed for one project is not carried to the next
      if (Compile.ProjectFile != shown_project)
      {
        shown_project = Compile.ProjectFile;
        public_name = Compile.PublicName;
        output_folder = Compile.DefaultOutputFolder;
        starting_scene = "";
        main_pipeline = "";
      }
      if (!Contains(Compile.Scenes, starting_scene))
      {
        starting_scene = Compile.StartingScene;
      }
      if (!Contains(Compile.Pipelines, main_pipeline))
      {
        main_pipeline = Compile.MainPipeline;
      }

      left_out_text = $"{Compile.LeftOut.Count} file(s) that nothing in the project uses will be left out ({Compile.LeftOutSize}). Name the ones the game loads by path in [package] keep.";
      was_left_out_text = $"{Compile.WasLeftOut.Count} file(s) that nothing in the project uses were left out ({Compile.WasLeftOutSize}). Name the ones the game loads by path in [package] keep.";
      packaged_text = $"Packaged as {Compile.Package}";
      reason_name = "\0";
    }

    private static bool Contains(IReadOnlyList<string> names, string name)
    {
      for (int i = 0; i < names.Count; i++)
      {
        if (names[i] == name)
        {
          return true;
        }
      }
      return false;
    }

    /// a combo over names. true when another was chosen
    private static bool NameCombo(string id, IReadOnlyList<string> names, ref string chosen)
    {
      bool changed = false;
      UIBindings.SetNextItemWidth(UIBindings.GetContentRegionAvail().X);
      if (UIBindings.BeginCombo(id, chosen.Length == 0 ? "choose one" : chosen))
      {
        for (int i = 0; i < names.Count; i++)
        {
          if (UIBindings.Selectable(names[i], names[i] == chosen) && names[i] != chosen)
          {
            chosen = names[i];
            changed = true;
          }
        }
        UIBindings.EndCombo();
      }
      return changed;
    }

    private void ErrorText(string text)
    {
      UIBindings.PushStyleColor(StyleColor.Text, error_red);
      UIBindings.TextWrapped(text);
      UIBindings.PopStyleColor();
    }

    private void DrawChoices()
    {
      UIBindings.SeparatorText("Package");
      Widgets.PropertyDisplay("Project", shown_project);
      Widgets.PropertyText("Public name", ref public_name);

      Widgets.BeginPropertyRow("Output folder");
      UIBindings.SetNextItemWidth(UIBindings.GetContentRegionAvail().X - 90f);
      UIBindings.InputText("##output-folder", ref output_folder, kPathLength);
      UIBindings.SameLine();
      if (UIBindings.Button("Browse..."))
      {
        Dialogs.PickFolder(folder => output_folder = folder);
      }
      Widgets.EndPropertyRow();

      Widgets.BeginPropertyRow("Configuration");
      if (UIBindings.RadioButton("Release", !debug_package))
      {
        debug_package = false;
      }
      UIBindings.SameLine();
      if (UIBindings.RadioButton("Debug", debug_package))
      {
        debug_package = true;
      }
      Widgets.EndPropertyRow();

      Widgets.BeginPropertyRow("Starting scene");
      NameCombo("##starting-scene", Compile.Scenes, ref starting_scene);
      Widgets.EndPropertyRow();

      /// with one pipeline there is nothing to choose
      if (Compile.Pipelines.Count > 1)
      {
        Widgets.BeginPropertyRow("Main pipeline");
        NameCombo("##main-pipeline", Compile.Pipelines, ref main_pipeline);
        Widgets.EndPropertyRow();
      }
      else
      {
        Widgets.PropertyDisplay("Main pipeline", main_pipeline.Length == 0 ? "none" : main_pipeline);
      }
    }

    private void DrawContents()
    {
      UIBindings.SeparatorText("Boot Program");
      IReadOnlyList<string> sources = Compile.VmSources;
      if (sources.Count == 0)
      {
        UIBindings.TextDisabled("The project has no VM sources. The boot program only loads the project.");
      }
      else
      {
        if (UIBindings.BeginChild("##vm-sources", 0f, kListHeight, kListFlags))
        {
          Bullets(sources);
          Bullets(Compile.Hooks);
        }
        UIBindings.EndChild();
      }

      IReadOnlyList<string> problems = Compile.Problems;
      for (int i = 0; i < problems.Count; i++)
      {
        UIBindings.Bullet();
        ErrorText(problems[i]);
      }

      UIBindings.SeparatorText("Left Out");
      IReadOnlyList<string> left_out = Compile.LeftOut;
      if (left_out.Count == 0)
      {
        UIBindings.TextDisabled("Every model, texture, material, sound and scene of the project is used.");
      }
      else
      {
        UIBindings.TextWrapped(left_out_text);
        if (UIBindings.BeginChild("##will-be-left-out", 0f, kListHeight, kListFlags))
        {
          Lines(left_out);
        }
        UIBindings.EndChild();
      }
      if (UIBindings.SmallButton("Refresh"))
      {
        Compile.Refresh();
      }
    }

    private static void Bullets(IReadOnlyList<string> lines)
    {
      for (int i = 0; i < lines.Count; i++)
      {
        UIBindings.Bullet();
        UIBindings.Text(lines[i]);
      }
    }

    private static void Lines(IReadOnlyList<string> lines)
    {
      for (int i = 0; i < lines.Count; i++)
      {
        UIBindings.Text(lines[i]);
      }
    }

    /// why Compile is not offered; empty when it is
    private string BlockingReason()
    {
      if (Compile.Problems.Count > 0)
      {
        return "The project has problems that stop a compile.";
      }
      /// the name is asked about only when it is another name: the answer is a new string
      if (public_name != reason_name)
      {
        reason_name = public_name;
        string problem = Compile.NameProblem(public_name);
        name_problem = problem.Length == 0 ? "" : $"The public name cannot be used: {problem}.";
      }
      if (name_problem.Length > 0)
      {
        return name_problem;
      }
      if (output_folder.Length == 0)
      {
        return "Choose an output folder.";
      }
      if (starting_scene.Length == 0)
      {
        return "Choose the scene the game starts in.";
      }
      return main_pipeline.Length == 0 ? "Choose the pipeline whose image reaches the window." : "";
    }

    private void StartCompile()
    {
      if (!Compile.Start(public_name, output_folder, starting_scene, main_pipeline, debug_package, out string error))
      {
        start_error = error;
      }
    }

    private void DrawCompile()
    {
      UIBindings.SeparatorText("Compile");
      reason = BlockingReason();
      bool running = Compile.Running;
      UIBindings.BeginDisabled(running || reason.Length > 0);
      if (UIBindings.Button(running ? "Compiling..." : "Compile"))
      {
        start_error = "";
        /// a package is made from the files on disk, so a scene with edits that are only in
        ///  memory is saved first or nothing is compiled
        if (Edits.HasUnsavedChanges)
        {
          UIBindings.OpenPopup(kUnsavedPrompt);
        }
        else
        {
          StartCompile();
        }
      }
      UIBindings.EndDisabled();
      DrawUnsavedPrompt();

      if (running)
      {
        bool cancelling = Compile.Cancelling;
        UIBindings.SameLine();
        UIBindings.BeginDisabled(cancelling);
        if (UIBindings.Button(cancelling ? "Cancelling..." : "Cancel"))
        {
          Compile.Cancel();
        }
        UIBindings.EndDisabled();
        UIBindings.ProgressBar(Compile.Fraction, -1f, 0f, Compile.StageLabel);
      }

      if (!running && reason.Length > 0)
      {
        UIBindings.SameLine();
        UIBindings.TextDisabled(reason);
      }
      if (start_error.Length > 0)
      {
        ErrorText(start_error);
      }
    }

    /// asks before a compile of a scene with edits that are not on disk: save, or no compile
    private void DrawUnsavedPrompt()
    {
      if (!UIBindings.BeginPopupModal(kUnsavedPrompt, (int)WindowFlags.AlwaysAutoResize))
      {
        return;
      }

      UIBindings.Text("The active scene has edits that are not saved.");
      UIBindings.Text("A package is made from the files on disk.");
      UIBindings.Spacing();
      if (UIBindings.Button("Save and compile"))
      {
        UIBindings.CloseCurrentPopup();
        if (EditorScene.Save())
        {
          StartCompile();
        }
        else
        {
          start_error = "The active scene could not be saved, so nothing was compiled. The log says why.";
        }
      }
      UIBindings.SameLine();
      /// declining the save is declining the compile
      if (UIBindings.Button("Cancel"))
      {
        UIBindings.CloseCurrentPopup();
      }
      UIBindings.EndPopup();
    }

    private void DrawResult()
    {
      CompileResult result = Compile.Result;
      if (result == CompileResult.None)
      {
        return;
      }

      UIBindings.SeparatorText("Result");
      if (result == CompileResult.Packaged)
      {
        UIBindings.TextWrapped(packaged_text);
        if (UIBindings.SmallButton("Copy path"))
        {
          UIBindings.ClipboardText = Compile.Package;
        }
        if (Compile.WasLeftOut.Count > 0)
        {
          UIBindings.TextWrapped(was_left_out_text);
          if (UIBindings.BeginChild("##left-out", 0f, kListHeight, kListFlags))
          {
            Lines(Compile.WasLeftOut);
          }
          UIBindings.EndChild();
        }
      }
      else if (result == CompileResult.Cancelled)
      {
        UIBindings.TextWrapped("The compile was cancelled. Nothing was packaged, and the package that was there is as it was.");
      }
      else
      {
        ErrorText("The project was not packaged.");
      }

      IReadOnlyList<string> errors = Compile.Errors;
      for (int i = 0; i < errors.Count; i++)
      {
        UIBindings.Bullet();
        ErrorText(errors[i]);
      }
      IReadOnlyList<string> warnings = Compile.Warnings;
      for (int i = 0; i < warnings.Count; i++)
      {
        UIBindings.Bullet();
        UIBindings.TextWrapped(warnings[i]);
      }
    }
  }
}
