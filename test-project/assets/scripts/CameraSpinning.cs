using Other;
using Other.Core;

public class CameraSpinning : SceneBehavior
{
  Vec3 center = new Vec3(0f, 1f, 0f);
  float radius = 4.5f;
  // how far along the circle each second
  float speed = 3f;

  protected override void OnUpdate()
  {
    var cam = GetComponent<CameraComponent>();

    // where the camera is on the circle: the level direction from the center out to it
    Vec3 outward = cam.Position - center;
    outward.Y = 0f;
    outward = outward.LengthSquared() < 1e-6f ? Vec3.UnitZ : outward.Normalized();

    // a step along the tangent leaves the circle a little, so the camera is put back on it
    Vec3 stepped = outward * radius + PathTangent(outward) * speed * Time.DeltaTime;
    cam.Position = center + stepped.Normalized() * radius;
    cam.LookAt(center);
  }

  // the tangent where the camera is: the direction out to it, turned a quarter turn round up
  Vec3 PathTangent(Vec3 outward)
  {
    return Vec3.Cross(Vec3.Up, outward);
  }
}
