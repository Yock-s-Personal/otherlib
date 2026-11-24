/**
 * \file tests/core/bounding_box_tests.cpp
 **/
#include <gtest/gtest.h>

#include "math/bounding_box.hpp"
#include "math/ray.hpp"

#include "other_test.hpp"

namespace other {

  class bounding_box_tests : public other_test {
   protected:
    const bounding_box box{ { -1.f, -1.f, -1.f }, { 1.f, 1.f, 1.f } };
  };

  TEST_F(bounding_box_tests, a_ray_that_goes_through_the_box_meets_it) {
    EXPECT_TRUE(box.intersects(ray{ { 0.5f, 0.5f, 5.f }, { 0.f, 0.f, -1.f } }));
    /// at a slant, in at one side and out at another
    EXPECT_TRUE(box.intersects(ray{ { -5.f, -5.f, -5.f }, { 1.f, 1.f, 1.f } }));
    EXPECT_TRUE(box.intersects(ray{ { 3.f, 0.2f, 0.f }, { -1.f, 0.1f, 0.2f } }));
  }

  TEST_F(bounding_box_tests, a_ray_that_passes_the_box_does_not_meet_it) {
    EXPECT_FALSE(box.intersects(ray{ { 1.5f, 0.f, 5.f }, { 0.f, 0.f, -1.f } }));
    /// past a corner: between two of its three pairs of sides at once, never all three
    EXPECT_FALSE(box.intersects(ray{ { 1.2f, 1.2f, 5.f }, { 0.f, 0.f, -1.f } }));
    EXPECT_FALSE(box.intersects(ray{ { 3.f, 0.f, 0.f }, { -1.f, 1.f, 0.f } }));
  }

  TEST_F(bounding_box_tests, a_ray_that_points_away_from_the_box_does_not_meet_it) {
    EXPECT_FALSE(box.intersects(ray{ { 0.f, 0.f, 5.f }, { 0.f, 0.f, 1.f } }));
    EXPECT_FALSE(box.intersects(ray{ { -5.f, -5.f, -5.f }, { -1.f, -1.f, -1.f } }));
  }

  TEST_F(bounding_box_tests, a_ray_from_inside_the_box_meets_it) {
    EXPECT_TRUE(box.intersects(ray{ { 0.f, 0.f, 0.f }, { 1.f, 0.f, 0.f } }));
    EXPECT_TRUE(box.intersects(ray{ { 0.5f, -0.5f, 0.2f }, { -1.f, 2.f, 0.5f } }));
  }

  TEST_F(bounding_box_tests, a_ray_along_a_side_is_judged_by_where_it_starts) {
    /// it never crosses the two sides it runs along, so it is inside them or it is not
    EXPECT_TRUE(box.intersects(ray{ { 0.f, 0.f, 5.f }, { 0.f, 0.f, -1.f } }));
    EXPECT_FALSE(box.intersects(ray{ { 0.f, 2.f, 5.f }, { 0.f, 0.f, -1.f } }));
  }

}  // namespace other
