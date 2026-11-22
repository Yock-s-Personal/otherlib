/**
 * \file dotnet/binding_signature.hpp
 * what a native function handed to C# looks like, as text. the managed binder builds the
 * same text from the field the function is bound to, and binds only when the two agree
 **/
#ifndef OTHER_SCRIPTING_DOTNET_BINDING_SIGNATURE_HPP
#define OTHER_SCRIPTING_DOTNET_BINDING_SIGNATURE_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>

namespace other {
  namespace bindings {

    /// the scalar kinds that may cross, by size and class: nbool32 and natural_t are u32
    ///  and u64 here, and a string is a pointer to u8
    template <typename T>
    constexpr std::string_view scalar_code() {
      using U = std::remove_cv_t<T>;
      if constexpr (std::is_void_v<U>) {
        return "void";
      } else if constexpr (std::is_same_v<U, bool>) {
        return "bool";
      } else if constexpr (std::is_same_v<U, char> || std::is_same_v<U, uint8_t>) {
        return "u8";
      } else if constexpr (std::is_same_v<U, int8_t>) {
        return "i8";
      } else if constexpr (std::is_same_v<U, int16_t>) {
        return "i16";
      } else if constexpr (std::is_same_v<U, uint16_t>) {
        return "u16";
      } else if constexpr (std::is_same_v<U, int32_t>) {
        return "i32";
      } else if constexpr (std::is_same_v<U, uint32_t>) {
        return "u32";
      } else if constexpr (std::is_same_v<U, int64_t>) {
        return "i64";
      } else if constexpr (std::is_same_v<U, uint64_t>) {
        return "u64";
      } else if constexpr (std::is_same_v<U, float>) {
        return "f32";
      } else if constexpr (std::is_same_v<U, double>) {
        return "f64";
      } else {
        static_assert(false, "this type cannot cross to C# by value: pass scalars, or a pointer");
        return "";
      }
    }

    template <typename T>
    void append_code(std::string& out) {
      static_assert(!std::is_reference_v<T>, "a reference cannot cross to C#: take a pointer");
      if constexpr (std::is_pointer_v<T>) {
        using pointee = std::remove_cv_t<std::remove_pointer_t<T>>;
        if constexpr (std::is_void_v<pointee> || std::is_class_v<pointee>) {
          /// a handle: the other side cannot look inside it
          out += "ptr";
        } else {
          out += "ptr<";
          append_code<pointee>(out);
          out += ">";
        }
      } else {
        out += scalar_code<T>();
      }
    }

    /// "(ptr<u8>,i32)->u32"
    template <typename R, typename... Args>
    std::string signature_of(R (*)(Args...)) {
      std::string out = "(";
      [[maybe_unused]] size_t index = 0;
      ((out += (index++ == 0 ? "" : ","), append_code<Args>(out)), ...);
      out += ")->";
      append_code<R>(out);
      return out;
    }

  }  // namespace bindings
}  // namespace other

#endif  // OTHER_SCRIPTING_DOTNET_BINDING_SIGNATURE_HPP
