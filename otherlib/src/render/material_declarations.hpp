/**
 * \file render/material_declarations.hpp
 * a material declared in the command language, made into what the engine holds
 **/
#ifndef OTHERLIB_RENDER_MATERIAL_DECLARATIONS_HPP
#define OTHERLIB_RENDER_MATERIAL_DECLARATIONS_HPP

#include "gpu_resource/material.hpp"

#include "vm/command_files/asset_footprint.hpp"

namespace other {

  /// the material a footprint holds, as the engine holds one: the values the TOML reader
  ///  fills, and no more. a parameter the declaration leaves unsaid is not in it, so the
  ///  pipeline's material layout decides it, as for a .omat. a texture's path is as written.
  ///  source names the file in what is said about it
  material_parse_result material_from_footprint(const footprint_node& footprint, const filepath& source);

  /// the material a file of declarations declares, compiled as it is read; the file's own
  ///  first material is the one
  material_parse_result read_material_declarations(const filepath& path);

}  // namespace other

#endif  // OTHERLIB_RENDER_MATERIAL_DECLARATIONS_HPP
