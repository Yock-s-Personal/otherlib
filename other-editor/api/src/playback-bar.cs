using Other.UI;

namespace Other.Editor
{
  /// play, pause and stop in the middle of the main menu bar. each is its command with a
  ///  picture for a label: the one that is the scene's state now is lit, and one that
  ///  would do nothing is greyed
  internal static class PlaybackBar
  {
    private const float kGap = 2f;

    /// set once there is a menu bar to draw in
    internal static bool Shown;

    private static Command? play;
    private static Command? pause;
    private static Command? stop;

    internal static void Draw()
    {
      if (!Shown || !UIWidgets.BeginMainMenuBar())
      {
        return;
      }

      play ??= Commands.Find("playback.play");
      pause ??= Commands.Find("playback.pause");
      stop ??= Commands.Find("playback.stop");

      /// the bar puts its items one text line down from its top: as much is left under them
      Vec2 at = UIWidgets.CursorPos;
      Vec2 bar = UIWidgets.GetWindowSize();
      float side = bar.Y - at.Y * 2f;
      if (side > 4f && play != null && pause != null && stop != null)
      {
        PlaybackState state = Playback.State;
        float width = side * 1.5f;
        UIWidgets.CursorPos = new Vec2((bar.X - width * 3f - kGap * 2f) * 0.5f, at.Y);
        Button(0, play, width, side, state == PlaybackState.Playing, "core.success");
        UIWidgets.SameLine(0f, kGap);
        Button(1, pause, width, side, state == PlaybackState.Paused, "core.warning");
        UIWidgets.SameLine(0f, kGap);
        Button(2, stop, width, side, false, "core.text");
      }
      UIWidgets.EndMainMenuBar();
    }

    private static void Button(int which, Command command, float width, float height, bool lit, string lit_color)
    {
      bool enabled = command.Enabled;
      UIWidgets.PushID(which);
      UIWidgets.BeginDisabled(!enabled && !lit);
      bool clicked = UIWidgets.InvisibleButton("##playback", width, height);
      bool hovered = UIWidgets.IsItemHovered();
      UIWidgets.EndDisabled();
      UIWidgets.PopID();

      Vec2 min = UIWidgets.ItemRectMin;
      Vec2 max = UIWidgets.ItemRectMax;
      if (hovered && enabled)
      {
        DrawList.RectFilled(min, max, Theme.Color("core.button_hover"), 3f);
        UIWidgets.SetTooltip(command.Label);
      }

      Vec4 color = Theme.Color(lit ? lit_color : enabled ? "core.text" : "core.text_disabled");
      Vec2 middle = new Vec2((min.X + max.X) * 0.5f, (min.Y + max.Y) * 0.5f);
      float half = height * 0.28f;
      switch (which)
      {
        case 0:
          DrawList.TriangleFilled(new Vec2(middle.X - half * 0.8f, middle.Y - half), new Vec2(middle.X - half * 0.8f, middle.Y + half), new Vec2(middle.X + half, middle.Y), color);
          break;

        case 1:
          DrawList.RectFilled(new Vec2(middle.X - half, middle.Y - half), new Vec2(middle.X - half * 0.3f, middle.Y + half), color);
          DrawList.RectFilled(new Vec2(middle.X + half * 0.3f, middle.Y - half), new Vec2(middle.X + half, middle.Y + half), color);
          break;

        default:
          DrawList.RectFilled(new Vec2(middle.X - half, middle.Y - half), new Vec2(middle.X + half, middle.Y + half), color);
          break;
      }

      if (clicked && enabled)
      {
        command.Run();
      }
    }
  }
}
