/**
 * \file vm/hooks.cpp
 **/
#include "vm/hooks.hpp"

#include "core/logger.hpp"

namespace other {

  constexpr inline std::array<hook_descriptor, 2> kVmHooks = {
    {
      { hook_id::PRE_LOAD, "pre_load" },
      { hook_id::POST_LOAD, "post_load" },
    },
  };

  namespace detail {

    consteval bool hooks_are_in_id_order() {
      for (size_t i = 0; i < kVmHooks.size(); ++i) {
        if (static_cast<size_t>(kVmHooks[i].id) != i) {
          return false;
        }
      }
      return true;
    }

  }  // namespace detail

  static_assert(detail::hooks_are_in_id_order(), "each row of kHooks must be at the index of its hook_id");

  const hook_table& vm_hooks() {
    return kVmHooks;
  }

  std::string_view hook_name(hook_id id) {
    OTHER_ASSERT(static_cast<size_t>(id) < kVmHooks.size(), "invalid hook id");
    return kVmHooks[static_cast<size_t>(id)].name;
  }

  opt<hook_id> get_hook_id(std::string_view name) {
    for (auto& h : kVmHooks) {
      if (h.name == name) {
        return h.id;
      }
    }
    return {};
  }

}  // namespace other