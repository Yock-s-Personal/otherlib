/**
 * \file tests/renderer/material_declarations_tests.cpp
 **/
#include <filesystem>
#include <fstream>

#include "core/fnv.hpp"

#include "render/material_declarations.hpp"
#include "render/pipeline_declarations.hpp"

#include "other_test.hpp"

namespace other {
  namespace {

    /// everything a material is before it is registered, one thing to a line, so two are the
    ///  same material when their lines are and a difference names itself
    ostd::vector<std::string> lines_of(const material& mat) {
      ostd::vector<std::string> lines = { std::format("name {}", mat.name) };
      for (const auto& [hash, value] : mat.params) {
        const auto name = mat.param_names.find(hash);
        lines.push_back(std::format("param {} ({}): kind {} ({}, {}, {}, {})", name != mat.param_names.end() ? name->second : "<no name>", hash, static_cast<int>(value.value_kind), value.data.x,
                                    value.data.y, value.data.z, value.data.w));
      }
      for (const auto& [slot, path] : mat.texture_paths) {
        lines.push_back(std::format("texture {}: '{}'", slot, path));
      }
      lines.push_back(std::format("names {} hashes {}", mat.param_names.size(), mat.texture_hashes.size()));
      return lines;
    }

  }  // namespace

  class material_declarations_tests : public other_test {
   protected:
    void SetUp() override {
      other_test::SetUp();
      register_declaration_readers();
      const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
      sandbox = std::filesystem::temp_directory_path() / "other-material-declaration-tests" / info->name();
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

    /// the material a declaration makes; the test fails when it makes none
    material declared(const std::string_view name, const std::string_view text) const {
      const material_parse_result result = read_material_declarations(write_file(name, text));
      EXPECT_TRUE(result.success()) << result.error;
      EXPECT_TRUE(result.warnings.empty()) << (result.warnings.empty() ? "" : result.warnings.front());
      return result.success() ? *result.mat : material{};
    }

    filepath sandbox;
  };

  TEST_F(material_declarations_tests, each_of_test_projects_four_is_the_same_material_from_either_file) {
    const pipeline_definition pipeline = read_pipeline_definition_from_file("resources/editor-assets/default-pipeline.toml");
    ASSERT_TRUE(pipeline.materials.has_value());

    for (const std::string_view name : { "gold", "glass", "donut", "floor" }) {
      SCOPED_TRACE(name);
      const filepath folder = "test-project/assets/materials";
      const material_parse_result from_toml = read_material_file(folder / std::format("{}.omat", name));
      const material_parse_result from_declaration = read_material_file(folder / std::format("{}.oass", name));
      ASSERT_TRUE(from_toml.success()) << from_toml.error;
      ASSERT_TRUE(from_declaration.success()) << from_declaration.error;
      EXPECT_TRUE(from_declaration.warnings.empty());
      EXPECT_EQ(from_declaration.mat->name, name);
      EXPECT_EQ(lines_of(*from_declaration.mat), lines_of(*from_toml.mat));

      /// and so the bytes the shader is given for it are the same bytes
      ostd::vector<uint8_t> as_toml(pipeline.materials->element_size);
      ostd::vector<uint8_t> as_declared(pipeline.materials->element_size);
      size_t complaints = 0;
      const auto count = [&complaints](std::string_view, std::string_view) -> void {
        ++complaints;
      };
      pipeline.materials->pack(&*from_toml.mat, as_toml, count);
      pipeline.materials->pack(&*from_declaration.mat, as_declared, count);
      EXPECT_EQ(as_declared, as_toml);
      EXPECT_EQ(complaints, 0);
    }
  }

  TEST_F(material_declarations_tests, what_a_material_leaves_unsaid_is_not_in_it) {
    const material gold = declared("gold.oass", "material gold {\n  base_color = [1.0, 0.766, 0.336, 1.0]\n  roughness = 0.3\n  metalness = null\n}\n");
    EXPECT_EQ(gold.name, "gold");
    /// neither the one it never names nor the one it says null for: the layout decides both
    EXPECT_EQ(gold.params.size(), 2);
    EXPECT_TRUE(gold.params.contains(FNV("base_color")));
    EXPECT_TRUE(gold.params.contains(FNV("roughness")));
    EXPECT_FALSE(gold.params.contains(FNV("metalness")));
    EXPECT_FALSE(gold.params.contains(FNV("emissive_color")));
    EXPECT_TRUE(gold.texture_paths.empty());

    const material bare = declared("bare.oass", "material bare { }\n");
    EXPECT_TRUE(bare.params.empty());
  }

  TEST_F(material_declarations_tests, a_value_is_taken_as_the_toml_reader_takes_one) {
    const material mat = declared("hull.oass", R"(material hull-plating {
  base_color = [0.82, 0.85, 0.9, 1]
  emissive_color = [0.1, 0.2, 0.3]
  roughness = 1
  set sector 3
  set glow true
  set offset [0.5, 0.25]
  set shine 0.4
  map base_color "../textures/hull_albedo.png"
  map normal "../textures/hull_n.png"
}
)");
    EXPECT_EQ(mat.name, "hull-plating");
    ASSERT_EQ(mat.params.size(), 7);
    EXPECT_EQ(mat.params.at(FNV("base_color")).value_kind, material_value::kind::VEC4);
    EXPECT_FLOAT_EQ(mat.params.at(FNV("base_color")).data.x, 0.82f);
    EXPECT_FLOAT_EQ(mat.params.at(FNV("base_color")).data.w, 1.f);
    EXPECT_EQ(mat.params.at(FNV("emissive_color")).value_kind, material_value::kind::VEC3);
    /// the struct says roughness is a float, so a whole number given for it is one: in a
    ///  .omat 'roughness = 1' is an int, which the layout then refuses
    EXPECT_EQ(mat.params.at(FNV("roughness")).value_kind, material_value::kind::F32);
    EXPECT_FLOAT_EQ(mat.params.at(FNV("roughness")).data.x, 1.f);
    /// a parameter the struct has no field for is as it is written
    EXPECT_EQ(mat.params.at(FNV("sector")).value_kind, material_value::kind::I32);
    EXPECT_EQ(mat.params.at(FNV("glow")).value_kind, material_value::kind::B32);
    EXPECT_EQ(mat.params.at(FNV("offset")).value_kind, material_value::kind::VEC2);
    EXPECT_EQ(mat.params.at(FNV("shine")).value_kind, material_value::kind::F32);
    EXPECT_EQ(mat.param_names.at(FNV("shine")), "shine");

    ASSERT_EQ(mat.texture_paths.size(), 2);
    EXPECT_EQ(mat.texture_paths.at(FNV("base_color")), "../textures/hull_albedo.png");
    EXPECT_EQ(mat.texture_paths.at(FNV("normal")), "../textures/hull_n.png");
    EXPECT_TRUE(mat.texture_hashes.empty());
  }

  TEST_F(material_declarations_tests, a_file_that_is_no_material_is_an_error_and_a_value_of_no_shape_a_warning) {
    EXPECT_FALSE(read_material_declarations(write_file("broken.oass", "material {\n")).success());
    EXPECT_FALSE(read_material_declarations("resources/editor-assets/default-pipeline.oass").success());
    EXPECT_FALSE(read_material_declarations(sandbox / "not-there.oass").success());
    /// the struct holds what it can: a colour is numbers, and a slot's texture a path in quotes
    EXPECT_FALSE(read_material_declarations(write_file("words.oass", "material m { base_color = red }\n")).success());
    EXPECT_FALSE(read_material_declarations(write_file("slot.oass", "material m { map base_color floor }\n")).success());
    EXPECT_FALSE(read_material_declarations(write_file("field.oass", "material m { shininess = 0.4 }\n")).success());

    /// what the struct cannot hold to a shape is the reader's to refuse, one parameter at a time
    const material_parse_result partial = read_material_declarations(write_file("odd.oass", "material m {\n  roughness = 0.5\n  base_color = [1.0, 0.0, 0.0, 1.0, 0.5]\n  set words \"not a value\"\n}\n"));
    ASSERT_TRUE(partial.success()) << partial.error;
    EXPECT_EQ(partial.mat->params.size(), 1);
    ASSERT_EQ(partial.warnings.size(), 2);
    EXPECT_TRUE(partial.warnings[0].contains("base_color"));
    EXPECT_TRUE(partial.warnings[1].contains("words"));
  }

  TEST_F(material_declarations_tests, the_engine_reads_a_material_from_whichever_file_it_is) {
    const filepath toml = write_file("gold.omat", "name = \"gold\"\n\n[params]\nroughness = 0.3\n");
    const filepath declaration = write_file("gold.oass", "material gold { roughness = 0.3 }\n");
    const material_parse_result from_toml = read_material_file(toml);
    const material_parse_result from_declaration = read_material_file(declaration);
    ASSERT_TRUE(from_toml.success() && from_declaration.success());
    EXPECT_EQ(lines_of(*from_declaration.mat), lines_of(*from_toml.mat));

    /// and the asset handler is told it is a material
    EXPECT_EQ(asset::get_type_from_declaration(declaration), asset::MATERIAL);
    EXPECT_EQ(asset::get_name_from_declaration(declaration), "gold");
    EXPECT_EQ(asset::get_type_from_declaration("test-project/assets/materials/floor.oass"), asset::MATERIAL);

    set_material_declarations_reader(nullptr);
    const material_parse_result unread = read_material_file(declaration);
    EXPECT_FALSE(unread.success());
    EXPECT_FALSE(unread.error.empty());
    EXPECT_TRUE(read_material_file(toml).success());
    register_declaration_readers();
    EXPECT_TRUE(read_material_file(declaration).success());
  }

  TEST_F(material_declarations_tests, a_footprint_that_is_no_material_is_no_material) {
    const footprint_node pipeline = { .kind = asset_hash("pipeline"), .name = "default" };
    EXPECT_FALSE(material_from_footprint(pipeline, "p.oass").success());
    /// a material with no name is called after its file, as a .omat with none is
    const footprint_node nameless = { .kind = asset_hash("material") };
    const material_parse_result named = material_from_footprint(nameless, "assets/chalk.oass");
    ASSERT_TRUE(named.success());
    EXPECT_EQ(named.mat->name, "chalk");
    /// a field the engine has no name for is passed over, and said
    footprint_node odd = { .kind = asset_hash("material"), .name = "odd" };
    odd.fields.push_back({ .name = asset_hash("shininess"), .value = { .kind = ASSET_VALUE_DECIMAL, .decimal = 0.4 } });
    const material_parse_result passed_over = material_from_footprint(odd, "odd.oass");
    ASSERT_TRUE(passed_over.success());
    EXPECT_TRUE(passed_over.mat->params.empty());
    EXPECT_EQ(passed_over.warnings.size(), 1);
  }

}  // namespace other
