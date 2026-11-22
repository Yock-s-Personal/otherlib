using OtherCsBindings;

namespace Other
{
  public static class Fnv
  {
    public static ulong Hash(string str)
    {
      unsafe
      {
        using var str_utf8 = new Utf8Arg(str, stackalloc byte[Utf8Arg.StackSize]);
        return OtherABI.NativeFnvHash(str_utf8.Pointer);
      }
    }
  }
}
