/**
 * \file script/behavior_fields.hpp
 * a behaviour's saved fields as one stream that says what each field is, so that it can be
 * read, written to a text scene and packaged with no script loaded:
 *
 *   stream := [count u32] then count fields
 *   field  := [name: length u16, UTF-8] [kind u8] [size u32] [bytes]
 *   kinds  :  BOOL (u8)  INT32  UINT32  INT64  UINT64  FLOAT  DOUBLE   the value
 *             TEXT  ENUM  ASSET                              UTF-8: the text, the member's name, the path
 *             VEC2  VEC3  VEC4  QUAT (x y z w)               floats
 *             STRUCT                                         a stream
 *             OBJECT                                         u64: a scene object's id, 0 for none
 *
 * every field can be skipped by its size alone. reading never asserts on the bytes: a stream
 * that is cut short is reported, not trusted
 **/
#ifndef OTHER_SCRIPTING_SCRIPT_BEHAVIOR_FIELDS_HPP
#define OTHER_SCRIPTING_SCRIPT_BEHAVIOR_FIELDS_HPP

#include <cstring>
#include <functional>
#include <span>
#include <string_view>

#include "core/defines.hpp"

namespace other {

  /// \note FieldKind in C# is the same list in the same order
  enum class behavior_field_kind : uint8_t {
    NONE = 0,
    BOOL,
    INT32,
    UINT32,
    INT64,
    UINT64,
    FLOAT,
    DOUBLE,
    TEXT,
    ENUM,
    ASSET,
    VEC2,
    VEC3,
    VEC4,
    QUAT,
    STRUCT,
    OBJECT,

    NUM_KINDS,
  };

  /// what one saved field of a behaviour's type is, for whoever draws it. a runtime says
  ///  it as a stream too: a struct for each field, named as the field is, that holds "kind",
  ///  and where they apply "label", "tooltip", "group", "min" and "max", "read-only",
  ///  "color", "separator", "members" (an enum's, a line each) and "fields" (a struct's own)
  struct behavior_field_description {
    std::string name;
    behavior_field_kind kind = behavior_field_kind::NONE;
    /// what to show in place of the name; empty for the name itself
    std::string label;
    std::string tooltip;
    /// the fields that share a group are drawn under its name
    std::string group;

    bool read_only = false;
    bool color = false;
    /// a line is drawn above it
    bool separator = false;
    bool has_range = false;
    float range_min = 0.f;
    float range_max = 0.f;

    /// an enum's members, by name
    ostd::vector<std::string> members;
    /// a struct's own fields
    ostd::vector<behavior_field_description> fields;
  };

  namespace behavior_fields {

    /// one field of a stream. name and body point into the stream it was read from
    struct field {
      std::string_view name = {};
      behavior_field_kind kind = behavior_field_kind::NONE;
      std::span<const uint8_t> body = {};

      /// false when the body is not the size of a T
      template <typename T>
        requires std::is_trivially_copyable_v<T>
      bool value(T& out) const {
        if (body.size() != sizeof(T)) {
          return false;
        }
        std::memcpy(&out, body.data(), sizeof(T));
        return true;
      }

      std::string_view text() const { return std::string_view{ reinterpret_cast<const char*>(body.data()), body.size() }; }
    };

    /// the fields of a stream, in its order. false when the stream is cut short; a stream
    ///  of no bytes has no fields
    bool read(std::span<const uint8_t> stream, ostd::vector<field>& out);

    /// builds a stream a field at a time
    class writer {
     public:
      writer();

      void add(const std::string_view name, behavior_field_kind kind, std::span<const uint8_t> body);
      void add_text(const std::string_view name, behavior_field_kind kind, const std::string_view text);

      template <typename T>
        requires std::is_trivially_copyable_v<T>
      void add_value(const std::string_view name, behavior_field_kind kind, const T& value) {
        add(name, kind, std::span<const uint8_t>{ reinterpret_cast<const uint8_t*>(&value), sizeof(T) });
      }

      uint32_t count() const { return fields; }
      /// the stream, with its count. the writer is empty again afterwards
      ostd::vector<uint8_t> take();

     private:
      ostd::vector<uint8_t> bytes;
      uint32_t fields = 0;
    };

    /// asked about every field that is not a struct, those inside structs too. true when it
    ///  put other bytes for the field in out_body; false leaves the field as it is
    using change_fn = std::function<bool(const field&, ostd::vector<uint8_t>& out_body)>;
    /// the stream again with what change changed. a stream cut short comes back as it is
    ostd::vector<uint8_t> rewrite(std::span<const uint8_t> stream, const change_fn& change);

    /// every object id through the map. one the map does not have becomes none: it was an
    ///  object outside the document the stream came from
    ostd::vector<uint8_t> remap_objects(std::span<const uint8_t> stream, const ostd::map<natural_t, natural_t>& ids);

    /// what a runtime's describe wrote. false when it is cut short
    bool read_description(std::span<const uint8_t> stream, ostd::vector<behavior_field_description>& out);

    /// every asset path that is not empty, those inside structs too
    void collect_assets(std::span<const uint8_t> stream, ostd::vector<std::string>& out);
    ostd::vector<uint8_t> rewrite_assets(std::span<const uint8_t> stream, const std::function<std::string(const std::string&)>& rewrite);

  }  // namespace behavior_fields
}  // namespace other

#endif  // OTHER_SCRIPTING_SCRIPT_BEHAVIOR_FIELDS_HPP
