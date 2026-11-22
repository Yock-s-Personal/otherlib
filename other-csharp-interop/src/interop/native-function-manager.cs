using System;
using System.Runtime.InteropServices;
using System.Reflection;
using System.Linq;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using System.Text;

#nullable enable
namespace OtherCsBindings
{
  [InteropBinding("NativeFunctionManager")]
  internal static class NativeFunctionManager
  {
    struct BindingPoint
    {
      public FieldInfo field;
      public string managed_name;
      public string registered_name;

      public bool autobind;

      /// the field's type as text, in the words the native side uses for its function. null
      ///  when the field is not a function pointer in the convention bindings use
      public string? signature;
      public string problem;

      public void SetValue(object? obj, IntPtr value)
      {
        if (autobind)
        {
          field.SetValue(obj, value);
        }
      }
    }

    /// field name -> BindingPoint
    private static Dictionary<string, BindingPoint> binding_points = new Dictionary<string, BindingPoint>();

    public static void DiscoverAndRegisterFunctions()
    {
      var assemblies = AssemblyLoader.GetFullLoadedAssemblyContext();
      foreach (var assembly in assemblies)
      {
        DiscoverAssemblyBindingPoints(assembly);
      }
    }

    public static void ClearBindingPoints()
    {
      binding_points.Clear();
    }

    private static void DiscoverAssemblyBindingPoints(Assembly assembly)
    {
      try
      {
        var types = assembly.GetTypes();
        foreach (var type in types)
        {
          DiscoverTypeBindingPoints(type);
        }
      }
      catch (Exception ex)
      {
        Host.HandleException(ex);
      }
    }

    private static void DiscoverTypeBindingPoints(Type type)
    {
      try
      {
        var count = 0;
        var binding_flags = BindingFlags.Static | BindingFlags.NonPublic | BindingFlags.Public;
        var fields = type.GetFields(binding_flags);
        foreach (var field in fields)
        {
          var attr = field.GetCustomAttribute<NativeFunctionAttribute>();
          if (attr == null)
          {
            continue;
          }

          if (binding_points.TryGetValue(attr.Name, out var existing))
          {
            /// discovery re-runs over already-known assemblies; seeing the same field
            ///  again is idempotent. a DIFFERENT field on the same name is a real
            ///  collision - only the first declaration binds, the second would stay a
            ///  null pointer forever, so make that case loud
            var same_field = existing.managed_name == field.Name &&
                             existing.field.DeclaringType?.FullName == type.FullName;
            if (!same_field)
            {
              Logger.LogError($"Native function '{attr.Name}' is declared again at {type.FullName}.{field.Name} " +
                              $"(bound at {existing.field.DeclaringType?.FullName}.{existing.managed_name}); the duplicate stays unbound");
            }
            continue;
          }

          if (!IsNativeFunctionType(field.FieldType))
          {
            Logger.LogError($"Field '{type.FullName}.{field.Name}' has NativeFunctionAttribute but is not a NativeFunction<T> or function pointer");
            continue;
          }

          string? signature = Signature(field, out string problem);
          binding_points.Add(attr.Name, new BindingPoint
          {
            field = field,
            managed_name = field.Name,
            registered_name = attr.Name,
            autobind = attr.AutoBind,
            signature = signature,
            problem = problem
          });
          count++;

          Logger.LogDebug($"Discovered native function binding point: {type.FullName}.{field.Name} for native function '{attr.Name}'");
        }
      }
      catch (Exception ex)
      {
        Host.HandleException(ex);
      }
    }

    /// does this work in all cases? this seems fragile
    private static bool IsNativeFunctionType(Type type)
    {
      if (type.IsFunctionPointer)
      {
        return true;
      }

      return false;
    }

    [UnmanagedCallersOnly]
    private static void RediscoverBindingPoints() => DiscoverAndRegisterFunctions();
    [UnmanagedCallersOnly]
    private static void CleanupBindingPoints() => ClearBindingPoints();

    /// what binding says back to the native side: binding_result in interop_interface.hpp
    private const Int32 kBound = 0;
    private const Int32 kNoField = 1;
    private const Int32 kNotUnmanagedCdecl = 2;
    private const Int32 kSignatureDiffers = 3;

    /// one kind of value that crosses, by size and class
    private static string Code(Type type)
    {
      if (type.IsPointer)
      {
        Type element = type.GetElementType()!;
        /// a pointer to anything but a scalar or another pointer is a handle on this side too
        if (element == typeof(void) || !(element.IsPrimitive || element.IsPointer || element == typeof(NativeBool32)))
        {
          return "ptr";
        }
        return $"ptr<{Code(element)}>";
      }
      if (type.IsEnum)
      {
        return Code(Enum.GetUnderlyingType(type));
      }
      if (type == typeof(void)) { return "void"; }
      if (type == typeof(bool)) { return "bool"; }
      if (type == typeof(byte)) { return "u8"; }
      if (type == typeof(sbyte)) { return "i8"; }
      if (type == typeof(short)) { return "i16"; }
      if (type == typeof(ushort)) { return "u16"; }
      if (type == typeof(int)) { return "i32"; }
      if (type == typeof(uint) || type == typeof(NativeBool32)) { return "u32"; }
      if (type == typeof(long)) { return "i64"; }
      if (type == typeof(ulong)) { return "u64"; }
      if (type == typeof(float)) { return "f32"; }
      if (type == typeof(double)) { return "f64"; }
      if (type == typeof(IntPtr) || type == typeof(UIntPtr)) { return "ptr"; }
      /// anything else may not cross by value; it matches no native signature
      return $"?{type.FullName}";
    }

    /// "(ptr<u8>,i32)->u32", as binding_signature.hpp writes a native function
    private static string? Signature(FieldInfo field, out string problem)
    {
      Type type = field.FieldType;
      problem = string.Empty;
      if (!type.IsFunctionPointer || !type.IsUnmanagedFunctionPointer)
      {
        problem = "is not declared 'delegate* unmanaged[Cdecl]<...>'";
        return null;
      }

      /// only the modified type carries the convention
      Type[] conventions = field.GetModifiedFieldType().GetFunctionPointerCallingConventions();
      if (conventions.Length != 1 || conventions[0] != typeof(CallConvCdecl))
      {
        problem = "is an unmanaged function pointer, but not Cdecl";
        return null;
      }

      var text = new StringBuilder("(");
      Type[] parameters = type.GetFunctionPointerParameterTypes();
      for (int i = 0; i < parameters.Length; i++)
      {
        text.Append(i == 0 ? "" : ",").Append(Code(parameters[i]));
      }
      text.Append(")->").Append(Code(type.GetFunctionPointerReturnType()));
      return text.ToString();
    }

    /// binds only a field whose own type agrees with the function it is given
    private static Int32 Bind(string name, FieldInfo field, string? signature, string problem, string native_signature, Action<IntPtr> set, IntPtr target, Action<string> report)
    {
      string where = $"{field.DeclaringType?.FullName}.{field.Name}";
      if (signature == null)
      {
        report($"Native function '{name}' was not bound: {where} {problem}");
        return kNotUnmanagedCdecl;
      }

      if (signature != native_signature)
      {
        report($"Native function '{name}' was not bound: the engine's is {native_signature} and {where} is declared {signature}");
        return kSignatureDiffers;
      }

      set(target);
      return kBound;
    }

    [UnmanagedCallersOnly]
    private static Int32 BindNativeFunction(NativeString name_str, IntPtr target, NativeString signature_str)
    {
      try
      {
        var name = name_str.ToString()!;
        if (!binding_points.TryGetValue(name, out var binding_point))
        {
          Logger.LogError($"No binding point found for native function '{name}'");
          return kNoField;
        }

        return Bind(name, binding_point.field, binding_point.signature, binding_point.problem, signature_str.ToString()!,
                    value => binding_point.SetValue(null, value), target, message => Logger.LogError(message));
      }
      catch (Exception e)
      {
        Host.HandleException(e);
        return kNoField;
      }
    }

    /// an unloaded assembly's fields are gone with it: when it is loaded again its fields
    ///  are found and bound afresh, and the old ones no longer keep it in memory
    internal static void ForgetAssembly(Assembly assembly)
    {
      var gone = binding_points.Where(point => point.Value.field.DeclaringType?.Assembly == assembly).Select(point => point.Key).ToList();
      foreach (var name in gone)
      {
        binding_points.Remove(name);
      }
    }

    /// calls to this happen before the discovery of assembly modules and binding points for native functions 
    /// so we do not use the binding points api here

    [UnmanagedCallersOnly]
    private static Int32 RegisterInternalCall(NativeString name_str, IntPtr target, NativeString signature_str)
    {
      try
      {
        var name = name_str.ToString()!;

        var name_start = name.IndexOf('+');
        var name_end = name.IndexOf(",", name_start, StringComparison.CurrentCulture);
        var field_name = name.Substring(name_start + 1, name_end - name_start - 1);
        var containing_type_name = name.Remove(name_start, name_end - name_start);

        var type = TypeInterface.FindType(containing_type_name);
        if (type == null)
        {
          return kNoField;
        }

        var binding_flags = BindingFlags.Static | BindingFlags.NonPublic;
        var field = type.GetFields(binding_flags).FirstOrDefault(field => field.Name == field_name);
        if (field == null)
        {
          return kNoField;
        }

        /// the logger is what this binds, so a refusal can only go to the console
        string? signature = Signature(field, out string problem);
        return Bind(name, field, signature, problem, signature_str.ToString()!, value => field.SetValue(null, value), target,
                    message => Console.Error.WriteLine(message));
      }
      catch (Exception ex)
      {
        Host.HandleException(ex);
        return kNoField;
      }
    }

    public static bool BindingPointsValid()
    {
      var all_bound = true;
      foreach (var kvp in binding_points)
      {
        var binding_point = kvp.Value;
        var value = binding_point.field.GetValue(null);
        if (value == null || (value is IntPtr ptr && ptr == IntPtr.Zero))
        {
          Logger.LogError($"Native function '{binding_point.registered_name}' was not bound to managed counterpart.");
          all_bound = false;
        }
      }

      if (all_bound)
      {
        Logger.LogDebug("All native functions successfully bound to managed counterparts.");
      }
      return all_bound;
    }

    [UnmanagedCallersOnly]
    private static NativeBool32 ValidateBindingPoints()
    {
      return BindingPointsValid();
    }
  }
}
#nullable disable