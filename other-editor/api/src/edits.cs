using System;
using OtherCsBindings;

namespace Other.Editor
{
  /// one action that is one undo step. made by Edits.Begin and ended by being disposed
  public struct EditScope : IDisposable
  {
    private bool open;

    internal EditScope(bool open)
    {
      this.open = open;
    }

    public void Dispose()
    {
      if (open)
      {
        open = false;
        Edits.End();
      }
    }

    /// ends the action with nothing recorded, for one that was put back by hand: a drag
    ///  that Escape cancelled. disposing it afterwards does nothing more
    public void Drop()
    {
      if (open)
      {
        open = false;
        Edits.Drop();
      }
    }
  }

  /// changes to the scene that can be undone. a window says what it changed and the editor
  ///  keeps the steps: nothing a window does to the scene is undoable unless it says so here.
  ///  nothing is recorded while the scene plays, since stop puts everything back
  public static unsafe class Edits
  {
    [NativeFunction("EditorEditsChanged")]
    internal static delegate* unmanaged[Cdecl]<void> NativeChanged;
    [NativeFunction("EditorEditsBegin")]
    internal static delegate* unmanaged[Cdecl]<byte*, void> NativeBegin;
    [NativeFunction("EditorEditsEnd")]
    internal static delegate* unmanaged[Cdecl]<void> NativeEnd;
    [NativeFunction("EditorEditsDrop")]
    internal static delegate* unmanaged[Cdecl]<void> NativeDrop;
    [NativeFunction("EditorEditsCanUndo")]
    internal static delegate* unmanaged[Cdecl]<NativeBool32> NativeCanUndo;
    [NativeFunction("EditorEditsCanRedo")]
    internal static delegate* unmanaged[Cdecl]<NativeBool32> NativeCanRedo;
    [NativeFunction("EditorEditsUndo")]
    internal static delegate* unmanaged[Cdecl]<void> NativeUndo;
    [NativeFunction("EditorEditsRedo")]
    internal static delegate* unmanaged[Cdecl]<void> NativeRedo;
    [NativeFunction("EditorEditsUndoName")]
    internal static delegate* unmanaged[Cdecl]<byte*> NativeUndoName;
    [NativeFunction("EditorEditsHasUnsaved")]
    internal static delegate* unmanaged[Cdecl]<NativeBool32> NativeHasUnsaved;

    /// a value is being changed, perhaps over many frames (a drag, typing): called each
    ///  frame it changes. it becomes one undo step once the changes stop
    public static void Changed()
    {
      NativeChanged();
    }

    /// one action, one undo step, recorded when the scope ends:
    ///  using (Edits.Begin("Rename")) { item.Name = text; }
    ///  scopes nest, and the outermost is the step
    public static EditScope Begin(string name)
    {
      using var name_utf8 = new Utf8Arg(name, stackalloc byte[Utf8Arg.StackSize]);
      NativeBegin(name_utf8.Pointer);
      return new EditScope(true);
    }

    internal static void End()
    {
      NativeEnd();
    }

    internal static void Drop()
    {
      NativeDrop();
    }

    public static bool CanUndo => NativeCanUndo();
    public static bool CanRedo => NativeCanRedo();

    public static void Undo()
    {
      NativeUndo();
    }

    public static void Redo()
    {
      NativeRedo();
    }

    /// what the next undo would undo, as Begin was given it. empty for a value that was
    ///  dragged, and when there is nothing to undo
    public static string UndoName => Utf8Arg.Read(NativeUndoName());

    /// the scene holds a change that is not in its file
    public static bool HasUnsavedChanges => NativeHasUnsaved();
  }
}
