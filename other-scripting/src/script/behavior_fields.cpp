/**
 * \file script/behavior_fields.cpp
 **/
#include "script/behavior_fields.hpp"

#include <limits>

namespace other {
  namespace behavior_fields {
    namespace {

      template <typename T>
      bool read_raw(std::span<const uint8_t> data, size_t& offset, T& out) {
        if (offset + sizeof(T) > data.size()) {
          return false;
        }
        std::memcpy(&out, data.data() + offset, sizeof(T));
        offset += sizeof(T);
        return true;
      }

      template <typename T>
      void write_raw(const T& value, ostd::vector<uint8_t>& out) {
        const uint8_t* raw = reinterpret_cast<const uint8_t*>(&value);
        out.insert(out.end(), raw, raw + sizeof(T));
      }

      /// false when the stream is cut short. rewritten gets the stream's fields with what
      ///  change changed, without the count
      bool rewrite_fields(std::span<const uint8_t> stream, const change_fn& change, writer& rewritten) {
        ostd::vector<field> fields = {};
        if (!read(stream, fields)) {
          return false;
        }

        ostd::vector<uint8_t> body = {};
        for (const field& f : fields) {
          if (f.kind == behavior_field_kind::STRUCT) {
            writer nested = {};
            if (!rewrite_fields(f.body, change, nested)) {
              return false;
            }
            rewritten.add(f.name, f.kind, nested.take());
            continue;
          }

          body.clear();
          if (change(f, body)) {
            rewritten.add(f.name, f.kind, body);
          } else {
            rewritten.add(f.name, f.kind, f.body);
          }
        }
        return true;
      }

      void collect_assets_into(std::span<const uint8_t> stream, ostd::vector<std::string>& out) {
        ostd::vector<field> fields = {};
        if (!read(stream, fields)) {
          return;
        }
        for (const field& f : fields) {
          if (f.kind == behavior_field_kind::STRUCT) {
            collect_assets_into(f.body, out);
          } else if (f.kind == behavior_field_kind::ASSET && !f.body.empty()) {
            out.emplace_back(f.text());
          }
        }
      }

    }  // namespace

    bool read(std::span<const uint8_t> stream, ostd::vector<field>& out) {
      out.clear();
      if (stream.empty()) {
        return true;
      }

      size_t offset = 0;
      uint32_t count = 0;
      if (!read_raw(stream, offset, count)) {
        return false;
      }

      for (uint32_t i = 0; i < count; ++i) {
        uint16_t name_length = 0;
        if (!read_raw(stream, offset, name_length) || offset + name_length > stream.size()) {
          out.clear();
          return false;
        }
        field f = { .name = std::string_view{ reinterpret_cast<const char*>(stream.data() + offset), name_length } };
        offset += name_length;

        uint8_t kind = 0;
        uint32_t size = 0;
        if (!read_raw(stream, offset, kind) || !read_raw(stream, offset, size) || offset + size > stream.size()) {
          out.clear();
          return false;
        }
        /// a kind a later build added is kept as bytes and matches nothing here
        f.kind = kind < static_cast<uint8_t>(behavior_field_kind::NUM_KINDS) ? static_cast<behavior_field_kind>(kind) : behavior_field_kind::NONE;
        f.body = stream.subspan(offset, size);
        offset += size;
        out.push_back(f);
      }
      return true;
    }

    writer::writer() {
      write_raw<uint32_t>(0, bytes);
    }

    void writer::add(const std::string_view name, behavior_field_kind kind, std::span<const uint8_t> body) {
      OTHER_ASSERT(name.size() <= std::numeric_limits<uint16_t>::max(), "A behavior field's name is {} bytes long.", name.size());
      OTHER_ASSERT(body.size() <= std::numeric_limits<uint32_t>::max(), "The behavior field '{}' is too large to save.", name);
      write_raw<uint16_t>(static_cast<uint16_t>(name.size()), bytes);
      bytes.insert(bytes.end(), name.begin(), name.end());
      write_raw<uint8_t>(static_cast<uint8_t>(kind), bytes);
      write_raw<uint32_t>(static_cast<uint32_t>(body.size()), bytes);
      bytes.insert(bytes.end(), body.begin(), body.end());
      ++fields;
    }

    void writer::add_text(const std::string_view name, behavior_field_kind kind, const std::string_view text) {
      add(name, kind, std::span<const uint8_t>{ reinterpret_cast<const uint8_t*>(text.data()), text.size() });
    }

    ostd::vector<uint8_t> writer::take() {
      std::memcpy(bytes.data(), &fields, sizeof(fields));
      ostd::vector<uint8_t> stream = std::move(bytes);
      bytes.clear();
      write_raw<uint32_t>(0, bytes);
      fields = 0;
      return stream;
    }

    bool read_description(std::span<const uint8_t> stream, ostd::vector<behavior_field_description>& out) {
      out.clear();
      ostd::vector<field> fields = {};
      if (!read(stream, fields)) {
        return false;
      }

      for (const field& f : fields) {
        ostd::vector<field> said = {};
        if (f.kind != behavior_field_kind::STRUCT || !read(f.body, said)) {
          out.clear();
          return false;
        }

        behavior_field_description description = { .name = std::string{ f.name } };
        for (const field& about : said) {
          uint32_t kind = 0;
          uint8_t flag = 0;
          if (about.name == "kind" && about.value(kind) && kind < static_cast<uint32_t>(behavior_field_kind::NUM_KINDS)) {
            description.kind = static_cast<behavior_field_kind>(kind);
          } else if (about.name == "label") {
            description.label = about.text();
          } else if (about.name == "tooltip") {
            description.tooltip = about.text();
          } else if (about.name == "group") {
            description.group = about.text();
          } else if (about.name == "min" && about.value(description.range_min)) {
            description.has_range = true;
          } else if (about.name == "max" && about.value(description.range_max)) {
            description.has_range = true;
          } else if (about.name == "read-only" && about.value(flag)) {
            description.read_only = flag != 0;
          } else if (about.name == "color" && about.value(flag)) {
            description.color = flag != 0;
          } else if (about.name == "separator" && about.value(flag)) {
            description.separator = flag != 0;
          } else if (about.name == "members") {
            std::string_view members = about.text();
            while (!members.empty()) {
              const size_t end = members.find('\n');
              description.members.emplace_back(members.substr(0, end));
              members = end == std::string_view::npos ? std::string_view{} : members.substr(end + 1);
            }
          } else if (about.name == "fields" && !read_description(about.body, description.fields)) {
            out.clear();
            return false;
          }
        }
        out.push_back(std::move(description));
      }
      return true;
    }

    ostd::vector<uint8_t> rewrite(std::span<const uint8_t> stream, const change_fn& change) {
      writer rewritten = {};
      if (stream.empty() || !rewrite_fields(stream, change, rewritten)) {
        return ostd::vector<uint8_t>{ stream.begin(), stream.end() };
      }
      return rewritten.take();
    }

    ostd::vector<uint8_t> remap_objects(std::span<const uint8_t> stream, const ostd::map<natural_t, natural_t>& ids) {
      return rewrite(stream, [&ids](const field& f, ostd::vector<uint8_t>& out_body) -> bool {
        natural_t id = 0;
        if (f.kind != behavior_field_kind::OBJECT || !f.value(id) || id == 0) {
          return false;
        }
        const auto found = ids.find(id);
        write_raw<natural_t>(found != ids.end() ? found->second : 0, out_body);
        return true;
      });
    }

    void collect_assets(std::span<const uint8_t> stream, ostd::vector<std::string>& out) {
      collect_assets_into(stream, out);
    }

    ostd::vector<uint8_t> rewrite_assets(std::span<const uint8_t> stream, const std::function<std::string(const std::string&)>& rewrite_path) {
      return rewrite(stream, [&rewrite_path](const field& f, ostd::vector<uint8_t>& out_body) -> bool {
        if (f.kind != behavior_field_kind::ASSET || f.body.empty()) {
          return false;
        }
        const std::string path = rewrite_path(std::string{ f.text() });
        out_body.insert(out_body.end(), path.begin(), path.end());
        return true;
      });
    }

  }  // namespace behavior_fields
}  // namespace other
