using System;
using Other;
using Other.UI;

/// a window that counts what the engine asks of it
[EditorWindow("test-counter", Title = "Test Counter", DefaultWidth = 300, DefaultHeight = 200)]
public class CounterWindow : UIWindow
{
  public static int draws;
  public static int initialized;
  public static int shut_down;

  protected override void OnInitialize() { initialized++; }
  protected override void OnShutdown() { shut_down++; }

  protected override void OnRenderBody()
  {
    draws++;
    UIBindings.Text("drawn by the testing assembly");
  }
}

/// a window with a bug: it leaves an id pushed and throws, every time it is drawn
[EditorWindow("test-thrower")]
public class ThrowerWindow : UIWindow
{
  public static int draws;

  protected override void OnRenderBody()
  {
    draws++;
    UIBindings.PushID("left pushed");
    throw new InvalidOperationException("a window with a bug");
  }
}

/// a quieter bug: nothing is thrown, but an id is left pushed
[EditorWindow("test-unbalanced")]
public class UnbalancedWindow : UIWindow
{
  public static int draws;

  protected override void OnRenderBody()
  {
    draws++;
    UIBindings.PushID("left pushed");
  }
}

/// and the other way round: an id is popped that this window never pushed
[EditorWindow("test-overpop")]
public class OverpopWindow : UIWindow
{
  public static int draws;

  protected override void OnRenderBody()
  {
    draws++;
    UIBindings.PopID();
  }
}

/// declared a window but is not one: it is passed over with an error
[EditorWindow("test-not-a-window")]
public class NotAWindow
{
}

/// what the suite asks about the windows above
public static class WindowProbe
{
  public static int CounterDraws() { return CounterWindow.draws; }
  public static int CounterInitialized() { return CounterWindow.initialized; }
  public static int CounterShutDown() { return CounterWindow.shut_down; }
  public static int ThrowerDraws() { return ThrowerWindow.draws; }
  public static int UnbalancedDraws() { return UnbalancedWindow.draws; }
  public static int OverpopDraws() { return OverpopWindow.draws; }

  public static int IsOpen(int which) { return Windows.IsOpen(which == 0 ? "test-counter" : "test-thrower") ? 1 : 0; }
  public static int Open(int which) { return Windows.Open(which == 0 ? "test-counter" : "test-thrower") ? 1 : 0; }
  public static int Close(int which) { return Windows.Close(which == 0 ? "test-counter" : "test-thrower") ? 1 : 0; }
  public static int OpenMissing() { return Windows.Open("test-no-such-window") ? 1 : 0; }
}
