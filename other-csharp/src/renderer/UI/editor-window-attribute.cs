using System;

namespace Other.UI
{
  /// declares a window. found when its assembly is loaded; there is no call to write.
  ///  the class derives UIWindow and has a constructor that takes nothing
  [AttributeUsage(AttributeTargets.Class, Inherited = false)]
  public sealed class EditorWindowAttribute : Attribute
  {
    /// what the window is opened and closed by: "project-overview"
    public string Name { get; }
    /// what its title bar says, and what its place in the layout is saved under. the name
    ///  when left empty
    public string Title { get; set; } = "";
    /// the size it has the first time it is ever shown; a size saved in the layout wins after
    public float DefaultWidth { get; set; }
    public float DefaultHeight { get; set; }
    public WindowFlags Flags { get; set; }

    public EditorWindowAttribute(string name)
    {
      Name = name;
    }
  }
}
