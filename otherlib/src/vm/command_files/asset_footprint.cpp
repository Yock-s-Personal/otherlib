/**
 * \file vm/command_files/asset_footprint.cpp
 **/
#include "vm/command_files/asset_footprint.hpp"

#include "core/profiler.hpp"

namespace other {
  namespace {

    /// the numbers in a footprint are the footprint's own and never an enum's of the code, so
    ///  a file keeps its meaning if that enum is reordered
    enum footprint_value_type : uint8_t {
      FOOTPRINT_INTEGER = 1,
      FOOTPRINT_DECIMAL = 2,
      FOOTPRINT_BOOL = 3,
      FOOTPRINT_TEXT = 4,
      FOOTPRINT_WORD = 5,
      FOOTPRINT_LIST = 6,
    };

    enum footprint_field_kind : uint8_t {
      FOOTPRINT_SET = 1,
      FOOTPRINT_REF = 2,
      FOOTPRINT_USE = 3,
    };

    /// a footprint read from a file is not to be trusted: nothing in one nests this deep
    constexpr size_t kDeepest = 64;

    /// every number is written low byte first, whatever machine writes it
    template <std::unsigned_integral T>
    void put(ostd::vector<uint8_t>& out, const T value) {
      for (size_t i = 0; i < sizeof(T); ++i) {
        out.push_back(static_cast<uint8_t>(value >> (i * 8)));
      }
    }

    void put_text(ostd::vector<uint8_t>& out, const std::string_view text) {
      put(out, static_cast<uint32_t>(text.size()));
      out.insert(out.end(), text.begin(), text.end());
    }

    template <std::unsigned_integral T>
    bool take(std::span<const uint8_t>& from, T& value) {
      if (from.size() < sizeof(T)) {
        return false;
      }
      value = 0;
      for (size_t i = 0; i < sizeof(T); ++i) {
        value = static_cast<T>(value | (static_cast<T>(from[i]) << (i * 8)));
      }
      from = from.subspan(sizeof(T));
      return true;
    }

    bool take_text(std::span<const uint8_t>& from, std::string& text) {
      uint32_t length = 0;
      if (!take(from, length) || length > from.size()) {
        return false;
      }
      text.assign(reinterpret_cast<const char*>(from.data()), length);
      from = from.subspan(length);
      return true;
    }

    void put_value(ostd::vector<uint8_t>& out, const asset_value& value) {
      switch (value.kind) {
        case ASSET_VALUE_INTEGER:
          out.push_back(FOOTPRINT_INTEGER);
          put(out, static_cast<uint64_t>(value.integer));
          break;
        case ASSET_VALUE_DECIMAL:
          out.push_back(FOOTPRINT_DECIMAL);
          /// zero has two spellings in bits, and one asset cannot have two footprints
          put(out, std::bit_cast<uint64_t>(value.decimal == 0.0 ? 0.0 : value.decimal));
          break;
        case ASSET_VALUE_BOOL:
          out.push_back(FOOTPRINT_BOOL);
          out.push_back(value.integer != 0 ? 1 : 0);
          break;
        case ASSET_VALUE_TEXT:
          out.push_back(FOOTPRINT_TEXT);
          put_text(out, value.text);
          break;
        case ASSET_VALUE_WORD:
          out.push_back(FOOTPRINT_WORD);
          put_text(out, value.text);
          break;
        case ASSET_VALUE_LIST:
          out.push_back(FOOTPRINT_LIST);
          put(out, static_cast<uint32_t>(value.items.size()));
          for (const asset_value& item : value.items) {
            put_value(out, item);
          }
          break;
        default: OTHER_ASSERT(false, "an asset value of no kind cannot be written"); break;
      }
    }

    bool take_value(std::span<const uint8_t>& from, asset_value& value, const size_t depth) {
      uint8_t type = 0;
      if (depth > kDeepest || !take(from, type)) {
        return false;
      }
      switch (type) {
        case FOOTPRINT_INTEGER: {
          uint64_t bits = 0;
          value.kind = ASSET_VALUE_INTEGER;
          if (!take(from, bits)) {
            return false;
          }
          value.integer = static_cast<int64_t>(bits);
          return true;
        }

        case FOOTPRINT_DECIMAL: {
          uint64_t bits = 0;
          value.kind = ASSET_VALUE_DECIMAL;
          if (!take(from, bits)) {
            return false;
          }
          value.decimal = std::bit_cast<double>(bits);
          return true;
        }

        case FOOTPRINT_BOOL: {
          uint8_t truth = 0;
          value.kind = ASSET_VALUE_BOOL;
          if (!take(from, truth) || truth > 1) {
            return false;
          }
          value.integer = truth;
          return true;
        }

        case FOOTPRINT_TEXT:
          value.kind = ASSET_VALUE_TEXT;
          return take_text(from, value.text);
        case FOOTPRINT_WORD:
          value.kind = ASSET_VALUE_WORD;
          return take_text(from, value.text);

        case FOOTPRINT_LIST: {
          uint32_t count = 0;
          value.kind = ASSET_VALUE_LIST;
          /// an item is a byte at the least, so a count past what is left is a lie
          if (!take(from, count) || count > from.size()) {
            return false;
          }
          value.items.resize(count);
          for (asset_value& item : value.items) {
            if (!take_value(from, item, depth + 1)) {
              return false;
            }
          }
          return true;
        }

        default: return false;
      }
    }

    void put_node(ostd::vector<uint8_t>& out, const asset_node& node) {
      put(out, asset_hash(node.kind));
      put_text(out, node.name);

      /// by name, so the order they were written in, and the order the struct has them in, is not in it
      ostd::vector<const asset_field*> fields;
      for (const asset_field& field : node.fields) {
        fields.push_back(&field);
      }
      std::ranges::sort(fields, {}, [](const asset_field* field) -> const std::string& { return field->name; });

      ostd::vector<const asset_edge*> edges;
      for (const asset_edge& edge : node.edges) {
        const bool repeated = std::ranges::any_of(edges, [&edge](const asset_edge* earlier) { return *earlier == edge; });
        if (!repeated) {
          edges.push_back(&edge);
        }
      }
      std::ranges::sort(edges, [](const asset_edge* a, const asset_edge* b) { return std::tie(a->from, a->to) < std::tie(b->from, b->to); });

      put(out, static_cast<uint32_t>(fields.size()));
      put(out, static_cast<uint32_t>(node.parts.size()));
      put(out, static_cast<uint32_t>(edges.size()));
      for (const asset_field* field : fields) {
        put(out, asset_hash(field->name));
        out.push_back(field->how == ASSET_FIELD_SET ? FOOTPRINT_SET : field->how == ASSET_FIELD_REF ? FOOTPRINT_REF :
                                                                                                      FOOTPRINT_USE);
        put_value(out, field->value);
      }
      for (const asset_node& part : node.parts) {
        put_node(out, part);
      }
      for (const asset_edge* edge : edges) {
        put_text(out, edge->from);
        put_text(out, edge->to);
      }
    }

    bool take_node(std::span<const uint8_t>& from, footprint_node& node, const size_t depth) {
      uint32_t field_count = 0;
      uint32_t part_count = 0;
      uint32_t edge_count = 0;
      if (depth > kDeepest || !take(from, node.kind) || !take_text(from, node.name) || !take(from, field_count) || !take(from, part_count) || !take(from, edge_count)) {
        return false;
      }
      if (field_count > from.size() || part_count > from.size() || edge_count > from.size()) {
        return false;
      }

      node.fields.resize(field_count);
      for (footprint_field& field : node.fields) {
        uint8_t how = 0;
        if (!take(from, field.name) || !take(from, how) || how < FOOTPRINT_SET || how > FOOTPRINT_USE || !take_value(from, field.value, depth + 1)) {
          return false;
        }
        field.how = how == FOOTPRINT_SET ? ASSET_FIELD_SET : how == FOOTPRINT_REF ? ASSET_FIELD_REF :
                                                                                    ASSET_FIELD_USE;
      }
      node.parts.resize(part_count);
      for (footprint_node& part : node.parts) {
        if (!take_node(from, part, depth + 1)) {
          return false;
        }
      }
      node.edges.resize(edge_count);
      for (asset_edge& edge : node.edges) {
        if (!take_text(from, edge.from) || !take_text(from, edge.to)) {
          return false;
        }
      }
      return true;
    }

    template <typename T>
    void append(ostd::vector<uint8_t>& out, const T& value) {
      const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);
      out.insert(out.end(), bytes, bytes + sizeof(T));
    }

  }  // namespace

  uint64_t asset_hash(const std::span<const uint8_t> bytes) {
    return asset_hash(std::string_view{ reinterpret_cast<const char*>(bytes.data()), bytes.size() });
  }

  const footprint_field* footprint_node::field(const uint64_t name) const {
    const auto found = std::ranges::find(fields, name, &footprint_field::name);
    return found != fields.end() ? &*found : nullptr;
  }

  ostd::vector<uint8_t> asset_footprint(const asset_node& asset) {
    PROFILE_SECTION("asset_footprint");
    ostd::vector<uint8_t> bytes;
    put_node(bytes, asset);
    return bytes;
  }

  opt<footprint_node> read_asset_footprint(const std::span<const uint8_t> bytes) {
    PROFILE_SECTION("read_asset_footprint");
    std::span<const uint8_t> rest = bytes;
    footprint_node node;
    if (!take_node(rest, node, 0) || !rest.empty()) {
      return std::nullopt;
    }
    return node;
  }

  opt<std::string> asset_footprint_refusal(const asset_node& asset) {
    for (size_t i = 0; i < asset.fields.size(); ++i) {
      for (size_t j = 0; j < i; ++j) {
        if (asset_hash(asset.fields[i].name) == asset_hash(asset.fields[j].name)) {
          return std::format("'{}' and '{}' of {} '{}' have the same hash, so a footprint cannot tell them apart; one of them needs another name", asset.fields[j].name, asset.fields[i].name, asset.kind, asset.name);
        }
      }
    }
    for (const asset_node& part : asset.parts) {
      if (opt<std::string> refusal = asset_footprint_refusal(part); refusal.has_value()) {
        return refusal;
      }
    }
    /// what is written has to be what can be read, and nothing is read past kDeepest
    if (!read_asset_footprint(asset_footprint(asset)).has_value()) {
      return std::format("{} '{}' nests deeper than {}, which is as deep as a footprint is read", asset.kind, asset.name, kDeepest);
    }
    return std::nullopt;
  }

  const obin_asset* obin_assets::find(const uint64_t kind, const uint64_t name) const {
    const auto found = std::ranges::find_if(assets, [&](const obin_asset& asset) { return asset.kind == kind && asset.name == name; });
    return found != assets.end() ? &*found : nullptr;
  }

  ostd::vector<uint8_t> obin_assets::encode(const std::span<const asset_node* const> assets) {
    PROFILE_SECTION("obin_assets::encode");
    struct written {
      asset_table_row row;
      ostd::vector<uint8_t> footprint;
    };
    ostd::vector<written> all;
    all.reserve(assets.size());
    for (const asset_node* asset : assets) {
      OTHER_ASSERT(asset != nullptr, "An asset to write cannot be null");
      written& one = all.emplace_back();
      one.footprint = asset_footprint(*asset);
      one.row.kind = asset_hash(asset->kind);
      one.row.name = asset_hash(asset->name);
      one.row.size = static_cast<uint32_t>(one.footprint.size());
      const uint64_t hash = asset_hash(std::span<const uint8_t>{ one.footprint });
      for (size_t i = 0; i < sizeof(hash); ++i) {
        one.row.hash[i] = static_cast<uint8_t>(hash >> (i * 8));
      }
    }
    /// by kind and name, so what else is linked and the order of the sources is not in it
    std::ranges::sort(all, [](const written& a, const written& b) { return std::tie(a.row.kind, a.row.name) < std::tie(b.row.kind, b.row.name); });

    size_t offset = sizeof(asset_section_header) + all.size() * sizeof(asset_table_row);
    for (size_t i = 0; i < all.size(); ++i) {
      OTHER_ASSERT(i == 0 || std::tie(all[i - 1].row.kind, all[i - 1].row.name) != std::tie(all[i].row.kind, all[i].row.name), "Two assets of one kind and name cannot be in one section");
      all[i].row.offset = static_cast<uint32_t>(offset);
      offset += all[i].footprint.size();
    }

    ostd::vector<uint8_t> section;
    section.reserve(offset);
    append(section, asset_section_header{ .size = static_cast<uint32_t>(offset), .asset_count = static_cast<uint32_t>(all.size()) });
    for (const written& one : all) {
      append(section, one.row);
    }
    for (const written& one : all) {
      section.insert(section.end(), one.footprint.begin(), one.footprint.end());
    }
    return section;
  }

  opt<obin_assets> obin_assets::decode(const std::span<const uint8_t> section) {
    PROFILE_SECTION("obin_assets::decode");
    asset_section_header header;
    if (section.size() < sizeof(header)) {
      return std::nullopt;
    }
    std::memcpy(&header, section.data(), sizeof(header));
    const size_t rows_end = sizeof(header) + static_cast<size_t>(header.asset_count) * sizeof(asset_table_row);
    if (std::string_view{ header.signature, sizeof(header.signature) } != "OAST" || header.version != kAssetSectionVersion || header.size != section.size() || rows_end > section.size()) {
      return std::nullopt;
    }

    obin_assets result;
    result.assets.reserve(header.asset_count);
    for (size_t i = 0; i < header.asset_count; ++i) {
      asset_table_row row;
      std::memcpy(&row, section.data() + sizeof(header) + i * sizeof(row), sizeof(row));
      if (row.offset < rows_end || row.offset > section.size() || row.size > section.size() - row.offset) {
        return std::nullopt;
      }
      obin_asset& asset = result.assets.emplace_back();
      asset.kind = row.kind;
      asset.name = row.name;
      asset.offset = row.offset;
      asset.footprint = section.subspan(row.offset, row.size);
      for (size_t byte = 0; byte < sizeof(asset.hash); ++byte) {
        asset.hash |= static_cast<uint64_t>(row.hash[byte]) << (byte * 8);
      }
      /// a footprint that is not what its row says it is was changed after it was written
      if (asset.hash != asset_hash(asset.footprint)) {
        return std::nullopt;
      }
    }
    return result;
  }

}  // namespace other
