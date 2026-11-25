/**
 * \file tests/renderer/pipeline_declarations_tests.cpp
 **/
#include <filesystem>
#include <fstream>

#include "render/pipeline_declarations.hpp"

#include "other_test.hpp"

namespace other {
  namespace {

    /// every field of a definition but the file it was read from, one to a line, so two
    ///  definitions are the same when their lines are and a difference names itself
    ostd::vector<std::string> lines_of(const pipeline_definition& def) {
      ostd::vector<std::string> lines;
      const auto add = [&lines]<typename... Args>(std::format_string<Args...> format, Args&&... args) -> void {
        lines.push_back(std::format(format, std::forward<Args>(args)...));
      };
      const auto number = [](const auto value) -> int64_t {
        return static_cast<int64_t>(value);
      };

      add("name {}", def.name);
      add("version {}", def.version);
      add("display {}", def.display_texture_name);
      for (const pipeline_buffer_definition& buffer : def.buffers) {
        add("buffer {}: type {} usage {} tag {}", buffer.name, number(buffer.type), number(buffer.usage), buffer.tag.value());
      }
      for (const pipeline_texture_definition& texture : def.textures) {
        add("texture {}: window {} size {}x{} depth {} type {} format {} tag {} mips {} generate {} seed '{}' filters {} wraps {}", texture.name, texture.use_window_size, texture.fixed_size.x,
            texture.fixed_size.y, texture.depth, number(texture.type), number(texture.format), texture.tag.value(), texture.mip_levels, texture.generate_mips,
            texture.seed_texture_path.value_or("<none>"), texture.filters.has_value(), texture.wraps.has_value());
      }
      for (const pipeline_shader_definition& shader : def.shaders) {
        add("shader {}: vertex '{}' fragment '{}' geometry '{}' compute '{}' defines {}", shader.name, shader.vertex_path, shader.fragment_path, shader.geometry_path.value_or("<none>"),
            shader.compute_path.value_or("<none>"), shader.defines.size());
      }

      add("materials {}", def.materials.has_value());
      if (def.materials.has_value()) {
        const material_layout& layout = *def.materials;
        add("layout: element {} capacity {} base colour at {}", layout.element_size, layout.instance_capacity, layout.base_color_offset.has_value() ? number(*layout.base_color_offset) : -1);
        for (const material_layout::param& param : layout.params) {
          add("param {}: hash {} kind {} offset {} default kind {} ({}, {}, {}, {})", param.name, param.name_hash, number(param.kind), param.offset, number(param.default_value.value_kind),
              param.default_value.data.x, param.default_value.data.y, param.default_value.data.z, param.default_value.data.w);
        }
        for (const material_layout::texture_slot& slot : layout.texture_slots) {
          add("slot {}: hash {} uniform {} unit {}", slot.name, slot.name_hash, slot.uniform, slot.unit);
        }
      }
      for (const resource_tag& tag : def.required_tags) {
        add("required tag {}", tag.value());
      }

      for (const pipeline_pass_definition& pass : def.passes) {
        add("pass {}: type {} shader {} window {} size {}x{} framebuffer {} samples {} iterations {} draws {} clear flags {} override {}", pass.name, number(pass.pass_type), pass.shader_name,
            pass.use_window_size, pass.fixed_size.x, pass.fixed_size.y, pass.create_framebuffer, pass.samples, pass.iterations_per_frame,
            pass.expected_max_draws.has_value() ? number(*pass.expected_max_draws) : -1, number(pass.clear_flags), pass.override_fb_clear);
        if (pass.clear_color.has_value()) {
          add("  {} clear colour ({}, {}, {}, {})", pass.name, pass.clear_color->r, pass.clear_color->g, pass.clear_color->b, pass.clear_color->a);
        }
        const auto add_reference = [&](const std::string_view how, const pipeline_resource_reference& reference) -> void {
          add("  {} {} {}: type {} uniform '{}' binding {} attachment {} mip {} access {}", pass.name, how, reference.resource_name, number(reference.type), reference.uniform_name, reference.binding,
              number(reference.attachment), reference.mip_level, number(reference.access));
        };
        for (const pipeline_resource_reference& input : pass.inputs) {
          add_reference("reads", input);
        }
        for (const pipeline_resource_reference& output : pass.outputs) {
          add_reference("writes", output);
        }
        add("  {} runs {}", pass.name, pass.executor.name);
        for (const auto& [name, param] : pass.executor.params) {
          add("  {} with {}: type {} = {}", pass.name, name, number(param.type()), param.to_string());
        }
        for (const std::string& before : pass.depends_on) {
          add("  {} after {}", pass.name, before);
        }
        for (const frame_binding_definition& binding : pass.bindings) {
          add("  {} binds {}: tag {} scope {} type {} at {} set {} array {} element {}", pass.name, binding.name, binding.tag.value(), number(binding.scope), number(binding.type), binding.binding,
              binding.set, binding.array_size, binding.element_size);
        }
        for (const auto& [key, uniform] : pass.uniforms) {
          add("  {} sets {} ({}) of {}: type {} = {}", pass.name, uniform.name, key, uniform.pass_name, number(uniform.val.type()), uniform.val.to_string());
        }
      }
      return lines;
    }

  }  // namespace

  class pipeline_declarations_tests : public other_test {
   protected:
    void SetUp() override {
      other_test::SetUp();
      register_declaration_readers();
      const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
      sandbox = std::filesystem::temp_directory_path() / "other-pipeline-declaration-tests" / info->name();
      std::filesystem::remove_all(sandbox);
      std::filesystem::create_directories(sandbox);
    }

    void TearDown() override {
      std::error_code ec;
      std::filesystem::remove_all(sandbox, ec);
      other_test::TearDown();
    }

    filepath write_file(const std::string_view name, const std::string_view text) const {
      const filepath path = sandbox / name;
      std::ofstream file(path, std::ios::binary | std::ios::trunc);
      file << text;
      return path;
    }

    /// the default pipeline's declaration with one piece of its text changed, as a file
    filepath changed(const std::string_view name, const std::string_view from, const std::string_view to) const {
      std::ifstream file("resources/editor-assets/default-pipeline.oass", std::ios::binary);
      std::string text{ std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };
      const size_t at = text.find(from);
      EXPECT_NE(at, std::string::npos) << from;
      if (at != std::string::npos) {
        text.replace(at, from.size(), to);
      }
      return write_file(name, text);
    }

    filepath sandbox;
  };

  TEST_F(pipeline_declarations_tests, the_default_pipeline_declared_is_the_default_pipeline_of_the_toml) {
    const pipeline_definition from_toml = read_pipeline_definition_from_file("resources/editor-assets/default-pipeline.toml");
    /// through the renderer's own reader, as the asset handler reads it
    const pipeline_definition declared = read_pipeline_definition_from_file("resources/editor-assets/default-pipeline.oass");
    ASSERT_EQ(from_toml.name, "default-instancing");
    ASSERT_FALSE(declared.name.empty());

    const ostd::vector<std::string> expected = lines_of(from_toml);
    const ostd::vector<std::string> found = lines_of(declared);
    for (size_t i = 0; i < std::min(expected.size(), found.size()); ++i) {
      EXPECT_EQ(found[i], expected[i]) << "line " << i;
    }
    EXPECT_EQ(found.size(), expected.size());
    /// enough was held for the comparison to mean something
    EXPECT_GT(expected.size(), 150);

    /// each is found against its own file, and both files are in one folder
    EXPECT_EQ(declared.source_path.filename(), "default-pipeline.oass");
    EXPECT_EQ(declared.source_path.parent_path(), from_toml.source_path.parent_path());
    EXPECT_TRUE(validate_pipeline_definition(declared).valid);
  }

  TEST_F(pipeline_declarations_tests, the_asset_handler_is_told_what_a_file_of_declarations_declares) {
    EXPECT_EQ(asset::get_type_from_extension(".oass"), asset::ASSET_DECLARATION);
    EXPECT_EQ(asset::get_type_from_declaration("resources/editor-assets/default-pipeline.oass"), asset::RENDERING_PIPELINE);
    EXPECT_EQ(asset::get_name_from_declaration("resources/editor-assets/default-pipeline.oass"), "default-instancing");
    /// a TOML declaration is told as it was
    EXPECT_EQ(asset::get_type_from_declaration("resources/editor-assets/default-pipeline.toml"), asset::RENDERING_PIPELINE);
    EXPECT_EQ(asset::get_name_from_declaration("resources/editor-assets/default-pipeline.toml"), "default-instancing");

    const opt<asset::declared> described = describe_declarations("resources/editor-assets/default-pipeline.oass");
    ASSERT_TRUE(described.has_value());
    EXPECT_EQ(described->kind, "pipeline");
    EXPECT_EQ(described->name, "default-instancing");

    /// a file that declares something the engine loads no other way yet, and one that declares nothing
    const filepath paint = write_file("paint.oass", "struct paint { int coats = 1 }\npaint white { }\n");
    EXPECT_EQ(asset::get_type_from_declaration(paint), asset::EMPTY);
    EXPECT_EQ(asset::get_type_from_declaration("resources/ocmd/kinds.oass"), asset::EMPTY);
    EXPECT_FALSE(read_pipeline_declarations(paint).has_value());
    const filepath broken = write_file("broken.oass", "pipeline {\n");
    EXPECT_EQ(asset::get_type_from_declaration(broken), asset::EMPTY);
  }

  TEST_F(pipeline_declarations_tests, a_pipeline_the_renderer_cannot_take_is_refused_whole) {
    /// each of these is a declaration the language accepts: its struct allows it
    EXPECT_FALSE(read_pipeline_declarations(changed("no-stage.oass", "shader voxel_clear_shader { compute = \"engine://resources/voxel-clear.comp\" }", "shader voxel_clear_shader { }")).has_value());
    EXPECT_FALSE(read_pipeline_declarations(changed("one-side.oass", "size = [160, 560]", "size = [160]")).has_value());
    EXPECT_FALSE(read_pipeline_declarations(changed("param.oass", "with voxel_dim 128", "with voxel_dim \"large\"")).has_value());
    EXPECT_FALSE(read_pipeline_declarations(changed("unknown-param.oass", "with voxel_dim 128", "with voxel_size 128")).has_value());
    EXPECT_FALSE(read_pipeline_declarations(changed("uniform.oass", "set OE_gamma 2.2", "set OE_gamma \"bright\"")).has_value());
    EXPECT_FALSE(read_pipeline_declarations(changed("default.oass", "param roughness      { type = float  default = 1.0 }", "param roughness      { type = float  default = [1.0, 2.0] }")).has_value());
    EXPECT_FALSE(read_pipeline_declarations(changed("clear-color.oass", "clear-color = [0.0, 0.0, 0.0, 0.0]", "clear-color = [0.0, 0.0]")).has_value());
    /// and through the renderer's reader a refusal is a definition with no name
    const pipeline_definition refused = read_pipeline_definition_from_file(changed("refused.oass", "with voxel_dim 128", "with voxel_size 128"));
    EXPECT_TRUE(refused.name.empty());
    EXPECT_TRUE(refused.source_path.empty());

    /// one that the language itself refuses never reaches the decoder
    EXPECT_FALSE(read_pipeline_declarations(changed("unread.oass", "read  camera_buffer    { at = 3 }", "read  camra_buffer    { at = 3 }")).has_value());

    /// a change the renderer can take is taken
    const opt<pipeline_definition> brighter = read_pipeline_declarations(changed("brighter.oass", "set OE_exposure 0.6", "set OE_exposure 0.75"));
    ASSERT_TRUE(brighter.has_value());
    const auto tonemap = std::ranges::find(brighter->passes, "tonemap-pass", &pipeline_pass_definition::name);
    ASSERT_NE(tonemap, brighter->passes.end());
    const auto exposure = std::ranges::find_if(tonemap->uniforms, [](const auto& uniform) { return uniform.second.name == "OE_exposure"; });
    ASSERT_NE(exposure, tonemap->uniforms.end());
    EXPECT_EQ(exposure->second.val.to_string(), value{ 0.75f }.to_string());
  }

  TEST_F(pipeline_declarations_tests, a_footprint_that_is_no_pipeline_is_no_pipeline) {
    footprint_node other_kind = { .kind = asset_hash("material"), .name = "gold" };
    EXPECT_FALSE(pipeline_from_footprint(other_kind, "gold.oass").has_value());
    footprint_node nameless = { .kind = asset_hash("pipeline") };
    EXPECT_FALSE(pipeline_from_footprint(nameless, "none.oass").has_value());
    /// a pipeline of no passes is none the renderer takes
    footprint_node empty = { .kind = asset_hash("pipeline"), .name = "empty" };
    EXPECT_FALSE(pipeline_from_footprint(empty, "empty.oass").has_value());
  }

  TEST_F(pipeline_declarations_tests, with_no_reader_a_file_of_declarations_is_refused_and_toml_is_read) {
    set_pipeline_declarations_reader(nullptr);
    asset::set_declaration_describer(nullptr);
    EXPECT_TRUE(read_pipeline_definition_from_file("resources/editor-assets/default-pipeline.oass").name.empty());
    EXPECT_EQ(asset::get_type_from_declaration("resources/editor-assets/default-pipeline.oass"), asset::EMPTY);
    EXPECT_EQ(read_pipeline_definition_from_file("resources/editor-assets/default-pipeline.toml").name, "default-instancing");
    register_declaration_readers();
    EXPECT_EQ(read_pipeline_definition_from_file("resources/editor-assets/default-pipeline.oass").name, "default-instancing");
  }

}  // namespace other
