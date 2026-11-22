using System;

namespace Other.UI
{
  /// a window drawn from C#. declare one with [EditorWindow] on a class that derives this:
  ///  the engine makes the one instance, puts it in the View menu and the layout, and calls
  ///  the body only while the window is open and on screen
  public abstract class UIWindow
  {
    /// once, after the window is made and before it is first drawn
    protected virtual void OnInitialize() { }
    /// once, before the window goes: its assembly is unloading or the UI is shutting down
    protected virtual void OnShutdown() { }
    protected virtual void OnRenderHeader() { }
    protected abstract void OnRenderBody();
    protected virtual void OnRenderFooter() { }

    /// what the window is opened and closed by. empty until the engine has made the window
    public string Name { get; internal set; } = "";

    public bool IsOpen => Windows.IsOpen(Name);
    public void Open() => Windows.Open(Name);
    public void Close() => Windows.Close(Name);

    /// the menu bar of a window declared with WindowFlags.MenuBar
    protected void DrawMenuBar(Action menu_contents)
    {
      if (UIBindings.BeginMenuBar())
      {
        menu_contents?.Invoke();
        UIBindings.EndMenuBar();
      }
    }

    /// the host's way in: the callbacks themselves are for the window's own class
    internal void Initialize() { OnInitialize(); }
    internal void Shutdown() { OnShutdown(); }
    internal void Draw()
    {
      OnRenderHeader();
      OnRenderBody();
      OnRenderFooter();
    }
  }
}
