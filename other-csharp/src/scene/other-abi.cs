using System;
using OtherCsBindings;

namespace Other
{
  internal static class OtherABI
  {

    [NativeFunction("OeFnvHash")]
    internal static unsafe delegate* unmanaged[Cdecl]<byte*, UInt64> NativeFnvHash;

    [NativeFunction("OeValidateHandle")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, uint, uint, NativeBool32> NativeValidateHandle;
    [NativeFunction("OeComponentTypeCount")]
    internal static unsafe delegate* unmanaged[Cdecl]<int> NativeComponentTypeCount;
    [NativeFunction("OeComponentTypeAt")]
    internal static unsafe delegate* unmanaged[Cdecl]<int, UInt64*, NativeBool32*, byte*> NativeComponentTypeAt;
    [NativeFunction("OeComponentTypeColor")]
    internal static unsafe delegate* unmanaged[Cdecl]<int, float*, void> NativeComponentTypeColor;
    [NativeFunction("OeComponentTypeNativeName")]
    internal static unsafe delegate* unmanaged[Cdecl]<int, byte*> NativeComponentTypeNativeName;
    [NativeFunction("OeHasComponent")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, UInt64, NativeBool32> NativeHasComponent;
    [NativeFunction("OeAddComponent")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, UInt64, NativeBool32> NativeAddComponent;
    [NativeFunction("OeRemoveComponent")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, UInt64, NativeBool32> NativeRemoveComponent;

    [NativeFunction("OeGetFieldBool")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, UInt64, UInt64, NativeBool32*, NativeBool32> NativeGetFieldBool;
    [NativeFunction("OeSetFieldBool")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, UInt64, UInt64, NativeBool32, NativeBool32> NativeSetFieldBool;

    [NativeFunction("OeGetFieldI32")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, UInt64, UInt64, int*, NativeBool32> NativeGetFieldI32;
    [NativeFunction("OeSetFieldI32")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, UInt64, UInt64, int, NativeBool32> NativeSetFieldI32;

    [NativeFunction("OeGetFieldU32")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, UInt64, UInt64, uint*, NativeBool32> NativeGetFieldU32;
    [NativeFunction("OeSetFieldU32")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, UInt64, UInt64, uint, NativeBool32> NativeSetFieldU32;

    [NativeFunction("OeGetFieldI64")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, UInt64, UInt64, long*, NativeBool32> NativeGetFieldI64;
    [NativeFunction("OeSetFieldI64")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, UInt64, UInt64, long, NativeBool32> NativeSetFieldI64;

    [NativeFunction("OeGetFieldU64")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, UInt64, UInt64, UInt64*, NativeBool32> NativeGetFieldU64;
    [NativeFunction("OeSetFieldU64")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, UInt64, UInt64, UInt64, NativeBool32> NativeSetFieldU64;

    [NativeFunction("OeGetFieldF32")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, UInt64, UInt64, float*, NativeBool32> NativeGetFieldF32;
    [NativeFunction("OeSetFieldF32")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, UInt64, UInt64, float, NativeBool32> NativeSetFieldF32;

    [NativeFunction("OeGetFieldF64")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, UInt64, UInt64, double*, NativeBool32> NativeGetFieldF64;
    [NativeFunction("OeSetFieldF64")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, UInt64, UInt64, double, NativeBool32> NativeSetFieldF64;

    [NativeFunction("OeGetFieldVec2")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, UInt64, UInt64, float*, float*, NativeBool32> NativeGetFieldVec2;
    [NativeFunction("OeSetFieldVec2")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, UInt64, UInt64, float, float, NativeBool32> NativeSetFieldVec2;

    [NativeFunction("OeGetFieldVec3")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, UInt64, UInt64, float*, float*, float*, NativeBool32> NativeGetFieldVec3;
    [NativeFunction("OeSetFieldVec3")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, UInt64, UInt64, float, float, float, NativeBool32> NativeSetFieldVec3;

    [NativeFunction("OeGetFieldVec4")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, UInt64, UInt64, float*, float*, float*, float*, NativeBool32> NativeGetFieldVec4;
    [NativeFunction("OeSetFieldVec4")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, UInt64, UInt64, float, float, float, float, NativeBool32> NativeSetFieldVec4;

    [NativeFunction("OeGetFieldQuat")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, UInt64, UInt64, float*, float*, float*, float*, NativeBool32> NativeGetFieldQuat;
    [NativeFunction("OeSetFieldQuat")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, UInt64, UInt64, float, float, float, float, NativeBool32> NativeSetFieldQuat;

    [NativeFunction("OeGetFieldMat4")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, UInt64, UInt64, float*, NativeBool32> NativeGetFieldMat4;
    [NativeFunction("OeSetFieldMat4")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, UInt64, UInt64, float*, NativeBool32> NativeSetFieldMat4;

    [NativeFunction("OeGetFieldString")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, UInt64, UInt64, byte**, NativeBool32> NativeGetFieldString;
    [NativeFunction("OeSetFieldString")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, UInt64, UInt64, byte*, NativeBool32> NativeSetFieldString;
  }
}