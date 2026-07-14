// SPDX-License-Identifier: GPL-3.0-or-later
#include "yohsai_cosserat/c_api.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

float distance(const float* left, const float* right) {
    const float x = right[0] - left[0];
    const float y = right[1] - left[1];
    const float z = right[2] - left[2];
    return std::sqrt(x * x + y * y + z * z);
}

struct NativeSolver {
    ysc_handle handle = nullptr;
    int32_t vertex_count = 0;
    int32_t segment_count = 0;
    int32_t angle_count = 0;
    int32_t quad_count = 0;
    int32_t seam_count = 0;

    NativeSolver(const ysc_create_desc& desc, const ysc_config& config) {
        std::array<char, 512> error{};
        const ysc_status status = ysc_create(&desc, &config, &handle, error.data(), static_cast<int32_t>(error.size()));
        require(status == YSC_STATUS_OK, std::string("ysc_create failed: ") + error.data());
        const ysc_status count_status = ysc_get_counts(
            handle,
            &vertex_count,
            &segment_count,
            &angle_count,
            &quad_count,
            &seam_count,
            error.data(),
            static_cast<int32_t>(error.size()));
        require(count_status == YSC_STATUS_OK, std::string("ysc_get_counts failed: ") + error.data());
    }

    NativeSolver(const NativeSolver&) = delete;
    NativeSolver& operator=(const NativeSolver&) = delete;

    ~NativeSolver() {
        ysc_destroy(handle);
    }

    ysc_stats advance(
        const std::array<float, 3>& gravity,
        float seam_closure,
        int32_t iterations = 0,
        const std::vector<int32_t>& body_candidates = {},
        const std::vector<int32_t>& self_candidates = {}) {
        ysc_advance_desc desc{};
        std::copy(gravity.begin(), gravity.end(), desc.gravity);
        desc.seam_closure = seam_closure;
        desc.iterations = iterations;
        desc.body_candidate_count = static_cast<int32_t>(body_candidates.size() / 2);
        desc.body_candidates = body_candidates.empty() ? nullptr : body_candidates.data();
        desc.self_candidate_count = static_cast<int32_t>(self_candidates.size() / 2);
        desc.self_candidates = self_candidates.empty() ? nullptr : self_candidates.data();
        ysc_stats stats{};
        std::array<char, 512> error{};
        const ysc_status status = ysc_advance(
            handle,
            &desc,
            &stats,
            error.data(),
            static_cast<int32_t>(error.size()));
        require(status == YSC_STATUS_OK, std::string("ysc_advance failed: ") + error.data());
        return stats;
    }

    std::vector<float> positions() const {
        std::vector<float> positions(static_cast<size_t>(vertex_count) * 3);
        std::vector<float> velocities(static_cast<size_t>(vertex_count) * 3);
        std::array<char, 512> error{};
        const ysc_status status = ysc_copy_state(
            handle,
            positions.data(),
            velocities.data(),
            error.data(),
            static_cast<int32_t>(error.size()));
        require(status == YSC_STATUS_OK, std::string("ysc_copy_state failed: ") + error.data());
        return positions;
    }

    std::vector<float> orientations() const {
        std::vector<float> result(static_cast<size_t>(segment_count) * 4);
        std::array<char, 512> error{};
        const ysc_status status = ysc_copy_orientations(
            handle,
            result.data(),
            error.data(),
            static_cast<int32_t>(error.size()));
        require(status == YSC_STATUS_OK, std::string("ysc_copy_orientations failed: ") + error.data());
        return result;
    }
};

ysc_config test_config() {
    ysc_config config{};
    require(ysc_default_config(&config) == YSC_STATUS_OK, "default config failed");
    config.substeps = 1;
    config.iterations = 8;
    config.maximum_position_correction = 0.05F;
    return config;
}

ysc_create_desc chain_desc(
    const std::vector<float>& positions,
    const std::vector<float>& rest,
    const std::vector<int32_t>& edges,
    const std::vector<float>& edge_rest,
    const std::vector<int32_t>& locked,
    const std::vector<int32_t>& seams = {},
    const std::vector<int32_t>& quads = {}) {
    ysc_create_desc desc{};
    desc.vertex_count = static_cast<int32_t>(positions.size() / 3);
    desc.positions = positions.data();
    desc.rest_frame_positions = rest.data();
    desc.material_rest_positions = rest.data();
    desc.locked = locked.empty() ? nullptr : locked.data();
    desc.edge_count = static_cast<int32_t>(edges.size() / 2);
    desc.edges = edges.data();
    desc.edge_rest_lengths = edge_rest.data();
    desc.quad_count = static_cast<int32_t>(quads.size() / 4);
    desc.quads = quads.empty() ? nullptr : quads.data();
    desc.seam_count = static_cast<int32_t>(seams.size() / 2);
    desc.seams = seams.empty() ? nullptr : seams.data();
    return desc;
}

void test_api_and_invalid_input() {
    require(ysc_get_api_version() == YSC_API_VERSION, "API version mismatch");
    require(ysc_default_config(nullptr) == YSC_STATUS_INVALID_ARGUMENT, "null default config was accepted");

    const std::vector<float> positions{0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F};
    const std::vector<int32_t> edges{0, 1};
    const std::vector<float> edge_rest{1.0F};
    const std::vector<int32_t> locked{0, 0};
    ysc_create_desc desc = chain_desc(positions, positions, edges, edge_rest, locked);
    desc.positions = nullptr;
    ysc_handle handle = nullptr;
    std::array<char, 512> error{};
    const ysc_config config = test_config();
    const ysc_status status = ysc_create(&desc, &config, &handle, error.data(), static_cast<int32_t>(error.size()));
    require(status == YSC_STATUS_INVALID_ARGUMENT, "missing position input was accepted");
    require(handle == nullptr, "failed create returned a handle");
    require(error[0] != '\0', "failed create returned no error message");
}

void test_single_segment_rest_state() {
    const std::vector<float> positions{0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F};
    const std::vector<int32_t> edges{0, 1};
    const std::vector<float> edge_rest{1.0F};
    const std::vector<int32_t> locked{0, 0};
    const ysc_create_desc desc = chain_desc(positions, positions, edges, edge_rest, locked);
    NativeSolver solver(desc, test_config());
    require(solver.segment_count == 1, "single segment count is wrong");
    require(solver.angle_count == 0, "single segment unexpectedly has an angle");
    const ysc_stats stats = solver.advance({0.0F, 0.0F, 0.0F}, 0.0F);
    const std::vector<float> solved = solver.positions();
    require(stats.maximum_displacement < 1.0e-5F, "rest segment drifted");
    require(distance(solved.data(), solved.data() + 3) > 0.9999F, "rest segment changed length");
}

void test_rigid_rotation_and_angle_graph() {
    const std::vector<float> rest{
        0.0F, 0.0F, 0.0F,
        0.0F, 0.0F, 1.0F,
        0.0F, 0.0F, 2.0F,
    };
    const std::vector<float> positions{
        3.0F, -2.0F, 4.0F,
        4.0F, -2.0F, 4.0F,
        5.0F, -2.0F, 4.0F,
    };
    const std::vector<int32_t> edges{0, 1, 1, 2};
    const std::vector<float> edge_rest{1.0F, 1.0F};
    const std::vector<int32_t> locked{0, 0, 0};
    const ysc_create_desc desc = chain_desc(positions, rest, edges, edge_rest, locked);
    NativeSolver solver(desc, test_config());
    require(solver.angle_count == 1, "straight chain did not create one angle");
    const ysc_stats stats = solver.advance({0.0F, 0.0F, 0.0F}, 0.0F);
    require(stats.maximum_displacement < 2.0e-5F, "rigidly transformed chain drifted");
    const std::vector<float> orientations = solver.orientations();
    for (int32_t index = 0; index < solver.segment_count; ++index) {
        const float* q = orientations.data() + index * 4;
        const float norm = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
        require(std::abs(norm - 1.0F) < 1.0e-5F, "orientation is not unit length");
    }
}

void test_progressive_seam_and_lock() {
    const std::vector<float> positions{
        0.0F, 0.0F, 0.0F,
        0.0F, 0.0F, 1.0F,
        2.0F, 0.0F, 1.0F,
        2.0F, 0.0F, 0.0F,
    };
    const std::vector<int32_t> edges{0, 1, 2, 3};
    const std::vector<float> edge_rest{1.0F, 1.0F};
    const std::vector<int32_t> locked{1, 1, 0, 0};
    const std::vector<int32_t> seams{1, 2};
    const ysc_create_desc desc = chain_desc(positions, positions, edges, edge_rest, locked, seams);
    ysc_config config = test_config();
    config.iterations = 1;
    config.seam_projection_passes = 1;
    NativeSolver solver(desc, config);
    require(solver.seam_count == 1, "seam count is wrong");
    const ysc_stats stats = solver.advance({0.0F, 0.0F, 0.0F}, 0.25F, 1);
    const std::vector<float> solved = solver.positions();
    require(distance(solved.data(), positions.data()) < 1.0e-7F, "locked vertex moved");
    require(distance(solved.data() + 3, positions.data() + 3) < 1.0e-7F, "locked seam endpoint moved");
    const float seam_distance = distance(solved.data() + 3, solved.data() + 6);
    require(seam_distance < 1.81F, "progressive seam did not move the free panel toward its target");
    require(stats.maximum_edge_strain < 0.1F, "progressive seam left an excessive local edge strain");
}

void test_deep_body_contact_is_incremental() {
    const std::vector<float> positions{
        0.1F, 0.1F, -0.05F,
        0.2F, 0.1F, -0.05F,
    };
    const std::vector<int32_t> edges{0, 1};
    const std::vector<float> edge_rest{0.1F};
    const std::vector<int32_t> locked{0, 0};
    const std::vector<float> body_positions{
        0.0F, 0.0F, 0.0F,
        1.0F, 0.0F, 0.0F,
        0.0F, 1.0F, 0.0F,
    };
    const std::vector<int32_t> body_faces{0, 1, 2};
    ysc_create_desc desc = chain_desc(positions, positions, edges, edge_rest, locked);
    desc.body_vertex_count = 3;
    desc.body_positions = body_positions.data();
    desc.body_face_count = 1;
    desc.body_faces = body_faces.data();
    ysc_config config = test_config();
    config.iterations = 8;
    NativeSolver solver(desc, config);
    const ysc_stats stats = solver.advance(
        {0.0F, 0.0F, 0.0F}, 0.0F, 8, std::vector<int32_t>{0, 0});
    const std::vector<float> solved = solver.positions();
    require(
        solved[2] > positions[2],
        "deep Body contact did not move toward the surface: " + std::to_string(solved[2]));
    require(solved[2] < -0.04F, "deep Body contact was projected to the surface in one advance");
    require(stats.maximum_edge_strain < 0.02F, "incremental Body contact tore the test segment");
}

void test_coplanar_nearby_triangle_does_not_repel() {
    const std::vector<float> positions{
        0.0F, 0.0F, 0.0F,
        1.0F, 0.0F, 0.0F,
        0.0F, 1.0F, 0.0F,
        1.001F, 1.001F, 0.0F,
        1.101F, 1.001F, 0.0F,
    };
    const std::vector<int32_t> edges{0, 1, 1, 2, 2, 0, 3, 4};
    const std::vector<float> edge_rest{1.0F, std::sqrt(2.0F), 1.0F, 0.1F};
    const std::vector<int32_t> locked{0, 0, 0, 0, 0};
    const std::vector<int32_t> faces{0, 1, 2};
    ysc_create_desc desc = chain_desc(positions, positions, edges, edge_rest, locked);
    desc.face_count = 1;
    desc.faces = faces.data();
    NativeSolver solver(desc, test_config());
    const ysc_stats stats = solver.advance(
        {0.0F, 0.0F, 0.0F}, 0.0F, 1, {}, std::vector<int32_t>{3, 0});
    require(stats.maximum_displacement < 1.0e-5F, "coplanar nearby triangle caused false self-contact");
}

void test_grain_quad_rest_and_rigid_transform() {
    const std::vector<float> material{
        0.0F, 0.0F, 0.0F,
        1.0F, 0.0F, 0.0F,
        1.0F, 0.0F, 1.0F,
        0.0F, 0.0F, 1.0F,
    };
    const std::vector<int32_t> edges{0, 1, 1, 2, 2, 3, 3, 0};
    const std::vector<float> edge_rest{1.0F, 1.0F, 1.0F, 1.0F};
    const std::vector<int32_t> locked{0, 0, 0, 0};
    const std::vector<int32_t> quads{0, 1, 2, 3};

    const ysc_create_desc rest_desc = chain_desc(material, material, edges, edge_rest, locked, {}, quads);
    NativeSolver rest_solver(rest_desc, test_config());
    require(rest_solver.quad_count == 1, "grain quad count is wrong");
    const ysc_stats rest_stats = rest_solver.advance({0.0F, 0.0F, 0.0F}, 0.0F);
    require(rest_stats.quad_count == 1, "grain quad stats count is wrong");
    require(rest_stats.maximum_displacement < 1.0e-5F, "rest grain quad drifted");
    require(rest_stats.shear_energy < 1.0e-6F, "rest grain quad has shear energy");
    require(rest_stats.area_energy < 1.0e-6F, "rest grain quad has area energy");

    const std::vector<float> transformed{
        3.0F, -2.0F, 4.0F,
        3.0F, -1.0F, 4.0F,
        4.0F, -1.0F, 4.0F,
        4.0F, -2.0F, 4.0F,
    };
    ysc_create_desc transformed_desc = chain_desc(
        transformed, material, edges, edge_rest, locked, {}, quads);
    transformed_desc.material_rest_positions = material.data();
    NativeSolver transformed_solver(transformed_desc, test_config());
    const ysc_stats transformed_stats = transformed_solver.advance({0.0F, 0.0F, 0.0F}, 0.0F);
    require(transformed_stats.maximum_displacement < 2.0e-5F, "rigidly transformed grain quad drifted");
}

void test_grain_quad_shear_and_area_response() {
    const std::vector<float> material{
        0.0F, 0.0F, 0.0F,
        1.0F, 0.0F, 0.0F,
        1.0F, 0.0F, 1.0F,
        0.0F, 0.0F, 1.0F,
    };
    const std::vector<int32_t> edges{0, 1, 1, 2, 2, 3, 3, 0};
    const std::vector<int32_t> locked{1, 1, 0, 0};
    const std::vector<int32_t> quads{0, 1, 2, 3};

    const std::vector<float> sheared{
        0.0F, 0.0F, 0.0F,
        1.0F, 0.0F, 0.0F,
        1.4F, 0.0F, 1.0F,
        0.4F, 0.0F, 1.0F,
    };
    const float side = std::sqrt(1.16F);
    const std::vector<float> sheared_edge_rest{1.0F, side, 1.0F, side};
    ysc_create_desc shear_desc = chain_desc(
        sheared, sheared, edges, sheared_edge_rest, locked, {}, quads);
    shear_desc.material_rest_positions = material.data();
    ysc_config shear_config = test_config();
    shear_config.iterations = 16;
    shear_config.stretch_stiffness = 1.0e-3F;
    shear_config.quad_area_stiffness = 0.0F;
    NativeSolver shear_solver(shear_desc, shear_config);
    const ysc_stats shear_stats = shear_solver.advance({0.0F, 0.0F, 0.0F}, 0.0F);
    const std::vector<float> shear_result = shear_solver.positions();
    require(
        0.5F * (shear_result[6] + shear_result[9]) < 0.85F,
        "grain quad shear did not restore the free edge");
    require(std::isfinite(shear_stats.shear_energy), "grain quad shear energy is non-finite");

    const std::vector<float> compressed{
        0.0F, 0.0F, 0.0F,
        1.0F, 0.0F, 0.0F,
        1.0F, 0.0F, 0.5F,
        0.0F, 0.0F, 0.5F,
    };
    const std::vector<float> compressed_edge_rest{1.0F, 0.5F, 1.0F, 0.5F};
    ysc_create_desc area_desc = chain_desc(
        compressed, compressed, edges, compressed_edge_rest, locked, {}, quads);
    area_desc.material_rest_positions = material.data();
    ysc_config area_config = test_config();
    area_config.iterations = 16;
    area_config.stretch_stiffness = 1.0e-3F;
    area_config.quad_shear_stiffness = 0.0F;
    NativeSolver area_solver(area_desc, area_config);
    const ysc_stats area_stats = area_solver.advance({0.0F, 0.0F, 0.0F}, 0.0F);
    const std::vector<float> area_result = area_solver.positions();
    require(
        0.5F * (area_result[8] + area_result[11]) > 0.55F,
        "grain quad area did not restore the compressed free edge");
    require(std::isfinite(area_stats.area_energy), "grain quad area energy is non-finite");
}

void test_gravity_is_finite_and_bounded() {
    const std::vector<float> positions{
        0.0F, 0.0F, 0.0F,
        0.0F, 0.0F, 1.0F,
        0.0F, 0.0F, 2.0F,
    };
    const std::vector<int32_t> edges{0, 1, 1, 2};
    const std::vector<float> edge_rest{1.0F, 1.0F};
    const std::vector<int32_t> locked{1, 0, 0};
    const ysc_create_desc desc = chain_desc(positions, positions, edges, edge_rest, locked);
    ysc_config config = test_config();
    config.substeps = 4;
    NativeSolver solver(desc, config);
    for (int step = 0; step < 20; ++step) {
        const ysc_stats stats = solver.advance({0.0F, 0.0F, -1.0F}, 0.0F);
        require(std::isfinite(stats.stretch_energy), "stretch energy is non-finite");
        require(std::isfinite(stats.bend_energy), "bend energy is non-finite");
        require(stats.maximum_edge_strain < 0.2F, "rod chain strain exceeded the safety envelope");
    }
    const std::vector<float> solved = solver.positions();
    require(distance(solved.data(), positions.data()) < 1.0e-7F, "gravity moved a locked root");
    for (float value : solved) {
        require(std::isfinite(value), "gravity test produced a non-finite position");
    }
}

}  // namespace

int main() {
    try {
        test_api_and_invalid_input();
        test_single_segment_rest_state();
        test_rigid_rotation_and_angle_graph();
        test_progressive_seam_and_lock();
        test_deep_body_contact_is_incremental();
        test_coplanar_nearby_triangle_does_not_repel();
        test_grain_quad_rest_and_rigid_transform();
        test_grain_quad_shear_and_area_response();
        test_gravity_is_finite_and_bounded();
        std::cout << "All Stable Cosserat native tests passed.\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& exception) {
        std::cerr << "Test failure: " << exception.what() << '\n';
        return EXIT_FAILURE;
    }
}
