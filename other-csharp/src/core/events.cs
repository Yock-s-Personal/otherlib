using System;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.InteropServices;
using OtherCsBindings;

namespace Other.Core
{
  /// one handler listening to one engine event. disposing it stops the listening; so does
  ///  unloading the assembly the handler lives in
  public sealed class Subscription : IDisposable
  {
    internal ulong id;

    internal Subscription(ulong id)
    {
      this.id = id;
    }

    public bool Active => id != 0;

    public void Dispose()
    {
      Events.Unsubscribe(this);
    }
  }

  public static class Events
  {
    /// what an event's data is as it crosses. script_value_kind in
    ///  otherlib/src/scripting/dotnet_bindings/event_bindings.hpp is the same list in the same order
    internal enum Kind : int
    {
      None = 0,
      Bool = 1,
      Int32 = 2,
      UInt32 = 3,
      Int64 = 4,
      UInt64 = 5,
      Float = 6,
      Double = 7,
      Text = 8,
      Vec2 = 9,
      Vec3 = 10,
      Vec4 = 11,
    }

    [NativeFunction("EventRegister")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, void> NativeRegister;
    [NativeFunction("EventTrigger")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, void> NativeTrigger;
    [NativeFunction("EventTriggerWithString")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, byte*, void> NativeTriggerWithString;
    [NativeFunction("EventTriggerWithValue")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, int, void*, void> NativeTriggerWithValue;
    [NativeFunction("EventSubscribe")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, ulong> NativeSubscribe;
    [NativeFunction("EventUnsubscribe")]
    internal static unsafe delegate* unmanaged[Cdecl]<ulong, void> NativeUnsubscribe;

    private abstract class Listener
    {
      public string event_name = string.Empty;
      public Assembly? owner;
      /// a payload of a kind the handler did not ask for is reported once, not every time
      public bool refused;

      public abstract bool Deliver(Kind kind, IntPtr data, int count);
    }

    private sealed class PlainListener : Listener
    {
      public Action handler = () => { };

      public override bool Deliver(Kind kind, IntPtr data, int count)
      {
        handler();
        return true;
      }
    }

    private sealed class TypedListener<T> : Listener
    {
      public Action<T> handler = _ => { };

      public override bool Deliver(Kind kind, IntPtr data, int count)
      {
        if (!TryRead(kind, data, count, out T value))
        {
          return false;
        }
        handler(value);
        return true;
      }
    }

    private static readonly Dictionary<ulong, Listener> listeners = new();
    private static bool watching_unloads;

    public static void Register(string event_name)
    {
      unsafe
      {
        using var event_name_utf8 = new Utf8Arg(event_name, stackalloc byte[Utf8Arg.StackSize]);
        NativeRegister(event_name_utf8.Pointer);
      }
    }

    public static void Trigger(string event_name)
    {
      unsafe
      {
        using var event_name_utf8 = new Utf8Arg(event_name, stackalloc byte[Utf8Arg.StackSize]);
        NativeTrigger(event_name_utf8.Pointer);
      }
    }

    public static void Trigger(string event_name, string data)
    {
      unsafe
      {
        using var event_name_utf8 = new Utf8Arg(event_name, stackalloc byte[Utf8Arg.StackSize]);
        using var data_utf8 = new Utf8Arg(data, stackalloc byte[Utf8Arg.StackSize]);
        NativeTriggerWithString(event_name_utf8.Pointer, data_utf8.Pointer);
      }
    }

    /// data is one of: bool, int, uint, long, ulong, float, double, string, Vec2, Vec3, Vec4.
    ///  native listeners see exactly that kind
    public static void Trigger<T>(string event_name, T data)
    {
      unsafe
      {
        using var event_name_utf8 = new Utf8Arg(event_name, stackalloc byte[Utf8Arg.StackSize]);
        switch (data)
        {
          case string text: Trigger(event_name, text); break;
          case bool v: { uint raw = v ? 1u : 0u; NativeTriggerWithValue(event_name_utf8.Pointer, (int)Kind.Bool, &raw); } break;
          case int v: NativeTriggerWithValue(event_name_utf8.Pointer, (int)Kind.Int32, &v); break;
          case uint v: NativeTriggerWithValue(event_name_utf8.Pointer, (int)Kind.UInt32, &v); break;
          case long v: NativeTriggerWithValue(event_name_utf8.Pointer, (int)Kind.Int64, &v); break;
          case ulong v: NativeTriggerWithValue(event_name_utf8.Pointer, (int)Kind.UInt64, &v); break;
          case float v: NativeTriggerWithValue(event_name_utf8.Pointer, (int)Kind.Float, &v); break;
          case double v: NativeTriggerWithValue(event_name_utf8.Pointer, (int)Kind.Double, &v); break;
          case Vec2 v: NativeTriggerWithValue(event_name_utf8.Pointer, (int)Kind.Vec2, &v); break;
          case Vec3 v: NativeTriggerWithValue(event_name_utf8.Pointer, (int)Kind.Vec3, &v); break;
          case Vec4 v: NativeTriggerWithValue(event_name_utf8.Pointer, (int)Kind.Vec4, &v); break;
          default: throw new NotSupportedException($"An event cannot carry a {typeof(T).Name}: use bool, an integer, float, double, string or Vec2 to Vec4.");
        }
      }
    }

    /// handler is called on the main thread each time the event is raised, whatever it carries
    public static Subscription Subscribe(string event_name, Action handler)
    {
      return Add(event_name, new PlainListener { handler = handler, owner = handler.Method.DeclaringType?.Assembly });
    }

    /// handler is called on the main thread with the event's data. T is one of: bool, int,
    ///  uint, long, ulong, float, double, string, Vec2, Vec3, Vec4. an integer is given to any
    ///  integer type it fits in; data of another kind is an error and the handler is not called
    public static Subscription Subscribe<T>(string event_name, Action<T> handler)
    {
      return Add(event_name, new TypedListener<T> { handler = handler, owner = handler.Method.DeclaringType?.Assembly });
    }

    internal static void Unsubscribe(Subscription subscription)
    {
      if (subscription.id == 0)
      {
        return;
      }
      listeners.Remove(subscription.id);
      unsafe { NativeUnsubscribe(subscription.id); }
      subscription.id = 0;
    }

    private static Subscription Add(string event_name, Listener listener)
    {
      if (!watching_unloads)
      {
        AssemblyUnload.Unloading += DropSubscriptionsOf;
        watching_unloads = true;
      }

      ulong id;
      unsafe
      {
        using var event_name_utf8 = new Utf8Arg(event_name, stackalloc byte[Utf8Arg.StackSize]);
        id = NativeSubscribe(event_name_utf8.Pointer);
      }
      if (id == 0)
      {
        Logger.LogError($"Could not subscribe to the event '{event_name}': there is no event system to listen on.");
        return new Subscription(0);
      }

      listener.event_name = event_name;
      listeners[id] = listener;
      return new Subscription(id);
    }

    /// a handler is code of the assembly it was written in: when that assembly goes, so do
    ///  its subscriptions, or they would keep it in memory and keep calling its old code
    private static void DropSubscriptionsOf(Assembly assembly)
    {
      var gone = new List<ulong>();
      foreach (var entry in listeners)
      {
        if (entry.Value.owner == assembly)
        {
          gone.Add(entry.Key);
        }
      }
      foreach (ulong id in gone)
      {
        listeners.Remove(id);
        unsafe { NativeUnsubscribe(id); }
      }

      if (assembly == typeof(Events).Assembly)
      {
        AssemblyUnload.Unloading -= DropSubscriptionsOf;
        watching_unloads = false;
      }
    }

    /// called by the engine, by name, for each subscription an event reaches
    internal static int Dispatch(ulong subscription, int kind, IntPtr data, int count)
    {
      if (!listeners.TryGetValue(subscription, out Listener? listener))
      {
        return 0;
      }

      try
      {
        if (!listener.Deliver((Kind)kind, data, count) && !listener.refused)
        {
          listener.refused = true;
          Logger.LogError($"A handler subscribed to '{listener.event_name}' was not called: the event carries {(Kind)kind}, which its parameter cannot take.");
        }
      }
      catch (Exception e)
      {
        Logger.LogError($"A handler subscribed to '{listener.event_name}' threw: {e}");
      }
      return 1;
    }

    private static unsafe bool TryRead<T>(Kind kind, IntPtr data, int count, out T value)
    {
      value = default!;
      object? read = null;
      Type wanted = typeof(T);

      switch (kind)
      {
        case Kind.Bool:
          if (wanted == typeof(bool)) { read = *(uint*)data != 0; }
          break;

        case Kind.Int32:
        case Kind.Int64:
        case Kind.UInt32:
        case Kind.UInt64:
          {
            /// one integer, given to whichever integer type was asked for if it fits there
            bool negative = false;
            ulong magnitude;
            if (kind == Kind.Int32 || kind == Kind.Int64)
            {
              long signed = kind == Kind.Int32 ? *(int*)data : *(long*)data;
              negative = signed < 0;
              magnitude = negative ? (ulong)(-(signed + 1)) + 1 : (ulong)signed;
            }
            else
            {
              magnitude = kind == Kind.UInt32 ? *(uint*)data : *(ulong*)data;
            }

            if (wanted == typeof(ulong)) { if (!negative) { read = magnitude; } }
            else if (wanted == typeof(uint)) { if (!negative && magnitude <= uint.MaxValue) { read = (uint)magnitude; } }
            else if (wanted == typeof(long)) { if (!negative && magnitude <= long.MaxValue) { read = (long)magnitude; } else if (negative && magnitude <= (ulong)long.MaxValue + 1) { read = unchecked(-(long)(magnitude - 1) - 1); } }
            else if (wanted == typeof(int)) { if (!negative && magnitude <= int.MaxValue) { read = (int)magnitude; } else if (negative && magnitude <= (ulong)int.MaxValue + 1) { read = unchecked(-(int)(magnitude - 1) - 1); } }
          }
          break;

        case Kind.Float:
          if (wanted == typeof(float)) { read = *(float*)data; }
          else if (wanted == typeof(double)) { read = (double)*(float*)data; }
          break;

        case Kind.Double:
          if (wanted == typeof(double)) { read = *(double*)data; }
          else if (wanted == typeof(float)) { read = (float)*(double*)data; }
          break;

        case Kind.Text:
          if (wanted == typeof(string)) { read = data == IntPtr.Zero ? string.Empty : Marshal.PtrToStringUTF8(data, count); }
          break;

        case Kind.Vec2:
          if (wanted == typeof(Vec2)) { read = *(Vec2*)data; }
          break;

        case Kind.Vec3:
          if (wanted == typeof(Vec3)) { read = *(Vec3*)data; }
          break;

        case Kind.Vec4:
          if (wanted == typeof(Vec4)) { read = *(Vec4*)data; }
          break;

        default:
          break;
      }

      if (read == null)
      {
        return false;
      }
      value = (T)read;
      return true;
    }
  }
}
