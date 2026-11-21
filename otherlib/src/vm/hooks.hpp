/**
 * \file vm/hooks.hpp
 **/
#ifndef OTHERLIB_VM_HOOKS_HPP
#define OTHERLIB_VM_HOOKS_HPP

#include <array>
#include <string_view>

#include "core/defines.hpp"

namespace other {

  enum class hook_id : uint8_t {
    PRE_LOAD,
    POST_LOAD,

    NUM_HOOKS,
  };

  struct hook_descriptor {
    hook_id id;
    /// a view, not a string: MSVC cannot build a constexpr table of std::string in Debug
    std::string_view name;
  };

  using hook_table = std::array<hook_descriptor, static_cast<uint8_t>(hook_id::NUM_HOOKS)>;

  const hook_table& vm_hooks();

  std::string_view hook_name(hook_id id);
  opt<hook_id> get_hook_id(std::string_view name);

}  // namespace other

#endif  // OTHERLIB_VM_HOOKS_HPP