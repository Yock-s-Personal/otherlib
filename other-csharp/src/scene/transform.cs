using System;
using OtherCsBindings;

namespace Other
{
  [NativeComponent("transform")]
  public class Transform : Component
  {
    public Transform(ulong object_id)
      : base(object_id)
    {
    }

    [NativeFunction("TransformGetWorldPosition")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, float*, void> NativeGetWorldPosition;
    [NativeFunction("TransformSetWorldPosition")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, float, float, float, void> NativeSetWorldPosition;
    [NativeFunction("TransformGetWorldRotation")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, float*, void> NativeGetWorldRotation;
    [NativeFunction("TransformSetWorldRotation")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, float, float, float, float, void> NativeSetWorldRotation;
    [NativeFunction("TransformGetWorldScale")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, float*, void> NativeGetWorldScale;

    /// Position, Rotation and Scale are the object's own, under whatever it is under. the
    ///  three below are where that puts it in the world
    public Vec3 WorldPosition
    {
      get
      {
        unsafe
        {
          float* v = stackalloc float[3];
          NativeGetWorldPosition(ObjectId, v);
          return new Vec3(v[0], v[1], v[2]);
        }
      }
      set
      {
        unsafe { NativeSetWorldPosition(ObjectId, value.X, value.Y, value.Z); }
      }
    }

    public Quaternion WorldRotation
    {
      get
      {
        unsafe
        {
          float* q = stackalloc float[4];
          NativeGetWorldRotation(ObjectId, q);
          return new Quaternion(q[0], q[1], q[2], q[3]);
        }
      }
      set
      {
        unsafe { NativeSetWorldRotation(ObjectId, value.X, value.Y, value.Z, value.W); }
      }
    }

    /// read only: a scale set under a turned parent would not be a scale any more
    public Vec3 WorldScale
    {
      get
      {
        unsafe
        {
          float* v = stackalloc float[3];
          NativeGetWorldScale(ObjectId, v);
          return new Vec3(v[0], v[1], v[2]);
        }
      }
    }

    [NativeField("local_position")]
    public Vec3 Position { get => GetVec3(); set => SetVec3(value); }

    [NativeField("local_rotation_quat")]
    public Quaternion Rotation { get => GetQuat(); set => SetQuat(value); }

    [NativeField("local_scale")]
    public Vec3 Scale { get => GetVec3(); set => SetVec3(value); }

    public Vec3 EulerAngles
    {
      get => Rotation.ToEulerAngles();
      set
      {
        float deg2rad = MathF.PI / 180.0f;
        Rotation = Quaternion.CreateFromYawPitchRoll(value.Y * deg2rad, value.X * deg2rad, value.Z * deg2rad);
      }
    }

    public void Translate(Vec3 delta)
    {
      Position += delta;
    }
  }
}