using System;
using Other.UI;

namespace Other.Editor
{
  /// the three axes of the world in the top right corner of the Scene View, turned as the
  ///  view is turned, each with its letter at the end that points the way the axis counts
  ///  up. it says which way the view is looking, and it is taken hold of: dragged, the view
  ///  turns round what it looks at, as Alt and the left button turn it; a click on an end
  ///  looks along that axis from that end, and a second click on it from the other
  [SceneTool("view-axes", Title = "View Axes", Always = true, Order = -90)]
  public class ViewAxesTool : SceneTool
  {
    private static readonly Vec3[] kAxes = { new Vec3(1f, 0f, 0f), new Vec3(0f, 1f, 0f), new Vec3(0f, 0f, 1f) };
    private static readonly string[] kLetters = { "X", "Y", "Z" };
    private static readonly Vec4[] kColours =
    {
      new Vec4(0.92f, 0.26f, 0.24f, 1f),
      new Vec4(0.45f, 0.80f, 0.20f, 1f),
      new Vec4(0.25f, 0.52f, 0.96f, 1f),
    };
    private static readonly Vec4 kBack = new Vec4(1f, 1f, 1f, 0.10f);
    private static readonly Vec4 kLetter = new Vec4(0.08f, 0.08f, 0.09f, 1f);

    /// in the UI's units: how long an axis is, the end with a letter, the end without,
    ///  and how far the whole is kept from the view's edges
    private const float kLength = 34f;
    private const float kLettered = 9f;
    private const float kPlain = 5.5f;
    private const float kMargin = 14f;
    /// how far the pointer goes before a press is a drag and not a click
    private const float kClickSlack = 3f;
    /// degrees the view turns for each unit the pointer is dragged
    private const float kDegreesPerUnit = 0.5f;

    /// the six ends, as the frame has them: where each is on the screen, how near the
    ///  viewer it is, and the order they are drawn in, the farthest first
    private readonly Vec2[] places = new Vec2[6];
    private readonly float[] nearness = new float[6];
    private readonly int[] order = new int[6];

    private bool held;
    private bool dragged;
    private Vec2 pressed_at;
    private Vec2 last;
    private int pressed_on = -1;

    public override void OnSceneView()
    {
      Vec2 size = SceneViewport.Size;
      Vec3 forward = SceneViewport.CameraDirection;
      float reach = kLength + kLettered;
      if (size.X < reach * 4f || size.Y < reach * 4f || forward.LengthSquared() < 0.5f)
      {
        return;
      }
      Vec2 corner = SceneViewport.Position;
      Vec2 centre = new Vec2(corner.X + size.X - kMargin - reach, corner.Y + kMargin + reach);

      /// an item of its own over the picture: a press on it is not a click on the view,
      ///  nor one a handle or the camera takes
      UIWidgets.CursorScreenPos = new Vec2(centre.X - reach, centre.Y - reach);
      UIWidgets.InvisibleButton("##view-axes", reach * 2f, reach * 2f);
      bool hovered = UIWidgets.IsItemHovered();
      bool active = UIWidgets.IsItemActive();
      Vec2 pointer = UIWidgets.MousePos;

      /// each axis as the view sees it: how far to the right and up its end is, and how
      ///  far towards the viewer. the other end is the opposite of all three
      Vec3 right = Vec3.Cross(forward, Vec3.Up).Normalized();
      Vec3 up = Vec3.Cross(right, forward);
      for (int i = 0; i < 3; i++)
      {
        Vec2 across = new Vec2(Vec3.Dot(kAxes[i], right) * kLength, -Vec3.Dot(kAxes[i], up) * kLength);
        float towards = -Vec3.Dot(kAxes[i], forward);
        places[i] = new Vec2(centre.X + across.X, centre.Y + across.Y);
        nearness[i] = towards;
        places[i + 3] = new Vec2(centre.X - across.X, centre.Y - across.Y);
        nearness[i + 3] = -towards;
      }

      /// the end under the pointer: the nearest the viewer of those the pointer is on
      int under = -1;
      if (hovered || active)
      {
        for (int i = 0; i < 6; i++)
        {
          float ball = i < 3 ? kLettered : kPlain;
          float dx = pointer.X - places[i].X;
          float dy = pointer.Y - places[i].Y;
          if (dx * dx + dy * dy <= (ball + 1.5f) * (ball + 1.5f) && (under < 0 || nearness[i] > nearness[under]))
          {
            under = i;
          }
        }
      }

      if (active && !held)
      {
        held = true;
        dragged = false;
        pressed_at = pointer;
        last = pointer;
        pressed_on = under;
      }
      else if (active)
      {
        float gone_x = pointer.X - pressed_at.X;
        float gone_y = pointer.Y - pressed_at.Y;
        dragged = dragged || gone_x * gone_x + gone_y * gone_y > kClickSlack * kClickSlack;
        if (dragged && (pointer.X != last.X || pointer.Y != last.Y))
        {
          SceneViewport.Orbit((pointer.X - last.X) * kDegreesPerUnit, (pointer.Y - last.Y) * kDegreesPerUnit);
        }
        last = pointer;
      }
      else if (held)
      {
        held = false;
        if (!dragged && pressed_on >= 0)
        {
          /// from that end, towards the middle: the way the axis counts down for the end
          ///  with the letter. looking that way already, it is the other end that is meant
          Vec3 along = kAxes[pressed_on % 3] * (pressed_on < 3 ? -1f : 1f);
          if (Vec3.Dot(forward, along) > 0.999f)
          {
            along = along * -1f;
          }
          SceneViewport.LookAlong(along);
        }
        pressed_on = -1;
      }

      Draw(centre, reach, hovered || held, held && !dragged ? pressed_on : under);
    }

    private void Draw(Vec2 centre, float reach, bool lit, int marked)
    {
      if (lit)
      {
        DrawList.CircleFilled(centre, reach + 4f, kBack);
      }

      /// the farthest first, so that a nearer end is drawn over a farther one
      for (int i = 0; i < 6; i++)
      {
        order[i] = i;
      }
      for (int i = 1; i < 6; i++)
      {
        int moving = order[i];
        int j = i - 1;
        while (j >= 0 && nearness[order[j]] > nearness[moving])
        {
          order[j + 1] = order[j];
          j--;
        }
        order[j + 1] = moving;
      }

      for (int k = 0; k < 6; k++)
      {
        int end = order[k];
        int axis = end % 3;
        Vec4 colour = kColours[axis];
        /// an end that points away from the viewer is drawn dimmer, as farther things are
        float shade = 0.72f + 0.28f * MathF.Max(0f, MathF.Min(1f, (nearness[end] + 1f) * 0.5f));
        colour = new Vec4(colour.X * shade, colour.Y * shade, colour.Z * shade, 1f);
        if (end == marked)
        {
          colour = new Vec4(MathF.Min(1f, colour.X + 0.25f), MathF.Min(1f, colour.Y + 0.25f), MathF.Min(1f, colour.Z + 0.25f), 1f);
        }

        if (end < 3)
        {
          DrawList.Line(centre, places[end], colour, 2.5f);
          DrawList.CircleFilled(places[end], kLettered, colour);
          Vec2 letter = UIWidgets.CalcTextSize(kLetters[axis]);
          DrawList.Text(new Vec2(places[end].X - letter.X * 0.5f, places[end].Y - letter.Y * 0.5f), kLetter, kLetters[axis]);
        }
        else
        {
          DrawList.CircleFilled(places[end], kPlain, new Vec4(colour.X, colour.Y, colour.Z, 0.75f));
        }
      }
    }
  }
}
