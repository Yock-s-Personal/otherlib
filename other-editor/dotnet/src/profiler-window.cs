using System;
using Other.Core;
using Other.UI;

namespace Other.Editor
{
  /// how the editor's frames are going: how many a second and how long each, where the time
  ///  goes, how fast the scene and its scripts are told time passes, and what each of the
  ///  editor's own windows costs. the numbers are the engine's (FrameStats); this draws them
  [EditorWindow("profiler", Title = "Profiler", DefaultWidth = 420f, DefaultHeight = 520f)]
  public class ProfilerWindow : UIWindow
  {
    private const float kGraphHeight = 70f;
    /// a time rate further than this from 1 is said to be wrong
    private const float kTimeTolerance = 0.03f;

    private static readonly string[] kWindows =
    {
      "scene-hierarchy", "object-editor", "scene-graph-editor", "project-overview", "project-compiler", "project-creator", "profiler",
    };

    private readonly float[] history = new float[FrameStats.kHistory];
    /// the rows' text, made again when the engine makes its numbers again and not each frame
    private float shown_frames = -1f;
    private float shown_frame_ms = -1f;
    private string frames_text = "";
    private string frame_text = "";
    private string longest_text = "";
    private string update_text = "";
    private string render_text = "";
    private string wait_text = "";
    private string engine_text = "";
    private string script_text = "";
    private string steps_text = "";
    private string playback_text = "";
    private string graph_text = "";
    private readonly string[] window_text = new string[kWindows.Length];
    /// how many times each had been drawn when the text was last made: one that was not
    ///  drawn since is a tab behind another, and what it cost when it last was says nothing
    private readonly ulong[] window_draws = new ulong[kWindows.Length];
    private bool time_is_kept = true;

    protected override void OnRenderBody()
    {
      /// the engine's numbers change about twice a second: so does the text
      float frames = FrameStats.FramesPerSecond;
      float frame_ms = FrameStats.FrameMilliseconds;
      if (frames != shown_frames || frame_ms != shown_frame_ms)
      {
        shown_frames = frames;
        shown_frame_ms = frame_ms;
        Refresh();
      }

      UIWidgets.SeparatorText("Frames");
      Widgets.PropertyDisplay("Frames a second", frames_text);
      Widgets.PropertyDisplay("Each frame", frame_text);
      Widgets.PropertyDisplay("The longest", longest_text);
      DrawGraph();

      UIWidgets.SeparatorText("Where a frame goes");
      Widgets.PropertyDisplay("Update", update_text);
      Widgets.PropertyDisplay("Drawing", render_text);
      Widgets.PropertyDisplay("Held by the limit", wait_text);

      UIWidgets.SeparatorText("Time");
      Widgets.PropertyDisplay("Scene", playback_text);
      Widgets.PropertyDisplay("Told to the scene", engine_text);
      if (time_is_kept)
      {
        Widgets.PropertyDisplay("Told to scripts", script_text);
      }
      else
      {
        Widgets.PropertyDisplay("Told to scripts", script_text, Theme.Color("core.friendly_error_red"));
      }
      Widgets.PropertyDisplay("Fixed steps", steps_text);

      UIWidgets.SeparatorText("The editor's windows");
      for (int i = 0; i < kWindows.Length; i++)
      {
        if (Windows.IsOpen(kWindows[i]))
        {
          Widgets.PropertyDisplay(kWindows[i], window_text[i] ?? "");
        }
      }
    }

    private void Refresh()
    {
      frames_text = $"{FrameStats.FramesPerSecond:F1}";
      frame_text = $"{FrameStats.FrameMilliseconds:F2} ms";
      longest_text = $"{FrameStats.LongestFrameMilliseconds:F2} ms";
      update_text = $"{FrameStats.UpdateMilliseconds:F2} ms";
      render_text = $"{FrameStats.RenderMilliseconds:F2} ms";
      wait_text = $"{FrameStats.WaitMilliseconds:F2} ms";

      float engine = FrameStats.EngineSecondsPerSecond;
      float script = FrameStats.ScriptSecondsPerSecond;
      time_is_kept = MathF.Abs(script - engine) <= kTimeTolerance;
      engine_text = $"{engine:F3} s a second";
      script_text = time_is_kept ? $"{script:F3} s a second" : $"{script:F3} s a second: scripts run {script / MathF.Max(engine, 0.001f):F2} times too fast";
      steps_text = $"{FrameStats.FixedStepsPerSecond:F1} a second";
      playback_text = Playback.State.ToString();

      for (int i = 0; i < kWindows.Length; i++)
      {
        ulong draws = Windows.DrawCount(kWindows[i]);
        window_text[i] = draws != window_draws[i] ? $"{Windows.LastDrawMicroseconds(kWindows[i]):F0} us" : "not on screen";
        window_draws[i] = draws;
      }
    }

    /// the last frames' lengths as a line, with what the top and bottom of it are
    private void DrawGraph()
    {
      int count = FrameStats.History(history);
      Vec2 at = UIWidgets.CursorScreenPos;
      float width = UIWidgets.GetContentRegionAvail().X;
      Vec2 end = new Vec2(at.X + width, at.Y + kGraphHeight);
      DrawList.RectFilled(at, end, Theme.Color("core.field"), 2f);

      float top = 1f;
      for (int i = 0; i < count; i++)
      {
        top = MathF.Max(top, history[i]);
      }
      if (count > 1)
      {
        Vec4 line = Theme.Color("core.accent");
        float step = width / (FrameStats.kHistory - 1);
        float scale = (kGraphHeight - 6f) / top;
        Vec2 from = new Vec2(at.X, end.Y - 3f - history[0] * scale);
        for (int i = 1; i < count; i++)
        {
          Vec2 to = new Vec2(at.X + i * step, end.Y - 3f - history[i] * scale);
          DrawList.Line(from, to, line, 1.2f);
          from = to;
        }
      }
      UIWidgets.Dummy(width, kGraphHeight);

      /// said again only when the tallest frame of the graph is another number
      if (top != graph_top)
      {
        graph_top = top;
        graph_text = $"the last {FrameStats.kHistory} frames, up to {top:F1} ms";
      }
      UIWidgets.TextDisabled(graph_text);
    }

    private float graph_top = -1f;
  }
}
