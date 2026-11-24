using System;
using Other;
using Other.Core;
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
    UIWidgets.Text("drawn by the testing assembly");
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
    UIWidgets.PushID("left pushed");
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
    UIWidgets.PushID("left pushed");
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
    UIWidgets.PopID();
  }
}

/// makes each of the drawing calls a window has, and checks what they hand back
[EditorWindow("test-drawing")]
public class DrawingWindow : UIWindow
{
  public static int draws;
  /// one bit for each group of calls that did what it should, from the first draw and from
  ///  the latest
  public static int first_checks;
  public static int checks;

  private int count = 3;
  private Vec2 pair = new Vec2(1f, 2f);
  private Vec3 triple = new Vec3(1f, 2f, 3f);
  private Vec4 four = new Vec4(1f, 2f, 3f, 4f);

  protected override void OnRenderBody()
  {
    draws++;
    int ok = 0;

    /// an item's rectangle is where the cursor was, and the size it was given
    Vec2 origin = UIWidgets.CursorScreenPos;
    UIWidgets.BeginGroup();
    UIWidgets.Dummy(40f, 20f);
    Vec2 size = UIWidgets.ItemRectSize;
    Vec2 min = UIWidgets.ItemRectMin;
    Vec2 max = UIWidgets.ItemRectMax;
    UIWidgets.EndGroup();
    if (size.X == 40f && size.Y == 20f) { ok |= 1; }
    if (min.X == origin.X && min.Y == origin.Y && max.X == origin.X + 40f && max.Y == origin.Y + 20f) { ok |= 2; }

    UIWidgets.CursorPos = new Vec2(30f, 60f);
    Vec2 moved = UIWidgets.CursorPos;
    if (moved.X == 30f && moved.Y == 60f) { ok |= 4; }

    /// every shape, inside a clip rectangle that is closed again
    Vec2 at = UIWidgets.CursorScreenPos;
    Vec4 color = new Vec4(1f, 0.5f, 0f, 1f);
    DrawList.PushClipRect(at, new Vec2(at.X + 200f, at.Y + 200f));
    DrawList.Line(at, new Vec2(at.X + 50f, at.Y + 50f), color, 2f);
    DrawList.Rect(at, new Vec2(at.X + 50f, at.Y + 50f), color, 4f);
    DrawList.RectFilled(at, new Vec2(at.X + 20f, at.Y + 20f), color);
    DrawList.Circle(at, 10f, color);
    DrawList.CircleFilled(at, 5f, color, 12);
    DrawList.Triangle(at, new Vec2(at.X + 10f, at.Y), new Vec2(at.X, at.Y + 10f), color);
    DrawList.TriangleFilled(at, new Vec2(at.X + 10f, at.Y), new Vec2(at.X, at.Y + 10f), color);
    DrawList.BezierCubic(at, new Vec2(at.X + 30f, at.Y), new Vec2(at.X, at.Y + 30f), new Vec2(at.X + 30f, at.Y + 30f), color);
    DrawList.Text(at, color, "drawn text");
    DrawList.PopClipRect();
    if (DrawList.Pack(color) == 0xFF0080FF) { ok |= 8; }

    /// a texture nobody has is not valid, draws nothing and still takes its space
    UITexture none = Textures.FromPipeline("no-such-pipeline", "nothing");
    Textures.Image(none, new Vec2(16f, 12f));
    DrawList.Image(none, at, new Vec2(at.X + 16f, at.Y + 12f));
    Vec2 image_size = UIWidgets.ItemRectSize;
    if (!none.IsValid && none.Width == 0 && !Textures.FromAsset(12345ul).IsValid && !Textures.FromAsset("no/such/texture.png").IsValid &&
        image_size.X == 16f && image_size.Y == 12f)
    {
      ok |= 16;
    }

    /// numbers nobody is editing stay what they are
    UIWidgets.SetNextItemWidth(120f);
    bool edited = UIWidgets.DragInt("count", ref count) | UIWidgets.DragFloat2("pair", ref pair) | UIWidgets.DragFloat4("four", ref four) |
                  UIWidgets.SliderFloat2("pair slider", ref pair, 0f, 10f) | UIWidgets.SliderFloat3("triple slider", ref triple, 0f, 10f) |
                  UIWidgets.SliderFloat4("four slider", ref four, 0f, 10f);
    if (!edited && !UIWidgets.IsItemDeactivatedAfterEdit() && count == 3 && pair.Y == 2f && triple.Z == 3f && four.W == 4f) { ok |= 32; }

    /// nothing is held down in a test
    if (!UIWidgets.IsKeyDown(Key.A) && !UIWidgets.IsKeyPressed(Key.Enter) && UIWidgets.KeyMods == KeyMods.None &&
        !UIWidgets.IsMouseDown() && !UIWidgets.IsMouseClicked(MouseButton.Right) && !UIWidgets.IsMouseDoubleClicked() &&
        !UIWidgets.IsMouseReleased() && UIWidgets.GetMouseDragDelta().X == 0f && UIWidgets.MouseWheel.Y == 0f)
    {
      ok |= 64;
    }

    UIWidgets.AlignTextToFramePadding();
    UIWidgets.SetNextItemOpen(true);
    if (UIWidgets.TreeNode("opened for it"))
    {
      ok |= 128;
      UIWidgets.TreePop();
    }

    if (UIWidgets.IsWindowAppearing() == (draws == 1)) { ok |= 256; }
    if (UIWidgets.ScrollY == 0f && UIWidgets.ScrollX == 0f && UIWidgets.ScrollMaxY >= 0f && UIWidgets.ScrollMaxX >= 0f) { ok |= 512; }

    /// the mouse is nowhere, so it is over nothing
    Vec2 mouse = UIWidgets.MousePos;
    if (!UIWidgets.IsMouseHoveringRect(at, new Vec2(at.X + 50f, at.Y + 50f)) && mouse.X < at.X) { ok |= 1024; }

    if (draws == 1) { first_checks = ok; }
    checks = ok;
  }
}

/// a clip rectangle pushed and never popped
[EditorWindow("test-clip-left-open")]
public class ClipLeftOpenWindow : UIWindow
{
  public static int draws;

  protected override void OnRenderBody()
  {
    draws++;
    Vec2 at = UIWidgets.CursorScreenPos;
    DrawList.PushClipRect(at, new Vec2(at.X + 10f, at.Y + 10f));
  }
}

/// and one popped that this window never pushed
[EditorWindow("test-clip-overpop")]
public class ClipOverpopWindow : UIWindow
{
  public static int draws;

  protected override void OnRenderBody()
  {
    draws++;
    DrawList.PopClipRect();
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
  public static int DrawingDraws() { return DrawingWindow.draws; }
  public static int DrawingFirstChecks() { return DrawingWindow.first_checks; }
  public static int DrawingChecks() { return DrawingWindow.checks; }
  public static int ClipLeftOpenDraws() { return ClipLeftOpenWindow.draws; }
  public static int ClipOverpopDraws() { return ClipOverpopWindow.draws; }

  public static int IsOpen(int which) { return Windows.IsOpen(which == 0 ? "test-counter" : "test-thrower") ? 1 : 0; }
  public static int Open(int which) { return Windows.Open(which == 0 ? "test-counter" : "test-thrower") ? 1 : 0; }
  public static int Close(int which) { return Windows.Close(which == 0 ? "test-counter" : "test-thrower") ? 1 : 0; }
  public static int OpenMissing() { return Windows.Open("test-no-such-window") ? 1 : 0; }
}
