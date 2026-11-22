/**
 * \file tests/project/project_file_changes_tests.cpp
 *
 * what differs between the project file a project was loaded from and the one on disk:
 * what an open project takes as it is, and what waits until the project is opened again
 **/
#include "project/project_file_changes.hpp"

#include "other_test.hpp"

namespace other {
  namespace {

    constexpr std::string_view kLoaded =
      "# a game\n"
      "[project]\n"
      "metadata = [ { key = \"name\", value = \"Sandbox Game\" }, { key = \"version\", value = \"0.1.0\" } ]\n"
      "plugins = [ { name = \"Systems\", path = \"build/game/systems.dll\" } ]\n"
      "\n"
      "[scripting]\n"
      "cs_project = \"game/Game.csproj\"\n"
      "\n"
      "[scene-graph]\n"
      "starting-scene = \"Main\"\n"
      "scenes = [\n"
      "  { name = \"Main\", path = \"game/assets/scenes/main.oscn\" },\n"
      "  { name = \"Second\", path = \"game/assets/scenes/second.oscn\" },\n"
      "]\n";

    std::string with(const std::string_view old_part, const std::string_view new_part) {
      std::string text{ kLoaded };
      const size_t at = text.find(old_part);
      EXPECT_NE(at, std::string::npos) << old_part;
      text.replace(at, old_part.size(), new_part);
      return text;
    }

  }  // namespace

  class project_file_changes_tests : public other_test {};

  TEST_F(project_file_changes_tests, the_same_project_written_another_way_is_no_change) {
    EXPECT_FALSE(compare_project_files(kLoaded, kLoaded).any());
    /// comments, blank lines and spacing are not what a project is loaded from
    const std::string reworded = with("# a game\n", "# the same game, with a note\n\n");
    EXPECT_FALSE(compare_project_files(kLoaded, with("starting-scene = \"Main\"", "starting-scene   =   \"Main\"   # where it starts")).any());
    EXPECT_FALSE(compare_project_files(kLoaded, reworded).any());
  }

  TEST_F(project_file_changes_tests, the_starting_scene_and_the_metadata_can_be_taken_by_an_open_project) {
    const project_file_changes moved = compare_project_files(kLoaded, with("starting-scene = \"Main\"", "starting-scene = \"Second\""));
    ASSERT_TRUE(moved.starting_scene.has_value());
    EXPECT_EQ(*moved.starting_scene, "Second");
    EXPECT_FALSE(moved.metadata_changed);
    EXPECT_TRUE(moved.needs_reopen.empty());
    EXPECT_FALSE(moved.error.has_value());

    const project_file_changes renamed = compare_project_files(kLoaded, with("Sandbox Game", "Another Name"));
    EXPECT_TRUE(renamed.metadata_changed);
    EXPECT_FALSE(renamed.starting_scene.has_value());
    EXPECT_TRUE(renamed.needs_reopen.empty());

    /// a starting scene taken away is a change to none
    const project_file_changes none = compare_project_files(kLoaded, with("starting-scene = \"Main\"\n", ""));
    ASSERT_TRUE(none.starting_scene.has_value());
    EXPECT_TRUE(none.starting_scene->empty());
  }

  TEST_F(project_file_changes_tests, what_is_only_read_when_a_project_is_opened_is_named) {
    const project_file_changes scenes = compare_project_files(kLoaded, with("  { name = \"Second\", path = \"game/assets/scenes/second.oscn\" },\n", ""));
    EXPECT_EQ(scenes.needs_reopen, (ostd::vector<std::string>{ "the scene list" }));

    std::string several = with("game/Game.csproj", "game/Other.csproj");
    several.replace(several.find("build/game/systems.dll"), std::string_view{ "build/game/systems.dll" }.size(), "build/game/other.dll");
    several += "graph = [ { name = \"Main\", incoming = [], outgoing = [ \"Second\" ] } ]\n";
    several += "\n[filesystem]\nworking-directory = \"game\"\n";
    const project_file_changes all = compare_project_files(kLoaded, several);
    EXPECT_EQ(all.needs_reopen, (ostd::vector<std::string>{ "the scene graph", "the scripting table", "the plugins", "the filesystem table" }));
    EXPECT_FALSE(all.metadata_changed);
    EXPECT_FALSE(all.starting_scene.has_value());
  }

  TEST_F(project_file_changes_tests, what_only_a_compile_reads_is_not_a_change_to_an_open_project) {
    std::string text = with("[project]\n", "[project]\npublic-name = \"Renamed\"\n");
    text += "\n[rendering]\nmain-pipeline = \"mine\"\n\n[package]\nkeep = [ \"assets/audio/**\" ]\n\n[vm]\nboot-file = \"boot.oasm\"\n";
    EXPECT_FALSE(compare_project_files(kLoaded, text).any());
  }

  TEST_F(project_file_changes_tests, a_file_that_cannot_be_read_is_an_error_and_nothing_else) {
    const project_file_changes broken = compare_project_files(kLoaded, "[project\nthis is not toml");
    ASSERT_TRUE(broken.error.has_value());
    EXPECT_TRUE(broken.error->starts_with("the project file on disk cannot be read: "));
    EXPECT_FALSE(broken.metadata_changed);
    EXPECT_FALSE(broken.starting_scene.has_value());
    EXPECT_TRUE(broken.needs_reopen.empty());
    EXPECT_TRUE(broken.any());
  }

}  // namespace other
