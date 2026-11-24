using OtherCsBindings;

namespace Other.Editor
{
  /// making a project. it is what `oecli create` does, reached from inside the editor
  public static unsafe class Projects
  {
    [NativeFunction("EditorCreateProject")]
    internal static delegate* unmanaged[Cdecl]<byte*, byte*, byte*, byte*, byte*> NativeCreate;
    [NativeFunction("EditorCreatedProject")]
    internal static delegate* unmanaged[Cdecl]<byte*> NativeCreated;
    [NativeFunction("EditorCreatedProjectOpening")]
    internal static delegate* unmanaged[Cdecl]<NativeBool32> NativeCreatedOpening;

    /// makes a folder of that name inside folder, with a project file, a runtime script, a
    ///  starter scene and a C# script project in it. the editor opens it when it can
    ///  (OpeningLastCreated). name starts with a letter
    ///  and has only letters, digits, '-' and '_'. an empty author or description is
    ///  written as "N/A". false when nothing was made, and error says why
    public static bool Create(string name, string folder, string author, string description, out string error)
    {
      using var name_utf8 = new Utf8Arg(name, stackalloc byte[Utf8Arg.StackSize]);
      using var folder_utf8 = new Utf8Arg(folder, stackalloc byte[Utf8Arg.StackSize]);
      using var author_utf8 = new Utf8Arg(author, stackalloc byte[Utf8Arg.StackSize]);
      using var description_utf8 = new Utf8Arg(description, stackalloc byte[Utf8Arg.StackSize]);
      error = Utf8Arg.Read(NativeCreate(name_utf8.Pointer, folder_utf8.Pointer, author_utf8.Pointer, description_utf8.Pointer));
      return error.Length == 0;
    }

    /// the project file of the project that was last made here. empty when none was
    public static string LastCreated => Utf8Arg.Read(NativeCreated());
    /// the editor was told to open it. false when a project was open then (the engine does
    ///  not close one project and open another while it runs), and when it was made
    ///  anywhere but the folder the editor works from: a project's paths are written from
    ///  the folder that holds it, so it opens from there, as `oecli open` does
    public static bool OpeningLastCreated => NativeCreatedOpening();
  }
}
