using System;
using System.Runtime.InteropServices;
using System.Collections.Generic;
using OtherCsBindings;

namespace Other
{
#nullable enable
  [NativeComponent("render_component")]
  public class RenderComponent : Component
  {
    [NativeFunction("GetNumVertices")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, int> NativeGetNumVertices;
    [NativeFunction("GetNumIndices")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, int> NativeGetNumIndices;
    [NativeFunction("GetMeshName")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, byte*> NativeGetMeshName;
    [NativeFunction("FetchMesh")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, float*, int*, int*, int*, void> NativeFetchMesh;
    [NativeFunction("UploadMesh")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, byte*, float*, int*, int*, int*, void> NativeUploadMesh;
    [NativeFunction("GetMaterialPath")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, byte*> NativeGetMaterialPath;
    [NativeFunction("SetMaterialPath")]
    internal static unsafe delegate* unmanaged[Cdecl]<UInt64, byte*, void> NativeSetMaterialPath;

    private Mesh? model = null;

    public RenderComponent(ulong object_id)
      : base(object_id)
    {
      model = null;
    }

    public Mesh? Model {
      get
      {
        if (model == null)
        {
          unsafe
          {
            model = null;
            string mesh_name = Utf8Arg.Read(NativeGetMeshName(ObjectId));
            model = new Mesh(mesh_name.Length > 0 ? mesh_name : "default_mesh");

            /// a vertex is Vertex.Stride values of four bytes and an index entry is one
            ///  triangle, three indices: the model's own layout, copied as it is
            int num_vertices = NativeGetNumVertices(ObjectId);
            int num_triangles = NativeGetNumIndices(ObjectId);
            float[] vertices = new float[num_vertices * Vertex.Stride];
            int[] indices = new int[num_triangles * 3];
            fixed (float* vertex_ptr = vertices)
            fixed (int* index_ptr = indices)
            {
              /// in: how much there is room for. out: how much was written
              NativeFetchMesh(ObjectId, vertex_ptr, &num_vertices, index_ptr, &num_triangles);
            }

            for (int v = 0; v < num_vertices; v++)
            {
              int i = v * Vertex.Stride;
              model.AddVertex(
                new Vec3(vertices[i], vertices[i + 1], vertices[i + 2]),
                new Vec3(vertices[i + 3], vertices[i + 4], vertices[i + 5]),
                new Vec3(vertices[i + 6], vertices[i + 7], vertices[i + 8]),
                new Vec3(vertices[i + 9], vertices[i + 10], vertices[i + 11]),
                new Vec2(vertices[i + 12], vertices[i + 13]),
                /// the bone ids are integers in the model; they are not floats to convert
                new IntVec4(BitConverter.SingleToInt32Bits(vertices[i + 14]), BitConverter.SingleToInt32Bits(vertices[i + 15]),
                            BitConverter.SingleToInt32Bits(vertices[i + 16]), BitConverter.SingleToInt32Bits(vertices[i + 17])),
                new Vec4(vertices[i + 18], vertices[i + 19], vertices[i + 20], vertices[i + 21])
              );
            }
            for (int t = 0; t < num_triangles; t++)
            {
              model.AddIndices(indices[t * 3], indices[t * 3 + 1], indices[t * 3 + 2]);
            }
          }
        }
        return model;
      }
      set
      {
        model = value;
        if (model == null)
        {
          return;
        }

        List<RawVertex> raw_vertices = new List<RawVertex>();
        List<float> vertex_data = new List<float>();
        model.Vertices.ForEach(vertex => raw_vertices.Add(vertex.ToRawVertex()));
        raw_vertices.ForEach(raw_vertex => raw_vertex.AddToList(vertex_data));

        List<RawIndex> raw_indices = new List<RawIndex>();
        List<int> index_data = new List<int>();
        model.Indices.ForEach(index => raw_indices.Add(index.ToRawIndex()));
        raw_indices.ForEach(raw_index => raw_index.AddToList(index_data));

        Span<float> vertices = CollectionsMarshal.AsSpan(vertex_data);
        Span<int> indices = CollectionsMarshal.AsSpan(index_data);
        unsafe
        {
          fixed (float* vertex_ptr = vertices)
          fixed (int* index_ptr = indices)
          {
            int vertex_count = model.VertexCount;
            int index_count = model.IndexCount;
            using var name_utf8 = new Utf8Arg(model.Name, stackalloc byte[Utf8Arg.StackSize]);
            NativeUploadMesh(ObjectId, name_utf8.Pointer, vertex_ptr, &vertex_count, index_ptr, &index_count);
          }
        }
      }
    }

    /// materials are assets: the property is the .omat path ("" = the model's own imported
    /// materials). parameter poking is a material-asset edit, not a component edit.
    public string Material {
      get
      {
        unsafe
        {
          return Utf8Arg.Read(NativeGetMaterialPath(ObjectId));
        }
      }
      set
      {
        unsafe
        {
          using var value_utf8 = new Utf8Arg(value ?? "", stackalloc byte[Utf8Arg.StackSize]);
          NativeSetMaterialPath(ObjectId, value_utf8.Pointer);
        }
      }
    }

    [NativeField("visible")]
    public bool Visible { get => GetBool(); set => SetBool(value); }
    [NativeField("tint")]
    public Vec4 Tint { get => GetVec4(); set => SetVec4(value); }
    [NativeField("model_asset_id")]
    public UInt64 ModelAssetId { get => GetU64(); set => SetU64(value); }
  }
#nullable disable
}
