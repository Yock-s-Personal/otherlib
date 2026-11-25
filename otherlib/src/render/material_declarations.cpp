/**
 * \file render/material_declarations.cpp
 **/
#include "render/material_declarations.hpp"

#include "core/fnv.hpp"
#include "core/logger.hpp"
#include "core/profiler.hpp"

#include "render/pipeline_declarations.hpp"

namespace other {
  namespace {

    /// the fields the engine's material struct has. a footprint holds the hash of a field's
    ///  name, so these are the names the engine can give back; any other parameter of a
    ///  layout is written 'set name value', which keeps the name
    constexpr std::string_view kFields[] = { "base_color", "emissive_color", "roughness", "metalness" };

    bool is_number(const asset_value& value) {
      return value.kind == ASSET_VALUE_INTEGER || value.kind == ASSET_VALUE_DECIMAL;
    }

    /// as the TOML reader takes a value: a float, a whole number, true or false, or two to
    ///  four numbers
    opt<material_value> value_from(const asset_value& given) {
      switch (given.kind) {
        case ASSET_VALUE_DECIMAL: return material_value::from(static_cast<float>(given.decimal));
        case ASSET_VALUE_INTEGER: return material_value::from(static_cast<int32_t>(given.integer));
        case ASSET_VALUE_BOOL: return material_value::from(given.integer != 0);
        case ASSET_VALUE_LIST: {
          if (given.items.size() < 2 || given.items.size() > 4 || !std::ranges::all_of(given.items, is_number)) {
            return std::nullopt;
          }
          glm::vec4 numbers{ 0.f };
          for (size_t i = 0; i < given.items.size(); ++i) {
            const asset_value& item = given.items[i];
            numbers[static_cast<glm::length_t>(i)] = item.kind == ASSET_VALUE_DECIMAL ? static_cast<float>(item.decimal) : static_cast<float>(item.integer);
          }
          switch (given.items.size()) {
            case 2: return material_value::from(glm::vec2(numbers));
            case 3: return material_value::from(glm::vec3(numbers));
            default: return material_value::from(numbers);
          }
        }

        default: return std::nullopt;
      }
    }

  }  // namespace

  material_parse_result material_from_footprint(const footprint_node& footprint, const filepath& source) {
    PROFILE_SECTION("material_from_footprint");
    material_parse_result result;
    if (footprint.kind != asset_hash("material")) {
      result.error = std::format("the asset '{}' of '{}' is not a material", footprint.name, source.string());
      return result;
    }

    material made;
    made.name = footprint.name.empty() ? source.stem().string() : footprint.name;
    const auto give = [&](const std::string_view name, const asset_value& given) -> void {
      const opt<material_value> value = value_from(given);
      if (!value.has_value()) {
        result.warnings.push_back(
          std::format("material '{}': param '{}' has an unsupported value shape (expected a number, true or false, or two to four numbers); ignored", source.string(), name));
        return;
      }
      const natural_t hash = FNV(name);
      made.params[hash] = *value;
      made.param_names[hash] = std::string{ name };
    };

    for (const footprint_field& field : footprint.fields) {
      const auto name = std::ranges::find(kFields, field.name, [](const std::string_view known) { return asset_hash(known); });
      if (name == std::end(kFields)) {
        result.warnings.push_back(std::format("material '{}': it has a field the engine has no name for; a parameter that is not one of the material struct's is written 'set name value'; ignored", source.string()));
        continue;
      }
      give(*name, field.value);
    }
    for (const footprint_node& part : footprint.parts) {
      if (part.kind == asset_hash("set")) {
        const footprint_field* given = part.field(asset_hash("value"));
        if (given != nullptr && !part.name.empty()) {
          give(part.name, given->value);
        }
      } else if (part.kind == asset_hash("map")) {
        const footprint_field* file = part.field(asset_hash("file"));
        if (file == nullptr || file->value.kind != ASSET_VALUE_TEXT || part.name.empty()) {
          result.warnings.push_back(std::format("material '{}': texture slot '{}' is not a path; ignored", source.string(), part.name));
          continue;
        }
        made.texture_paths[FNV(part.name)] = file->value.text;
      }
    }

    result.mat = std::move(made);
    return result;
  }

  material_parse_result read_material_declarations(const filepath& path) {
    PROFILE_SECTION("read_material_declarations");
    const opt<ostd::vector<asset_node>> assets = read_declared_assets(path);
    if (!assets.has_value()) {
      return { .error = std::format("failed to read material '{}' as a file of declarations; the errors are in the log", path.string()) };
    }
    const auto declared = std::ranges::find(*assets, std::string{ "material" }, &asset_node::kind);
    if (declared == assets->end()) {
      return { .error = std::format("'{}' declares no material", path.string()) };
    }

    /// through the bytes a linked program carries, as a pipeline is read
    if (const opt<std::string> refusal = asset_footprint_refusal(*declared); refusal.has_value()) {
      return { .error = std::format("material '{}': {}", path.string(), *refusal) };
    }
    const ostd::vector<uint8_t> bytes = asset_footprint(*declared);
    const opt<footprint_node> footprint = read_asset_footprint(bytes);
    OTHER_ASSERT(footprint.has_value(), "A footprint that was just written could not be read: {}", path.string());
    material_parse_result result = material_from_footprint(*footprint, path);
    if (result.success()) {
      CORE_LOG_INFO("[RENDERING] : material '{}' read from its declaration in '{}': a footprint of {} bytes, hash {:016x}", result.mat->name, path.string(), bytes.size(),
                    asset_hash(std::span<const uint8_t>{ bytes }));
    }
    return result;
  }

}  // namespace other
