/**
 * \file render/pipeline_declarations.hpp
 * a rendering pipeline declared in the command language, made into what the renderer takes
 **/
#ifndef OTHERLIB_RENDER_PIPELINE_DECLARATIONS_HPP
#define OTHERLIB_RENDER_PIPELINE_DECLARATIONS_HPP

#include "renderer/pipeline_definition.hpp"

#include "vm/command_files/asset_footprint.hpp"

#include "asset/asset.hpp"

namespace other {

  /// the pipeline a footprint holds, as the renderer takes one: the definition the TOML reader
  ///  fills. empty, with why in the log, when the footprint is not a pipeline the renderer can
  ///  take. source is the file it was declared in, which its shaders are found against
  opt<pipeline_definition> pipeline_from_footprint(const footprint_node& footprint, const filepath& source);

  /// the pipeline a file of declarations declares, compiled as it is read: held to its
  ///  structs, written as its asset block, assembled, and decoded from its footprint, the
  ///  bytes a linked program would carry. the file's own first pipeline is the one
  opt<pipeline_definition> read_pipeline_declarations(const filepath& path);

  /// the assets a file of declarations declares itself, not those of the files it uses, as
  ///  the assembler read them back; empty when the file has errors, which are in the log
  opt<ostd::vector<asset_node>> read_declared_assets(const filepath& path);

  /// the kind and the name of the first asset a file of declarations declares itself; empty
  ///  when it cannot be read or declares none
  opt<asset::declared> describe_declarations(const filepath& path);

  /// the renderer and the asset handler are below the command language and cannot read a
  ///  file of declarations; this tells them how, for a pipeline and for a material. whatever
  ///  starts the engine calls it, and calling it again changes nothing
  void register_declaration_readers();

}  // namespace other

#endif  // OTHERLIB_RENDER_PIPELINE_DECLARATIONS_HPP
