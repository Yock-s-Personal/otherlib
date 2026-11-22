using OtherCsBindings;
using System;

namespace Other.UI
{
  /// a texture the engine holds, to draw in a window. it is looked up, not kept: ask for it
  ///  again each frame, since a pipeline rebuilds its textures when it is resized
  public readonly struct UITexture
  {
    /// the engine's textures have their first row at the bottom: these corners show one the
    ///  right way up
    public static readonly Vec2 kTopLeft = new Vec2(0f, 1f);
    public static readonly Vec2 kBottomRight = new Vec2(1f, 0f);

    public readonly ulong Id;
    /// in pixels
    public readonly int Width;
    public readonly int Height;

    internal UITexture(ulong id, int width, int height)
    {
      Id = id;
      Width = width;
      Height = height;
    }

    /// false when the engine has no such texture, or has not finished loading it
    public bool IsValid => Id != 0;
  }

  public static class Textures
  {
    [NativeFunction("UIGetPipelineTexture")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, byte*, int*, int*, ulong> NativeGetPipelineTexture;
    [NativeFunction("UIGetAssetTexture")]
    internal static unsafe delegate* unmanaged[Cdecl]<ulong, int*, int*, ulong> NativeGetAssetTexture;
    [NativeFunction("UIGetAssetTextureByPath")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, int*, int*, ulong> NativeGetAssetTextureByPath;
    [NativeFunction("UIImage")]
    internal static unsafe delegate* unmanaged[Cdecl]<ulong, float, float, float, float, float, float, void> NativeImage;

    /// a texture a rendering pipeline draws into or reads, by the pipeline's name and the
    ///  name the pipeline gives it
    public static UITexture FromPipeline(string pipeline, string name)
    {
      unsafe
      {
        using var pipeline_utf8 = new Utf8Arg(pipeline, stackalloc byte[Utf8Arg.StackSize]);
        using var name_utf8 = new Utf8Arg(name, stackalloc byte[Utf8Arg.StackSize]);
        int width = 0;
        int height = 0;
        ulong id = NativeGetPipelineTexture(pipeline_utf8.Pointer, name_utf8.Pointer, &width, &height);
        return new UITexture(id, width, height);
      }
    }

    /// a texture asset that is loaded, by its id
    public static UITexture FromAsset(ulong asset_id)
    {
      unsafe
      {
        int width = 0;
        int height = 0;
        ulong id = NativeGetAssetTexture(asset_id, &width, &height);
        return new UITexture(id, width, height);
      }
    }

    /// a texture asset that is loaded, by the path it was loaded from. the path is read as
    ///  any asset path is: it may name a mount
    public static UITexture FromAsset(string path)
    {
      unsafe
      {
        using var path_utf8 = new Utf8Arg(path, stackalloc byte[Utf8Arg.StackSize]);
        int width = 0;
        int height = 0;
        ulong id = NativeGetAssetTextureByPath(path_utf8.Pointer, &width, &height);
        return new UITexture(id, width, height);
      }
    }

    /// an item of that size showing the texture the right way up. a texture that is not
    ///  valid leaves the item's space empty
    public static void Image(UITexture texture, Vec2 size)
    {
      Image(texture, size, UITexture.kTopLeft, UITexture.kBottomRight);
    }

    /// the same showing the part of the texture between the two corners
    public static void Image(UITexture texture, Vec2 size, Vec2 uv_min, Vec2 uv_max)
    {
      unsafe { NativeImage(texture.Id, size.X, size.Y, uv_min.X, uv_min.Y, uv_max.X, uv_max.Y); }
    }
  }
}
