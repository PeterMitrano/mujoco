// Copyright 2022 DeepMind Technologies Limited
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Tests for ray casting.

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mujoco/mjdata.h>
#include <mujoco/mjmodel.h>
#include <mujoco/mjtnum.h>
#include <mujoco/mujoco.h>
#include "src/engine/engine_ray.h"
#include "test/fixture.h"

namespace mujoco {
namespace {

static constexpr char kSingleGeomModel[] = R"(
<mujoco>
  <worldbody>
    <body pos="-1 0 0">
      <geom type="sphere" size=".1"/>
    </body>
  </worldbody>
</mujoco>
)";

static constexpr char kRayCastingModel[] = R"(
<mujoco>
  <worldbody>
    <geom name="static_group1" type="sphere" size=".1" pos="1 0 0"
     group="1"/>
    <body pos="0 0 0">
      <body pos="0 0 0">
        <geom name="group0" type="sphere" size=".1" pos="3 0 0"/>
      </body>
      <geom name="group2" type="sphere" size=".1" pos="5 0 0" group="2"/>
    </body>
  </worldbody>
</mujoco>
)";

using ::testing::NotNull;
using RayTest = MujocoTest;

TEST_F(RayTest, NoExclusions) {
  mjModel* model = LoadModelFromString(kRayCastingModel);
  ASSERT_THAT(model, NotNull());
  mjData* data = mj_makeData(model);
  ASSERT_THAT(data, NotNull());

  mjtNum pnt[] = {0.0, 0.0, 0.0};
  mjtNum vec[] = {1.0, 0.0, 0.0};
  mjtByte* geomgroup = nullptr;
  mjtByte flg_static = 1;  // Include static geoms
  int bodyexclude = -1;
  int geomid = -1;

  mj_kinematics(model, data);
  mjtNum distance = mj_ray(model, data, pnt, vec, geomgroup, flg_static,
                           bodyexclude, &geomid);
  EXPECT_STREQ(mj_id2name(model, mjOBJ_GEOM, geomid), "static_group1");
  EXPECT_FLOAT_EQ(distance, 0.9);
  mj_deleteData(data);
  mj_deleteModel(model);
}

TEST_F(RayTest, Exclusions) {
  mjModel* model = LoadModelFromString(kRayCastingModel);
  ASSERT_THAT(model, NotNull());
  mjData* data = mj_makeData(model);
  ASSERT_THAT(data, NotNull());

  mjtNum pnt[] = {0.0, 0.0, 0.0};
  mjtNum vec[] = {1.0, 0.0, 0.0};
  mjtByte geomgroup[] = {1, 1, 1};
  mjtByte flg_static = 1;
  int bodyexclude = -1;
  int geomid = -1;

  mj_kinematics(model, data);
  mjtNum distance = mj_ray(model, data, pnt, vec, geomgroup, flg_static,
                           bodyexclude, &geomid);
  EXPECT_STREQ(mj_id2name(model, mjOBJ_GEOM, geomid), "static_group1");
  EXPECT_FLOAT_EQ(distance, 0.9);

  // Exclude nearest geom
  geomgroup[1] = 0;
  distance = mj_ray(model, data, pnt, vec, geomgroup, flg_static, bodyexclude,
                    &geomid);
  EXPECT_STREQ(mj_id2name(model, mjOBJ_GEOM, geomid), "group0");
  EXPECT_FLOAT_EQ(distance, 2.9);

  geomgroup[0] = 0;
  distance = mj_ray(model, data, pnt, vec, geomgroup, flg_static, bodyexclude,
                    &geomid);
  EXPECT_STREQ(mj_id2name(model, mjOBJ_GEOM, geomid), "group2");
  EXPECT_FLOAT_EQ(distance, 4.9);

  geomgroup[2] = 0;
  distance = mj_ray(model, data, pnt, vec, geomgroup, flg_static, bodyexclude,
                    &geomid);
  EXPECT_EQ(geomid, -1);
  EXPECT_FLOAT_EQ(distance, -1);

  mj_deleteData(data);
  mj_deleteModel(model);
}

TEST_F(RayTest, ExcludeStatic) {
  mjModel* model = LoadModelFromString(kRayCastingModel);
  ASSERT_THAT(model, NotNull());
  mjData* data = mj_makeData(model);
  ASSERT_THAT(data, NotNull());

  mjtNum pnt[] = {0.0, 0.0, 0.0};
  mjtNum vec[] = {1.0, 0.0, 0.0};
  mjtByte geomgroup[] = {1, 1, 1};
  mjtByte flg_static = 0;  // Exclude static geoms
  int bodyexclude = -1;
  int geomid = -1;

  mj_kinematics(model, data);
  mjtNum distance = mj_ray(model, data, pnt, vec, geomgroup, flg_static,
                           bodyexclude, &geomid);
  EXPECT_STREQ(mj_id2name(model, mjOBJ_GEOM, geomid), "group0");
  EXPECT_FLOAT_EQ(distance, 2.9);
  mj_deleteData(data);
  mj_deleteModel(model);
}

TEST_F(RayTest, MultiRayEqualsSingleRay) {
  mjModel* m = LoadModelFromString(kRayCastingModel);
  ASSERT_THAT(m, NotNull());
  mjData* d = mj_makeData(m);
  ASSERT_THAT(d, NotNull());
  mj_forward(m, d);

  // create ray array
  constexpr int N = 80;
  constexpr int M = 60;
  mjtNum vec[3*N*M];
  mjtNum pnt[3] = {1, 2, 3};
  mjtNum cone[4][3] = {{1, 1, -1}, {1, 1, 1}, {1, -1, -1}, {1, -1, 1}};
  memset(vec, 0, 3*N*M*sizeof(mjtNum));

  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < M; ++j) {
      for (int k = 0; k < 3; ++k) {
        vec[3 * (i * M + j) + k] =           i * cone[0][k] / (N - 1) +
                                             j * cone[1][1] / (M - 1) +
                                   (N - i - 1) * cone[2][k] / (N - 1) +
                                   (M - j - 1) * cone[3][k] / (M - 1);
      }
    }
  }

  // compute intersections with multiray functions
  mjtNum dist_multiray[3*N*M];
  int rgeomid_multiray[N*M];
  mj_multiRay(m, d, pnt, vec, NULL, 1, -1, rgeomid_multiray, dist_multiray, N*M);

  // compare results with single ray function
  mjtNum dist;
  int rgeomid;

  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < M; ++j) {
      int idx = i * M + j;
      dist = mj_ray(m, d, pnt, vec + 3 * idx, NULL, 1, -1, &rgeomid);
      EXPECT_FLOAT_EQ(dist, dist_multiray[idx]);
      EXPECT_EQ(rgeomid, rgeomid_multiray[idx]);
    }
  }

  mj_deleteData(d);
  mj_deleteModel(m);
}

TEST_F(RayTest, EdgeCases) {
  mjModel* m = LoadModelFromString(kSingleGeomModel);
  ASSERT_THAT(m, NotNull());
  ASSERT_THAT(m->nbvh, 1);
  mjData* d = mj_makeData(m);
  ASSERT_THAT(d, NotNull());
  mj_forward(m, d);

  // spherical bounding box and result arrays
  mjtNum geom_ba[4];
  mjtNum dist;
  int rgeomid;

  // pnt contained in bounding box
  mjtNum pnt1[] = {-1, 0, 0};
  mju_multiRayPrepare(m, d, pnt1, NULL, NULL, 1, -1, geom_ba, NULL);
  EXPECT_FLOAT_EQ(geom_ba[0], -mjPI);
  EXPECT_FLOAT_EQ(geom_ba[1],  0);
  EXPECT_FLOAT_EQ(geom_ba[2],  mjPI);
  EXPECT_FLOAT_EQ(geom_ba[3],  mjPI);
  mjtNum vec1[] = {1, 0, 0};
  mj_multiRay(m, d, pnt1, vec1, NULL, 1, -1, &rgeomid, &dist, 1);
  EXPECT_FLOAT_EQ(dist, 0.1);

  // pnt at phi = Pi, -Pi
  mjtNum pnt2[] = {-.5, 0, 0};
  mju_multiRayPrepare(m, d, pnt2, NULL, NULL, 1, -1, geom_ba, NULL);
  EXPECT_FLOAT_EQ(geom_ba[0], -mjPI);  // atan(y<0, x<0)
  EXPECT_FLOAT_EQ(geom_ba[2],  mjPI);  // atan(y>0, x<0)
  mjtNum vec2[] = {-1, 0, 0};
  mj_multiRay(m, d, pnt2, vec2, NULL, 1, -1, &rgeomid, &dist, 1);
  EXPECT_FLOAT_EQ(dist, 0.4);

  // pnt on the boundary of the box
  mjtNum pnt3[] = {.1, .1, .05};
  mju_multiRayPrepare(m, d, pnt3, NULL, NULL, 1, -1, geom_ba, NULL);
  EXPECT_FLOAT_EQ(geom_ba[1], 0);
  EXPECT_FLOAT_EQ(geom_ba[3], mjPI);
  mjtNum vec3[] = {1, 1, 0};
  mj_multiRay(m, d, pnt3, vec3, NULL, 1, -1, &rgeomid, &dist, 1);
  EXPECT_FLOAT_EQ(dist, -1);

  // size 0 geom
  mjtNum pnt4[] = {-2, 0, 0};
  m->geom_aabb[0] = m->geom_aabb[1] = m->geom_aabb[2] = 0;
  m->geom_aabb[3] = m->geom_aabb[4] = m->geom_aabb[5] = 0;
  mju_multiRayPrepare(m, d, pnt4, NULL, NULL, 1, -1, geom_ba, NULL);
  EXPECT_FLOAT_EQ(geom_ba[0], 0);
  EXPECT_FLOAT_EQ(geom_ba[1], mjPI/2);
  EXPECT_FLOAT_EQ(geom_ba[2], 0);
  EXPECT_FLOAT_EQ(geom_ba[3], mjPI/2);
  mjtNum vec4[] = {1, 0, 0};
  mj_multiRay(m, d, pnt4, vec4, NULL, 1, -1, &rgeomid, &dist, 1);
  EXPECT_FLOAT_EQ(dist, 0.9);

  mj_deleteData(d);
  mj_deleteModel(m);
}

}  // namespace
}  // namespace mujoco
