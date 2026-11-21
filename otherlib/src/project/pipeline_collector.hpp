/**
 * \file project/pipeline_collector.hpp
 * what a project's rendering pipelines are, which files each one needs, and the order they
 * have to be loaded in. a package is built from the answer; the engine itself still loads
 * pipelines from the driver's config
 **/
#ifndef OTHERLIB_PROJECT_PIPELINE_COLLECTOR_HPP
#define OTHERLIB_PROJECT_PIPELINE_COLLECTOR_HPP

#include <span>
#include <string>
#include <string_view>

#include "core/defines.hpp"
#include "data-structures/std_container.hpp"

namespace other {

  /// what a project draws with when its file lists no pipeline
  constexpr inline std::string_view kDefaultProjectPipeline = "engine://resources/editor-assets/default-pipeline.toml";

  /// the [rendering] table of a project file
  struct project_rendering_settings {
    filepath project_file;
    /// as written: asset paths, each found the way any asset path is. the engine default when
    ///  the project lists none
    ostd::vector<std::string> pipelines;
    /// the pipeline whose display texture reaches the window; empty when the project does not say
    opt<std::string> main_pipeline;
  };

  struct collected_pipeline {
    std::string name;
    filepath source;
    /// shader stages, everything they include, and seed textures: absolute, sorted, each once
    ostd::vector<filepath> files;
    /// names of the pipelines this one needs loaded first. empty until pipelines can use each other
    ostd::vector<std::string> depends_on;
  };

  struct pipeline_collection {
    bool success = false;
    /// in load order: a pipeline comes after everything it depends on
    ostd::vector<collected_pipeline> pipelines;
    ostd::vector<std::string> errors;
  };

  /// empty when the file cannot be read or is not a project file. a project with no
  ///  [rendering] table is fine and gets the engine default
  opt<project_rendering_settings> read_project_rendering_settings(const filepath& project_file);

  /// the pipeline files the settings name, in the order written. standard path handling: tags
  ///  are replaced, a mount is followed when one is named, and otherwise the likely places are
  ///  tried in turn, the project's own folder first. one that is not found is an error and is left out
  ostd::vector<filepath> project_pipeline_files(const project_rendering_settings& settings, ostd::vector<std::string>& errors);

  /// reads each pipeline file and finds what it needs. a file that is not a pipeline, a file
  ///  a pipeline needs that is missing, two pipelines of one name and a dependency circle are
  ///  all errors; with any error nothing is returned but the errors
  pipeline_collection collect_pipelines(std::span<const filepath> pipeline_files);

  /// puts dependencies first and otherwise keeps the order given. false, with the chain in
  ///  errors, when a pipeline needs one that is not in the list or they need each other in a circle
  bool order_pipelines_by_dependency(ostd::vector<collected_pipeline>& pipelines, ostd::vector<std::string>& errors);

}  // namespace other

#endif  // OTHERLIB_PROJECT_PIPELINE_COLLECTOR_HPP
