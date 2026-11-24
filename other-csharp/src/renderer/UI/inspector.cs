using System;
using System.Numerics;
using System.Reflection;

namespace Other
{
  [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property)]
  public class InspectorFieldAttribute : Attribute
  {
    public string? DisplayName { get; set; }
    public string? Tooltip { get; set; }
    public float Min { get; set; } = float.MinValue;
    public float Max { get; set; } = float.MaxValue;
    public bool ReadOnly { get; set; } = false;

    public InspectorFieldAttribute(string? display_name = null)
    {
      DisplayName = display_name;
    }
  }

  [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property)]
  public class RangeAttribute : Attribute
  {
    public float Min { get; }
    public float Max { get; }

    public RangeAttribute(float min, float max)
    {
      Min = min;
      Max = max;
    }
  }

  [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property)]
  public class ColorFieldAttribute : Attribute
  {
  }

  [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property)]
  public class InspectorGroupAttribute : Attribute
  {
    public string GroupName { get; }

    public InspectorGroupAttribute(string group_name)
    {
      GroupName = group_name;
    }
  }

  [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property)]
  public class InspectorSeparatorAttribute : Attribute
  {
  }
}