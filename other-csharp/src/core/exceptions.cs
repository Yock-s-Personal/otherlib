using System;

namespace Other
{
  public class MissingComponentException : Exception
  {
    public MissingComponentException(string message) : base(message)
    {
    }
  }

  /// something was asked of a scene object that has been destroyed
  public class DestroyedObjectException : Exception
  {
    public DestroyedObjectException(string message) : base(message)
    {
    }
  }
}