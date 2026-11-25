/**
 * \file tests/vm/asset_block_tests.cpp
 **/
#include "vm/command_files/asset_block.hpp"
#include "vm/vm_tests.hpp"

namespace other {
  namespace {

    constexpr std::string_view kGold = R"(%gold : material
  set base_color, [1.0, 0.766, 0.336, 1.0]   ; what follows a semicolon is not read
  set roughness, 0.3
  set layers, -2
  set label, "a \"quoted\" text\non two lines"
  set lit, true
  set mode, opaque
  set none, []
  use albedo, "../textures/gold.png"
  use fallback, plain
  begin slot, base
    ref texture, noise
    begin texture, noise
      set size, [64, 64]
    end
  end
  begin note
  end
  edge base, other
end
)";

  }  // namespace

  class asset_block_tests : public other_test {
   protected:
    static ostd::vector<token> lex(const std::string_view text) {
      diagnostic_engine diag;
      return ocmd_lexer{ text }.tokenize(&diag);
    }

    /// the asset blocks of text; none when it cannot be read
    static ostd::vector<asset_node> read(const std::string_view text) {
      diagnostic_engine diag;
      const ocmd_ir ir = detail::parse_source(text, &diag);
      return ir.valid ? ir.assets : ostd::vector<asset_node>{};
    }

    static bool is_read(const std::string_view text) {
      diagnostic_engine diag;
      return detail::parse_source(text, &diag).valid;
    }
  };

  TEST_F(asset_block_tests, a_word_in_an_asset_block_is_a_word_whatever_it_spells) {
    /// outside the block 'add' and 'end' are keywords and 'face' is a number
    const ostd::vector<token> tokens = lex("%face : texture\n  set add, r1\n  begin pass, end\n  end\nend\nset r1, 5\n");
    ostd::vector<token_type> types;
    for (const token& found : tokens) {
      types.push_back(found.type);
    }
    // clang-format off
    const ostd::vector<token_type> expected = {
      TOKEN_TYPE_SOURCE_START,
      TOKEN_TYPE_PERCENT, TOKEN_TYPE_IDENTIFIER, TOKEN_TYPE_COLON, TOKEN_TYPE_IDENTIFIER, TOKEN_TYPE_SEMICOLON,
      TOKEN_TYPE_IDENTIFIER, TOKEN_TYPE_IDENTIFIER, TOKEN_TYPE_COMMA, TOKEN_TYPE_IDENTIFIER, TOKEN_TYPE_SEMICOLON,
      TOKEN_TYPE_IDENTIFIER, TOKEN_TYPE_IDENTIFIER, TOKEN_TYPE_COMMA, TOKEN_TYPE_IDENTIFIER, TOKEN_TYPE_SEMICOLON,
      TOKEN_TYPE_IDENTIFIER, TOKEN_TYPE_SEMICOLON,
      TOKEN_TYPE_IDENTIFIER, TOKEN_TYPE_SEMICOLON,
      /// the block is over at its own 'end', and what follows is assembly again
      TOKEN_TYPE_KW_SET, TOKEN_TYPE_KW_R1, TOKEN_TYPE_COMMA, TOKEN_TYPE_INTEGER_LITERAL,
      TOKEN_TYPE_EOF,
    };
    // clang-format on
    EXPECT_EQ(types, expected);
    EXPECT_EQ(tokens[2].text, "face");
    EXPECT_EQ(tokens[7].text, "add");
  }

  TEST_F(asset_block_tests, a_block_is_read_as_fields_parts_and_edges) {
    const ostd::vector<asset_node> assets = read(kGold);
    ASSERT_EQ(assets.size(), 1);
    const asset_node& gold = assets[0];
    EXPECT_EQ(gold.name, "gold");
    EXPECT_EQ(gold.kind, "material");

    ASSERT_EQ(gold.fields.size(), 9);
    const asset_value& colour = gold.fields[0].value;
    EXPECT_EQ(gold.fields[0].name, "base_color");
    EXPECT_EQ(gold.fields[0].how, ASSET_FIELD_SET);
    ASSERT_EQ(colour.kind, ASSET_VALUE_LIST);
    ASSERT_EQ(colour.items.size(), 4);
    EXPECT_EQ(colour.items[0].kind, ASSET_VALUE_DECIMAL);
    EXPECT_DOUBLE_EQ(colour.items[1].decimal, 0.766);
    EXPECT_DOUBLE_EQ(gold.fields[1].value.decimal, 0.3);
    EXPECT_EQ(gold.fields[2].value.kind, ASSET_VALUE_INTEGER);
    EXPECT_EQ(gold.fields[2].value.integer, -2);
    EXPECT_EQ(gold.fields[3].value.kind, ASSET_VALUE_TEXT);
    EXPECT_EQ(gold.fields[3].value.text, "a \"quoted\" text\non two lines");
    EXPECT_EQ(gold.fields[4].value.kind, ASSET_VALUE_BOOL);
    EXPECT_EQ(gold.fields[4].value.integer, 1);
    EXPECT_EQ(gold.fields[5].value.kind, ASSET_VALUE_WORD);
    EXPECT_EQ(gold.fields[5].value.text, "opaque");
    EXPECT_EQ(gold.fields[6].value.kind, ASSET_VALUE_LIST);
    EXPECT_TRUE(gold.fields[6].value.items.empty());
    /// another asset: by where its file is, or by its name in the build
    EXPECT_EQ(gold.fields[7].how, ASSET_FIELD_USE);
    EXPECT_EQ(gold.fields[7].value.kind, ASSET_VALUE_TEXT);
    EXPECT_EQ(gold.fields[7].value.text, "../textures/gold.png");
    EXPECT_EQ(gold.fields[8].how, ASSET_FIELD_USE);
    EXPECT_EQ(gold.fields[8].value.kind, ASSET_VALUE_WORD);

    ASSERT_EQ(gold.parts.size(), 2);
    const asset_node& slot = gold.parts[0];
    EXPECT_EQ(slot.kind, "slot");
    EXPECT_EQ(slot.name, "base");
    ASSERT_EQ(slot.fields.size(), 1);
    EXPECT_EQ(slot.fields[0].how, ASSET_FIELD_REF);
    EXPECT_EQ(slot.fields[0].value.text, "noise");
    ASSERT_EQ(slot.parts.size(), 1);
    EXPECT_EQ(slot.parts[0].name, "noise");
    EXPECT_EQ(slot.parts[0].fields[0].value.items.size(), 2);
    EXPECT_EQ(gold.parts[1].kind, "note");
    EXPECT_TRUE(gold.parts[1].name.empty());

    ASSERT_EQ(gold.edges.size(), 1);
    EXPECT_EQ(gold.edges[0].from, "base");
    EXPECT_EQ(gold.edges[0].to, "other");
  }

  TEST_F(asset_block_tests, what_is_written_is_read_back_as_the_same_asset) {
    const ostd::vector<asset_node> first = read(kGold);
    ASSERT_EQ(first.size(), 1);
    const std::string written = asset_block_text(first[0]);
    const ostd::vector<asset_node> second = read(written);
    ASSERT_EQ(second.size(), 1);
    EXPECT_EQ(first[0], second[0]);
    EXPECT_EQ(asset_block_text(second[0]), written);

    /// a number that is not whole stays one, however it is written; a text keeps every byte
    asset_node odd = { .kind = "thing", .name = "odd" };
    const double numbers[] = { 1.0, -0.0, 0.1, 1e20, 1e-9, 123456789.125, -2.5 };
    for (size_t i = 0; i < std::size(numbers); ++i) {
      odd.fields.push_back({ .name = std::format("n{}", i), .value = { .kind = ASSET_VALUE_DECIMAL, .decimal = numbers[i] } });
    }
    odd.fields.push_back({ .name = "big", .value = { .kind = ASSET_VALUE_INTEGER, .integer = std::numeric_limits<int64_t>::min() } });
    odd.fields.push_back({ .name = "text", .value = { .kind = ASSET_VALUE_TEXT, .text = std::string{ "tab\there \\ \"q\" \x01 \xC3\xA9 ; not a comment\r\n" } } });
    odd.fields.push_back({ .name = "nested", .value = { .kind = ASSET_VALUE_LIST, .items = { { .kind = ASSET_VALUE_LIST, .items = { { .kind = ASSET_VALUE_WORD, .text = "end" } } }, { .kind = ASSET_VALUE_BOOL, .integer = 0 } } } });
    const ostd::vector<asset_node> back = read(asset_block_text(odd));
    ASSERT_EQ(back.size(), 1);
    EXPECT_EQ(back[0], odd);
    EXPECT_TRUE(asset_block_text(odd).contains("set n0, 1.0\n"));
  }

  TEST_F(asset_block_tests, a_block_that_cannot_be_read_is_an_error) {
    EXPECT_TRUE(is_read("%a : b\n  set x, 1\nend\n"));
    EXPECT_FALSE(is_read("%a : b\n  set x, 1\n"));
    EXPECT_FALSE(is_read("%a : b\n  begin part, p\n  set x, 1\nend\n"));
    EXPECT_FALSE(is_read("%a\n  set x, 1\nend\n"));
    EXPECT_FALSE(is_read("%a : b\n  put x, 1\nend\n"));
    EXPECT_FALSE(is_read("%a : b\n  set x\nend\n"));
    EXPECT_FALSE(is_read("%a : b\n  set x, 1, 2\nend\n"));
    EXPECT_FALSE(is_read("%a : b\n  set x, 1\n  set x, 2\nend\n"));
    EXPECT_FALSE(is_read("%a : b\n  ref x, 3\nend\n"));
    EXPECT_FALSE(is_read("%a : b\n  ref x, \"path\"\nend\n"));
    EXPECT_FALSE(is_read("%a : b\n  set x, 12abc\nend\n"));
    EXPECT_FALSE(is_read("%a : b\n  set x, 99999999999999999999\nend\n"));
    EXPECT_FALSE(is_read("%a : b\n  set x, [1, 2\nend\n"));
    EXPECT_TRUE(lex("%a : b\n  set x, \"no end\nend\n").empty());
    EXPECT_TRUE(lex("%a : b\n  set x, \"bad \\q escape\"\nend\n").empty());
    EXPECT_TRUE(lex("%a : b\n  set x, {1}\nend\n").empty());
    EXPECT_FALSE(is_read("%a : b\n  edge one\nend\n"));
    EXPECT_FALSE(is_read("%a : b\n  set x, 1\nend\n%a : b\n  set x, 2\nend\n"));
    /// two kinds can share a name
    EXPECT_TRUE(is_read("%a : b\n  set x, 1\nend\n%a : c\n  set x, 2\nend\n"));
  }

  TEST_F(asset_block_tests, code_and_data_beside_a_block_are_assembled_as_before) {
    const std::string_view source = "/entry: main\n#data {\n  .count : = 17\n}\n%pipe : pipeline\n  set version, 1\nend\n$main:\n  set r1, 5\n  ret\nend\n%other : pipeline\n  set version, 2\nend\n";
    diagnostic_engine diag;
    const ocmd_ir ir = detail::parse_source(source, &diag);
    ASSERT_TRUE(ir.valid);
    EXPECT_EQ(ir.data_blocks.size(), 1);
    ASSERT_EQ(ir.assets.size(), 2);
    EXPECT_EQ(ir.assets[1].name, "other");
    ASSERT_TRUE(std::ranges::find(ir.code_blocks, std::string{ "main" }, &code_block::name) != ir.code_blocks.end());

    /// loose code with no 'end' of its own stops where a block begins
    const ocmd_ir loose = detail::parse_source("set r1, 5\n%pipe : pipeline\n  set version, 1\nend\n", &diag);
    ASSERT_TRUE(loose.valid);
    EXPECT_EQ(loose.assets.size(), 1);

    const ocmd_program program = detail::compile_source(source, &diag);
    ASSERT_TRUE(program.valid);
    ASSERT_EQ(program.assets.size(), 2);
    EXPECT_EQ(program.assets[0], ir.assets[0]);
  }

}  // namespace other
