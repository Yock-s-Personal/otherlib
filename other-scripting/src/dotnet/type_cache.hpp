/**
 * \file dotnet/type_cache.hpp
 **/
#ifndef OTHER_SCRIPTING_DOTNET_TYPE_CACHE_HPP
#define OTHER_SCRIPTING_DOTNET_TYPE_CACHE_HPP

#include <cstdint>

#include "core/memory_pool.hpp"
#include "core/ref.hpp"

#include "dotnet/dotnet_type.hpp"

namespace other {

  class dotnet_host;

  class type_cache {
   public:
    type_cache() = default;
    ~type_cache() = default;

    dotnet_type* cache_type(dotnet_host* host, int32_t dotnet_handle);
    void remove_type(int32_t dotnet_handle);

    void clear_cache(dotnet_host* interop);

    /// by full name. more than one loaded assembly may declare a type of the name: one that
    ///  is not a driver's module is chosen over one that is, and it is said, once, that the
    ///  name is taken twice. between two of a kind the choice is by assembly name, so it is
    ///  the same choice every run, and that is said as an error
    dotnet_type* get_type(const std::string_view name);
    dotnet_type* get_type(int32_t id);

    [[nodiscard]] auto begin() { return cached_types.begin(); }
    [[nodiscard]] auto end() { return cached_types.end(); }
    [[nodiscard]] auto rbegin() { return cached_types.rbegin(); }
    [[nodiscard]] auto rend() { return cached_types.rend(); }

    [[nodiscard]] auto begin() const { return cached_types.begin(); }
    [[nodiscard]] auto end() const { return cached_types.end(); }
    [[nodiscard]] auto rbegin() const { return cached_types.rbegin(); }
    [[nodiscard]] auto rend() const { return cached_types.rend(); }

    auto size() const { return cached_types.size(); }

   private:
    ostd::map<int32_t, dotnet_type> cached_types;

    ostd::unordered_map<uint64_t, dotnet_type*> name_cache;
    ostd::unordered_map<int32_t, dotnet_type*> id_cache;
  };

}  // namespace other

#endif  // OTHER_SCRIPTING_DOTNET_TYPE_CACHE_HPP