using OtherCsBindings;
using System;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Other.Core
{
  /// what the engine can ask a behaviour to do. behavior_callback on the native side is the
  ///  same list in the same order
  internal enum BehaviorCallback : int
  {
    Awake = 0,
    Enable,
    Start,
    FixedUpdate,
    Update,
    LateUpdate,
    Render,
    CollisionEnter,
    CollisionExit,
    TriggerEnter,
    TriggerExit,
    JointBreak,
    Disable,
    Destroy,

    Count,
  }

  /// the C# end of the engine's behaviours. the engine keeps each behaviour's life and calls
  ///  its callbacks through the functions here, which it is handed once; C# adds, removes
  ///  and switches a behaviour by asking the engine, so there is one record of what an
  ///  object has
  internal static unsafe class BehaviorHost
  {
    [NativeFunction("BehaviorSetRuntime")]
    internal static delegate* unmanaged[Cdecl]<void*, void> NativeSetRuntime;
    [NativeFunction("BehaviorAdd")]
    internal static delegate* unmanaged[Cdecl]<long, byte*, void*> NativeAdd;
    [NativeFunction("BehaviorRemove")]
    internal static delegate* unmanaged[Cdecl]<long, byte*, NativeBool32> NativeRemove;
    [NativeFunction("BehaviorSetEnabled")]
    internal static delegate* unmanaged[Cdecl]<long, byte*, NativeBool32, NativeBool32> NativeSetEnabled;

    /// behavior_runtime on the native side, field for field
    [StructLayout(LayoutKind.Sequential)]
    private struct Runtime
    {
      public delegate* unmanaged[Cdecl]<IntPtr, long, void> bind_object;
      public delegate* unmanaged[Cdecl]<IntPtr, IntPtr, byte*, uint*, uint> attach;
      public delegate* unmanaged[Cdecl]<IntPtr, IntPtr, void> detach;
      public delegate* unmanaged[Cdecl]<IntPtr, int, void*, uint> call;
      public delegate* unmanaged[Cdecl]<IntPtr, uint, void> set_enabled;
    }

    /// what a contact callback is told: behavior_contact on the native side
    [StructLayout(LayoutKind.Sequential)]
    private struct Contact
    {
      public ulong other_object_id;
      public float px, py, pz;
      public float nx, ny, nz;
    }

    /// the two names a callback can be written under: OtherBehavior's own, and the one
    ///  SceneBehavior gives a user
    private static readonly string[][] callback_names =
    {
      new[] { "Awake", "OnAwake" },
      new[] { "Enable", "OnEnable" },
      new[] { "Start", "OnStart" },
      new[] { "FixedUpdate", "OnFixedUpdate" },
      new[] { "Update", "OnUpdate" },
      new[] { "LateUpdate", "OnLateUpdate" },
      new[] { "Render", "OnRender" },
      new[] { "CollisionEnter", "OnCollisionEnter" },
      new[] { "CollisionExit", "OnCollisionExit" },
      new[] { "TriggerEnter", "OnTriggerEnter" },
      new[] { "TriggerExit", "OnTriggerExit" },
      new[] { "JointBreak", "OnJointBreak" },
      new[] { "Disable", "OnDisable" },
      new[] { "Destroy", "OnDestroy" },
    };

    private static readonly Dictionary<Type, uint> written = new();
    private static bool watching_unloads;

    /// called by the engine, by name, once its functions are bound
    internal static void Attach()
    {
      if (!watching_unloads)
      {
        ScriptAssemblies.Unloading += AssemblyUnloading;
        watching_unloads = true;
      }

      Runtime runtime = new Runtime
      {
        bind_object = &BindObject,
        attach = &AttachBehavior,
        detach = &DetachBehavior,
        call = &Call,
        set_enabled = &SetEnabledFromEngine,
      };
      NativeSetRuntime(&runtime);
    }

    /// the behaviour of that type on the object, made by the engine if the object has none.
    ///  null when the object is not in a scene or the engine could not make it
    internal static OtherBehavior? Add(OtherObject target, Type type)
    {
      if (target.script_object_id < 0)
      {
        return null;
      }

      string name = type.FullName ?? type.Name;
      using var name_utf8 = new Utf8Arg(name, stackalloc byte[Utf8Arg.StackSize]);
      void* instance = NativeAdd(target.script_object_id, name_utf8.Pointer);
      return instance == null ? null : GCHandle.FromIntPtr((IntPtr)instance).Target as OtherBehavior;
    }

    internal static bool Remove(OtherObject target, string type_name)
    {
      using var name_utf8 = new Utf8Arg(type_name, stackalloc byte[Utf8Arg.StackSize]);
      return target.script_object_id >= 0 && NativeRemove(target.script_object_id, name_utf8.Pointer);
    }

    internal static bool SetEnabled(OtherObject target, string type_name, bool enabled)
    {
      using var name_utf8 = new Utf8Arg(type_name, stackalloc byte[Utf8Arg.StackSize]);
      return target.script_object_id >= 0 && NativeSetEnabled(target.script_object_id, name_utf8.Pointer, enabled);
    }

    /// one bit for each callback the type has written, found once for a type: a callback
    ///  left as the base class has it is never called
    private static uint Written(Type type)
    {
      if (written.TryGetValue(type, out uint mask))
      {
        return mask;
      }

      Assembly own = typeof(OtherBehavior).Assembly;
      for (int i = 0; i < callback_names.Length; i++)
      {
        foreach (string name in callback_names[i])
        {
          MethodInfo? method = type.GetMethod(name, BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic);
          if (method != null && method.DeclaringType != null && method.DeclaringType.Assembly != own)
          {
            mask |= 1u << i;
          }
        }
      }
      written[type] = mask;
      return mask;
    }

    private static void AssemblyUnloading(Assembly assembly)
    {
      var gone = new List<Type>();
      foreach (Type type in written.Keys)
      {
        if (type.Assembly == assembly)
        {
          gone.Add(type);
        }
      }
      foreach (Type type in gone)
      {
        written.Remove(type);
      }

      if (assembly == typeof(BehaviorHost).Assembly)
      {
        /// this code is going too: the engine must not call into it again
        NativeSetRuntime(null);
        ScriptAssemblies.Unloading -= AssemblyUnloading;
        watching_unloads = false;
      }
    }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    private static void BindObject(IntPtr target, long script_object_id)
    {
      if (GCHandle.FromIntPtr(target).Target is OtherObject bound)
      {
        bound.script_object_id = script_object_id;
      }
    }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    private static uint AttachBehavior(IntPtr target, IntPtr instance, byte* type_name, uint* out_callbacks)
    {
      try
      {
        if (GCHandle.FromIntPtr(target).Target is not OtherObject parent || GCHandle.FromIntPtr(instance).Target is not OtherBehavior behavior)
        {
          return 0;
        }

        *out_callbacks = Written(behavior.GetType());
        behavior.engine_name = Utf8Arg.Read(type_name);
        behavior.Bind(parent);
        parent.HostAdd(behavior);
        return 1;
      }
      catch (Exception e)
      {
        Logger.LogError($"A behaviour could not be attached: {e}");
        return 0;
      }
    }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    private static void DetachBehavior(IntPtr target, IntPtr instance)
    {
      if (GCHandle.FromIntPtr(target).Target is OtherObject parent && GCHandle.FromIntPtr(instance).Target is OtherBehavior behavior)
      {
        parent.HostRemove(behavior);
        behavior.Bind(null);
      }
    }

    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    private static void SetEnabledFromEngine(IntPtr instance, uint enabled)
    {
      if (GCHandle.FromIntPtr(instance).Target is OtherBehavior behavior)
      {
        behavior.enabled = enabled != 0;
      }
    }

    /// 0 when the callback threw: the engine switches the behaviour off, so it is said once
    [UnmanagedCallersOnly(CallConvs = new[] { typeof(CallConvCdecl) })]
    private static uint Call(IntPtr instance, int callback, void* data)
    {
      OtherBehavior? behavior = null;
      try
      {
        behavior = (OtherBehavior)GCHandle.FromIntPtr(instance).Target!;
        switch ((BehaviorCallback)callback)
        {
          case BehaviorCallback.Awake: behavior.CallAwake(); break;
          case BehaviorCallback.Enable: behavior.CallEnable(); break;
          case BehaviorCallback.Start: behavior.CallStart(); break;
          case BehaviorCallback.FixedUpdate: behavior.CallFixedUpdate(); break;
          case BehaviorCallback.Update: behavior.CallUpdate(); break;
          case BehaviorCallback.LateUpdate: behavior.CallLateUpdate(); break;
          case BehaviorCallback.Render: behavior.CallRender(); break;
          case BehaviorCallback.CollisionEnter: behavior.CallCollisionEnter(Info((Contact*)data)); break;
          case BehaviorCallback.CollisionExit: behavior.CallCollisionExit(Info((Contact*)data)); break;
          case BehaviorCallback.TriggerEnter: behavior.CallTriggerEnter(Info((Contact*)data)); break;
          case BehaviorCallback.TriggerExit: behavior.CallTriggerExit(Info((Contact*)data)); break;
          case BehaviorCallback.JointBreak: behavior.CallJointBreak(*(float*)data); break;
          case BehaviorCallback.Disable: behavior.CallDisable(); break;
          case BehaviorCallback.Destroy: behavior.CallDestroy(); break;
          default: return 0;
        }
        return 1;
      }
      catch (Exception e)
      {
        string name = callback >= 0 && callback < callback_names.Length ? callback_names[callback][1] : "a callback";
        Logger.LogError($"'{behavior?.GetType().FullName ?? "A behaviour"}' threw in {name}: {e}");
        return 0;
      }
    }

    private static CollisionInfo Info(Contact* contact)
    {
      return new CollisionInfo(contact->other_object_id, new Vec3(contact->px, contact->py, contact->pz), new Vec3(contact->nx, contact->ny, contact->nz));
    }
  }
}
