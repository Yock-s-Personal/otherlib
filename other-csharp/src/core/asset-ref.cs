using System;

namespace Other
{
  /// an asset a script holds, by its path. as a behaviour's field it is saved with the scene,
  ///  and a packaged game takes the asset it names
  public struct AssetRef : IEquatable<AssetRef>
  {
    /// as a scene file writes it: from the working directory or a mount. empty for none
    public string Path;

    public AssetRef(string path)
    {
      Path = path;
    }

    public bool IsEmpty => string.IsNullOrEmpty(Path);

    public bool Equals(AssetRef other) => (Path ?? "") == (other.Path ?? "");
    public override bool Equals(object obj) => obj is AssetRef other && Equals(other);
    public override int GetHashCode() => (Path ?? "").GetHashCode();
    public override string ToString() => Path ?? "";
  }
}
