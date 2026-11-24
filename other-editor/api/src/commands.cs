using System;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;
using Other.UI;
using OtherCsBindings;

namespace Other.Editor
{
  /// a key with the modifiers held with it: Ctrl+S
  public readonly struct KeyChord : IEquatable<KeyChord>
  {
    public Key Key { get; }
    public KeyMods Mods { get; }

    public KeyChord(Key key, KeyMods mods = KeyMods.None)
    {
      Key = key;
      Mods = mods;
    }

    public bool IsNone => Key == Key.None;

    public bool Equals(KeyChord other) => Key == other.Key && Mods == other.Mods;
    public override bool Equals(object? other) => other is KeyChord chord && Equals(chord);
    public override int GetHashCode() => HashCode.Combine((int)Key, (int)Mods);

    /// as a menu shows it: "Ctrl+Shift+S". empty for none
    public override string ToString()
    {
      if (IsNone)
      {
        return "";
      }

      var text = new StringBuilder();
      if ((Mods & KeyMods.Ctrl) != 0) { text.Append("Ctrl+"); }
      if ((Mods & KeyMods.Shift) != 0) { text.Append("Shift+"); }
      if ((Mods & KeyMods.Alt) != 0) { text.Append("Alt+"); }
      if ((Mods & KeyMods.Super) != 0) { text.Append("Super+"); }
      text.Append(Key);
      return text.ToString();
    }
  }

  /// something the editor can be told to do, by its name, its keys or its menu item. it is
  ///  there until it is disposed, or the assembly its code was written in is unloaded
  public sealed class Command : IDisposable
  {
    /// what it is run by: "edit.undo"
    public string Name { get; }
    /// what a menu calls it: "Undo"
    public string Label { get; }
    /// none when it was given none, or its keys were another command's already
    public KeyChord Chord { get; internal set; }

    internal readonly Action run;
    internal readonly Func<bool>? enabled;
    internal Assembly? owner;
    internal MenuItem? item;

    internal Command(string name, string label, Action run, KeyChord chord, Func<bool>? enabled)
    {
      Name = name;
      Label = label;
      Chord = chord;
      this.run = run;
      this.enabled = enabled;
      owner = run.Method.DeclaringType?.Assembly;
    }

    /// whether it would do anything now. one that says nothing about it always would
    public bool Enabled
    {
      get
      {
        if (enabled == null)
        {
          return true;
        }

        try
        {
          return enabled();
        }
        catch (Exception e)
        {
          Core.Logger.LogError($"The command '{Name}' threw when asked whether it is enabled: {e}");
          return false;
        }
      }
    }

    /// false when it is not enabled, or it threw
    public bool Run()
    {
      if (!Enabled)
      {
        return false;
      }

      try
      {
        run();
        return true;
      }
      catch (Exception e)
      {
        Core.Logger.LogError($"The command '{Name}' threw: {e}");
        return false;
      }
    }

    internal void Clicked()
    {
      Run();
    }

    public void Dispose()
    {
      Commands.Remove(this);
    }
  }

  /// the editor's commands. undo, save and the rest are commands like any a window adds, so
  ///  one list knows every key the editor answers to
  public static unsafe class Commands
  {
    [NativeFunction("EditorSetFrameFunction")]
    internal static delegate* unmanaged[Cdecl]<void*, void> NativeSetFrameFunction;

    private static readonly List<Command> commands = new();
    private static bool hosted;

    public static IReadOnlyList<Command> All => commands;

    /// name is what the command is run by and is the one of its name: "edit.undo". chord
    ///  is checked every frame wherever the focus is, unless a text field being typed in
    ///  wants those keys. menu is where its item goes in the main menu bar, "Edit" or
    ///  "File/Project", once there is a menu bar. enabled is asked each frame, so it has
    ///  to be cheap. null when the name is taken
    public static Command? Add(string name, string label, Action run, KeyChord chord = default, string? menu = null, Func<bool>? enabled = null)
    {
      if (Find(name) != null)
      {
        Core.Logger.LogError($"The command '{name}' was not added: there is one of that name already.");
        return null;
      }

      if (!chord.IsNone)
      {
        foreach (Command other in commands)
        {
          if (other.Chord.Equals(chord))
          {
            Core.Logger.LogWarning($"The command '{name}' was added without its keys: {chord} runs '{other.Name}' already.");
            chord = default;
            break;
          }
        }
      }

      Host();
      var command = new Command(name, label, run, chord, enabled);
      commands.Add(command);
      if (menu != null)
      {
        command.item = Menus.Add(menu, label, command.Clicked);
        if (!chord.IsNone)
        {
          command.item.SetShortcutText(chord.ToString());
        }
      }
      return command;
    }

    public static Command? Find(string name)
    {
      foreach (Command command in commands)
      {
        if (command.Name == name)
        {
          return command;
        }
      }
      return null;
    }

    /// false when there is no command of that name, it is not enabled, or it threw
    public static bool Run(string name)
    {
      Command? command = Find(name);
      return command != null && command.Run();
    }

    internal static void Remove(Command command)
    {
      if (commands.Remove(command))
      {
        command.item?.Dispose();
        command.item = null;
      }
    }

    internal static void Host()
    {
      if (hosted)
      {
        return;
      }
      hosted = true;
      ScriptAssemblies.Unloading += DropCommandsOf;
      NativeSetFrameFunction((delegate* unmanaged[Cdecl]<void>)&Frame);
    }

    /// a command is code of the assembly it was written in: when that assembly goes, so do
    ///  its commands
    private static void DropCommandsOf(Assembly assembly)
    {
      bool all = assembly == typeof(Commands).Assembly;
      for (int i = commands.Count - 1; i >= 0; i--)
      {
        if (all || commands[i].owner == assembly)
        {
          Remove(commands[i]);
        }
      }

      if (all)
      {
        /// this code is going too: the editor must not call into it again
        NativeSetFrameFunction(null);
        ScriptAssemblies.Unloading -= DropCommandsOf;
        hosted = false;
      }
    }

    /// what the editor calls once a frame while the UI is drawn: the keys are checked, each
    ///  menu item is greyed or not, and the play buttons are drawn
    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    private static void Frame()
    {
      for (int i = 0; i < commands.Count; i++)
      {
        Command command = commands[i];
        if (command.item != null && command.enabled != null)
        {
          command.item.Enabled = command.Enabled;
        }
        if (!command.Chord.IsNone && UIWidgets.Shortcut(command.Chord.Key, command.Chord.Mods, true))
        {
          command.Run();
        }
      }
      PlaybackBar.Draw();
      Dialogs.Frame();
    }
  }
}
