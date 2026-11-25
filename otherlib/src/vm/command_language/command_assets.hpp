/**
 * \file vm/command_language/command_assets.hpp
 **/
#ifndef OTHERLIB_VM_COMMAND_LANGUAGE_COMMAND_ASSETS_HPP
#define OTHERLIB_VM_COMMAND_LANGUAGE_COMMAND_ASSETS_HPP

#include "vm/command_files/asset_block.hpp"
#include "vm/command_language/command_declarations.hpp"

namespace other {

  /// makes of a declaration the asset block that says how the asset is built: every field of
  ///  its struct in the struct's own order, with what the declaration did not say written out
  ///  as the struct's default, so the block is the whole asset and no struct is needed to read
  ///  it. declarations has to have checked the build and found nothing wrong
  class command_asset_lowering {
   public:
    explicit command_asset_lowering(const command_declaration_checker& declarations)
        : declarations(declarations) {}

    /// name is what the asset is called in the build; see the three below
    asset_node lower(const command_declaration& declaration, std::string_view name) const;

    /// what an asset declared outside everything is called: its own name, or for one with none,
    ///  which is one in a file given to a variable, the file's name without its ending
    static std::string declared_name(const command_declaration& declaration, std::string_view unit_name);
    /// 'let x = kind name { }' is called name, and 'let x = kind { }' is called x
    static std::string bound_name(const command_stmt& let);

   private:
    asset_node lower(const command_declaration& declaration, const command_declaration_checker::scope* outer, std::string_view name) const;

    const command_declaration_checker& declarations;
  };

}  // namespace other

#endif  // OTHERLIB_VM_COMMAND_LANGUAGE_COMMAND_ASSETS_HPP
