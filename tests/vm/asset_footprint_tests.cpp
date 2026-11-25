/**
 * \file tests/vm/asset_footprint_tests.cpp
 **/
#include "vm/command_files/asset_footprint.hpp"
#include "vm/vm_tests.hpp"

namespace other {
  namespace {

    constexpr std::string_view kRoom = R"(%hall : room
  set height, 2.5
  set doors, 2
  set label, "the hall"
  set lit, true
  set shade, dark
  set mix, [1, 2.0, "three", four, [5]]
  ref finish, white
  use floor, "floors/oak.things"
  begin wall, north
    set height, 3.0
  end
  begin wall, south
  end
  begin paint, white
    set coats, 2
  end
  edge north, south
  edge south, east
end
)";

  }  // namespace

  class asset_footprint_tests : public other_test {
   protected:
    /// the one asset block of text
    static asset_node asset(const std::string_view text) {
      diagnostic_engine diag;
      const ocmd_ir ir = detail::parse_source(text, &diag);
      EXPECT_TRUE(ir.valid && ir.assets.size() == 1) << text;
      return ir.valid && ir.assets.size() == 1 ? ir.assets[0] : asset_node{};
    }

    static ostd::vector<uint8_t> footprint(const std::string_view text) {
      return asset_footprint(asset(text));
    }

    static void put_u64(ostd::vector<uint8_t>& out, const uint64_t value) {
      for (size_t i = 0; i < sizeof(value); ++i) {
        out.push_back(static_cast<uint8_t>(value >> (i * 8)));
      }
    }
  };

  TEST_F(asset_footprint_tests, the_hash_is_fnv_1a_as_it_is_published) {
    EXPECT_EQ(asset_hash("foobar"), 0x85944171F73967E8);
    /// a byte above 127 is not read as a number below zero
    EXPECT_EQ(asset_hash("\xff"), (0xCBF29CE484222325 ^ 0xFF) * 0x100000001B3);
    const uint8_t bytes[] = { 'f', 'o', 'o', 'b', 'a', 'r' };
    EXPECT_EQ(asset_hash(std::span<const uint8_t>{ bytes }), asset_hash("foobar"));
  }

  TEST_F(asset_footprint_tests, a_footprint_is_these_bytes) {
    /// the kind's hash, the name, how many fields, parts and edges; then each field as the hash
    ///  of its name, how it is given, the type of its value and the value. low byte first
    ostd::vector<uint8_t> expected;
    put_u64(expected, asset_hash("b"));
    expected.append_range(std::initializer_list<uint8_t>{ 1, 0, 0, 0, 'a' });
    expected.append_range(std::initializer_list<uint8_t>{ 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 });
    put_u64(expected, asset_hash("w"));
    expected.append_range(std::initializer_list<uint8_t>{ 1, 4, 2, 0, 0, 0, 'h', 'i' });
    put_u64(expected, asset_hash("x"));
    expected.append_range(std::initializer_list<uint8_t>{ 1, 1, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF });
    EXPECT_EQ(footprint("%a : b\n  set x, -2\n  set w, \"hi\"\nend\n"), expected);
  }

  TEST_F(asset_footprint_tests, the_order_fields_and_edges_were_written_in_is_not_in_it) {
    const ostd::vector<uint8_t> first = footprint(kRoom);
    const ostd::vector<uint8_t> shuffled = footprint(R"(%hall : room
  edge south, east
  use floor, "floors/oak.things"
  set mix, [1, 2.0, "three", four, [5]]
  begin wall, north
    set height, 3.0
  end
  set shade, dark
  set lit, true
  edge north, south
  begin wall, south
  end
  ref finish, white
  set label, "the hall"
  edge south, east
  set doors, 2
  begin paint, white
    set coats, 2
  end
  set height, 2.5
end
)");
    EXPECT_EQ(first, shuffled);
    EXPECT_EQ(asset_hash(std::span<const uint8_t>{ first }), asset_hash(std::span<const uint8_t>{ shuffled }));
  }

  TEST_F(asset_footprint_tests, anything_the_asset_is_changes_it) {
    const std::string room{ kRoom };
    const ostd::vector<uint8_t> first = footprint(room);
    const auto with = [&room](const std::string_view from, const std::string_view to) -> ostd::vector<uint8_t> {
      std::string changed = room;
      const size_t at = changed.find(from);
      EXPECT_NE(at, std::string::npos) << from;
      changed.replace(at, from.size(), to);
      return footprint(changed);
    };
    EXPECT_NE(with("set doors, 2", "set doors, 3"), first);
    /// a whole number and a number with a point are two values, and so are a text and a word
    EXPECT_NE(with("set doors, 2", "set doors, 2.0"), first);
    EXPECT_NE(with("set shade, dark", "set shade, \"dark\""), first);
    EXPECT_NE(with("ref finish, white", "use finish, white"), first);
    EXPECT_NE(with("set lit, true", "set lit, false"), first);
    EXPECT_NE(with("[1, 2.0, \"three\", four, [5]]", "[1, 2.0, \"three\", [5], four]"), first);
    EXPECT_NE(with("%hall : room", "%hall : shed"), first);
    EXPECT_NE(with("%hall : room", "%hull : room"), first);
    EXPECT_NE(with("begin wall, south", "begin wall, west"), first);
    EXPECT_NE(with("edge north, south", "edge south, north"), first);
    EXPECT_NE(with("set height, 3.0", "set height, 3.5"), first);
    /// the order of the parts is the asset's: a kind may lay its parts out by it
    EXPECT_NE(with("  begin wall, south\n  end\n", ""), first);
    EXPECT_NE(footprint("%a : b\n  begin p, one\n  end\n  begin p, two\n  end\nend\n"), footprint("%a : b\n  begin p, two\n  end\n  begin p, one\n  end\nend\n"));
    /// zero is zero however its sign bit is set
    EXPECT_EQ(footprint("%a : b\n  set x, -0.0\nend\n"), footprint("%a : b\n  set x, 0.0\nend\n"));
  }

  TEST_F(asset_footprint_tests, a_footprint_is_read_back_with_hashes_for_names) {
    const ostd::vector<uint8_t> bytes = footprint(kRoom);
    const opt<footprint_node> read = read_asset_footprint(bytes);
    ASSERT_TRUE(read.has_value());
    EXPECT_EQ(read->kind, asset_hash("room"));
    EXPECT_EQ(read->name, "hall");

    /// in the order of their names
    ASSERT_EQ(read->fields.size(), 8);
    const std::string_view names[] = { "doors", "finish", "floor", "height", "label", "lit", "mix", "shade" };
    for (size_t i = 0; i < std::size(names); ++i) {
      EXPECT_EQ(read->fields[i].name, asset_hash(names[i])) << names[i];
    }
    const asset_node written = asset(kRoom);
    for (const asset_field& field : written.fields) {
      const footprint_field* found = read->field(asset_hash(field.name));
      ASSERT_TRUE(found != nullptr) << field.name;
      EXPECT_EQ(found->how, field.how) << field.name;
      EXPECT_EQ(found->value, field.value) << field.name;
    }
    EXPECT_EQ(read->field(asset_hash("nothing")), nullptr);

    ASSERT_EQ(read->parts.size(), 3);
    EXPECT_EQ(read->parts[0].kind, asset_hash("wall"));
    EXPECT_EQ(read->parts[0].name, "north");
    EXPECT_DOUBLE_EQ(read->parts[0].field(asset_hash("height"))->value.decimal, 3.0);
    EXPECT_EQ(read->parts[2].kind, asset_hash("paint"));
    /// by name, each once
    EXPECT_EQ(read->edges, (ostd::vector<asset_edge>{ { .from = "north", .to = "south" }, { .from = "south", .to = "east" } }));
  }

  TEST_F(asset_footprint_tests, bytes_that_are_not_a_whole_footprint_are_refused) {
    const ostd::vector<uint8_t> bytes = footprint(kRoom);
    for (size_t length = 0; length < bytes.size(); ++length) {
      ASSERT_FALSE(read_asset_footprint(std::span{ bytes }.first(length)).has_value()) << length;
    }
    ostd::vector<uint8_t> longer = bytes;
    longer.push_back(0);
    EXPECT_FALSE(read_asset_footprint(longer).has_value());

    /// a count that says more than there is, and a type that is none
    ostd::vector<uint8_t> lying = footprint("%a : b\n  set x, [1, 2]\nend\n");
    ostd::vector<uint8_t> untyped = lying;
    const size_t value_at = sizeof(uint64_t) + 5 + 12 + sizeof(uint64_t) + 1;
    ASSERT_EQ(lying[value_at], 6);
    lying[value_at + 1] = 0xFF;
    lying[value_at + 4] = 0xFF;
    EXPECT_FALSE(read_asset_footprint(lying).has_value());
    untyped[value_at] = 99;
    EXPECT_FALSE(read_asset_footprint(untyped).has_value());

    /// lists nested without end are not followed without end
    std::string deep = "%a : b\n  set x, ";
    deep += std::string(200, '[') + std::string(200, ']') + "\nend\n";
    EXPECT_FALSE(read_asset_footprint(footprint(deep)).has_value());
  }

  TEST_F(asset_footprint_tests, a_section_holds_each_asset_as_it_is_alone) {
    const asset_node hall = asset(kRoom);
    const asset_node gold = asset("%gold : material\n  set roughness, 0.3\nend\n");
    const asset_node shed = asset("%shed : room\n  set doors, 1\nend\n");
    const asset_node* in_order[] = { &hall, &gold, &shed };
    const asset_node* shuffled[] = { &shed, &hall, &gold };
    const ostd::vector<uint8_t> section = obin_assets::encode(in_order);
    /// whatever order they are given in
    EXPECT_EQ(section, obin_assets::encode(shuffled));

    const opt<obin_assets> read = obin_assets::decode(section);
    ASSERT_TRUE(read.has_value());
    ASSERT_EQ(read->assets.size(), 3);
    for (const asset_node* written : in_order) {
      const obin_asset* found = read->find(asset_hash(written->kind), asset_hash(written->name));
      ASSERT_TRUE(found != nullptr) << written->name;
      /// the bytes it has by itself, whatever is beside it
      const ostd::vector<uint8_t> alone = asset_footprint(*written);
      EXPECT_TRUE(std::ranges::equal(found->footprint, alone)) << written->name;
      EXPECT_EQ(found->hash, asset_hash(std::span<const uint8_t>{ alone })) << written->name;
      EXPECT_EQ(found->footprint.data(), section.data() + found->offset);
    }
    EXPECT_EQ(read->find(asset_hash("room"), asset_hash("gold")), nullptr);

    const asset_node* one[] = { &gold };
    const opt<obin_assets> single = obin_assets::decode(obin_assets::encode(one));
    ASSERT_TRUE(single.has_value());
    EXPECT_EQ(single->assets[0].hash, read->find(asset_hash("material"), asset_hash("gold"))->hash);
  }

  TEST_F(asset_footprint_tests, a_section_that_was_cut_or_changed_is_refused) {
    const asset_node hall = asset(kRoom);
    const asset_node* assets[] = { &hall };
    const ostd::vector<uint8_t> section = obin_assets::encode(assets);
    ASSERT_TRUE(obin_assets::decode(section).has_value());

    EXPECT_FALSE(obin_assets::decode(std::span{ section }.first(section.size() - 1)).has_value());
    EXPECT_FALSE(obin_assets::decode(std::span{ section }.first(8)).has_value());
    ostd::vector<uint8_t> renamed = section;
    renamed[0] = 'X';
    EXPECT_FALSE(obin_assets::decode(renamed).has_value());
    ostd::vector<uint8_t> newer = section;
    newer[4] = kAssetSectionVersion + 1;
    EXPECT_FALSE(obin_assets::decode(newer).has_value());
    /// one bit of the footprint, and its row no longer says what it is
    ostd::vector<uint8_t> changed = section;
    changed.back() ^= 0x01;
    EXPECT_FALSE(obin_assets::decode(changed).has_value());

    /// a section of none is a section
    EXPECT_TRUE(obin_assets::decode(obin_assets::encode(std::span<const asset_node* const>{})).has_value());
  }

  TEST_F(asset_footprint_tests, what_could_not_be_read_back_is_not_written) {
    EXPECT_FALSE(asset_footprint_refusal(asset(kRoom)).has_value());
    std::string deep = "%a : b\n  set x, ";
    deep += std::string(200, '[') + std::string(200, ']') + "\nend\n";
    const opt<std::string> refusal = asset_footprint_refusal(asset(deep));
    ASSERT_TRUE(refusal.has_value());
    EXPECT_TRUE(refusal->contains("nests deeper"));
  }

}  // namespace other
