using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Unicode;

namespace OtherCsBindings
{
  /// a string argument for one native call: UTF-8, null-terminated, in the caller's stack
  ///  buffer when it fits and in native memory when it does not. the native side reads it
  ///  during the call and keeps nothing
  public unsafe ref struct Utf8Arg
  {
    public const int StackSize = 256;

    private byte* heap;
    private readonly Span<byte> bytes;

    /// a null string crosses as an empty one: a native function never sees a null pointer
    public Utf8Arg(string? text, Span<byte> stack)
    {
      heap = null;
      text ??= string.Empty;

      if (Encoding.UTF8.GetMaxByteCount(text.Length) + 1 <= stack.Length)
      {
        bytes = stack;
      }
      else
      {
        int exact = Encoding.UTF8.GetByteCount(text) + 1;
        if (exact <= stack.Length)
        {
          bytes = stack;
        }
        else
        {
          heap = (byte*)NativeMemory.Alloc((nuint)exact);
          bytes = new Span<byte>(heap, exact);
        }
      }

      int written = Encoding.UTF8.GetBytes(text, bytes);
      bytes[written] = 0;
    }

    /// valid until Dispose
    public byte* Pointer => (byte*)Unsafe.AsPointer(ref MemoryMarshal.GetReference(bytes));

    public void Dispose()
    {
      if (heap != null)
      {
        NativeMemory.Free(heap);
        heap = null;
      }
    }

    /// a string native code handed back: read at once, since the next call may reuse its bytes
    public static string Read(byte* text)
    {
      return text == null ? string.Empty : Marshal.PtrToStringUTF8((IntPtr)text) ?? string.Empty;
    }

    /// text for native code to edit in place: as much of it as fits in buffer, null-terminated
    public static void Fill(string? text, Span<byte> buffer)
    {
      if (buffer.IsEmpty)
      {
        return;
      }
      Utf8.FromUtf16(text ?? string.Empty, buffer.Slice(0, buffer.Length - 1), out _, out int written);
      buffer[written] = 0;
    }

    /// what native code left in a buffer it edited, up to its terminator
    public static string Read(ReadOnlySpan<byte> buffer)
    {
      int end = buffer.IndexOf((byte)0);
      return Encoding.UTF8.GetString(end < 0 ? buffer : buffer.Slice(0, end));
    }
  }
}
