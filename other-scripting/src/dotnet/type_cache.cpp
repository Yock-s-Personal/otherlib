/**
 * \file dotnet/type_cache.cpp
 **/
#include "dotnet/type_cache.hpp"

#include "core/fnv.hpp"
#include "core/profiler.hpp"

#include "dotnet/dotnet_assembly.hpp"
#include "dotnet/dotnet_host.hpp"

namespace other {
  namespace {

    bool is_driver_module(const dotnet_type& type) {
      return type.owner != nullptr && type.owner->driver_module;
    }

    std::string_view assembly_name_of(const dotnet_type& type) {
      return type.owner != nullptr ? std::string_view{ type.owner->get_name() } : std::string_view{ "no loaded assembly" };
    }

    /// whether candidate is the one meant, of two types with one name
    bool is_chosen_over(const dotnet_type& candidate, const dotnet_type& other) {
      if (is_driver_module(candidate) != is_driver_module(other)) {
        return !is_driver_module(candidate);
      }
      if (assembly_name_of(candidate) != assembly_name_of(other)) {
        return assembly_name_of(candidate) < assembly_name_of(other);
      }
      return candidate.dotnet_id < other.dotnet_id;
    }

  }  // namespace

  dotnet_type* type_cache::cache_type(dotnet_host* host, int32_t dotnet_id) {
    auto [itr, success] = cached_types.insert({ dotnet_id, dotnet_type(host, dotnet_id) });
    if (!success) {
      CORE_LOG_ERROR("Failed to cache .NET type [{}]", dotnet_id);
    }

    itr->second.initialize_type_interface();
    /// what the name was found as before may not be the one meant now
    name_cache.erase(FNV(itr->second.full_name()));
    return &itr->second;
  }

  void type_cache::remove_type(int32_t dotnet_handle) {
    auto itr = cached_types.find(dotnet_handle);
    if (itr == cached_types.end()) {
      CORE_LOG_ERROR("Failed to remove .NET type [{}] from cache: not found", dotnet_handle);
      return;
    }

    name_cache.erase(FNV(itr->second.full_name()));
    id_cache.erase(dotnet_handle);
    cached_types.erase(itr);
  }

  void type_cache::clear_cache(dotnet_host* host) {
    cached_types.clear();
    name_cache.clear();
    id_cache.clear();
  }

  dotnet_type* type_cache::get_type(const std::string_view name) {
    PROFILE_SECTION("type_cache::get_type");
    const uint64_t key = FNV(name);
    if (const auto known = name_cache.find(key); known != name_cache.end()) {
      return known->second;
    }

    dotnet_type* chosen = nullptr;
    ostd::vector<dotnet_type*> named = {};
    for (auto& [id, type] : cached_types) {
      if (type.full_name() != name) {
        continue;
      }
      named.push_back(&type);
      if (chosen == nullptr || is_chosen_over(type, *chosen)) {
        chosen = &type;
      }
    }
    if (chosen == nullptr) {
      return nullptr;
    }
    name_cache[key] = chosen;

    /// said when the name is first found, which is once until an assembly comes or goes
    if (named.size() > 1) {
      std::string others = "";
      bool same_kind = false;
      for (const dotnet_type* type : named) {
        if (type == chosen) {
          continue;
        }
        others += std::format("{}'{}'", others.empty() ? "" : ", ", assembly_name_of(*type));
        same_kind |= is_driver_module(*type) == is_driver_module(*chosen);
      }
      if (same_kind) {
        CORE_LOG_ERROR("More than one loaded script assembly has a type named '{}'. The one in '{}' is used, not the one in {}: rename one of them, or put it in a namespace.", name, assembly_name_of(*chosen), others);
      } else {
        CORE_LOG_WARN("The type '{}' is in '{}' and also in {}, which the driver loaded for its own use. The one in '{}' is used.", name, assembly_name_of(*chosen), others, assembly_name_of(*chosen));
      }
    }
    return chosen;
  }

  dotnet_type* type_cache::get_type(int32_t id) {
    bool contains = id_cache.contains(id);
    dotnet_type* res = nullptr;
    if (!contains) {
      auto itr = cached_types.find(id);
      if (itr != cached_types.end()) {
        res = &itr->second;
        id_cache[id] = res;
      }
    } else {
      res = id_cache[id];
    }
    return res;
  }

}  // namespace other