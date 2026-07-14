// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "math.hpp"
#include "yohsai_cosserat/c_api.h"

#include <array>
#include <cstdint>
#include <vector>

namespace ysc {

class Solver {
public:
    Solver(const ysc_create_desc& desc, const ysc_config& config);

    [[nodiscard]] int32_t vertex_count() const noexcept;
    [[nodiscard]] int32_t segment_count() const noexcept;
    [[nodiscard]] int32_t angle_count() const noexcept;
    [[nodiscard]] int32_t quad_count() const noexcept;
    [[nodiscard]] int32_t seam_count() const noexcept;

    void replace_state(
        const float* positions,
        const float* velocities,
        const int32_t* locked,
        bool reinitialize_orientations);
    void copy_state(float* positions, float* velocities) const;

    void replace_orientations(const float* quaternions_wxyz);
    void copy_orientations(float* quaternions_wxyz) const;

    void replace_seam_state(const float* maximum_lengths);
    void copy_seam_state(float* maximum_lengths) const;

    ysc_stats advance(const ysc_advance_desc& desc);

private:
    struct Vertex {
        Vec3 position;
        Vec3 previous;
        Vec3 predicted;
        Vec3 velocity;
        float unlocked_inverse_mass = 1.0F;
        float inverse_mass = 1.0F;
        bool locked = false;
    };

    struct Segment {
        int32_t a = 0;
        int32_t b = 0;
        float rest_length = 0.0F;
        float stretch_stiffness = 0.0F;
        Quat orientation;
        Quat rest_orientation;
    };

    struct Angle {
        int32_t a = 0;
        int32_t b = 0;
        float bend_stiffness = 0.0F;
        Quat rest_relative;
    };

    struct Seam {
        int32_t a = 0;
        int32_t b = 0;
        float maximum_length = 0.0F;
    };

    struct Quad {
        std::array<int32_t, 4> vertices{};
        float rest_product = 0.0F;
        float rest_shear = 0.0F;
        float rest_area = 0.0F;
        float shear_stiffness = 0.0F;
        float area_stiffness = 0.0F;
        Vec3 rest_normal;
    };

    using Face = std::array<int32_t, 3>;

    ysc_config config_{};
    std::vector<Vertex> vertices_;
    std::vector<Vec3> rest_positions_;
    std::vector<Vec3> material_rest_positions_;
    std::vector<Segment> segments_;
    std::vector<Angle> angles_;
    std::vector<Quad> quads_;
    std::vector<Seam> seams_;
    std::vector<Face> faces_;
    std::vector<Vec3> body_positions_;
    std::vector<Face> body_faces_;
    std::vector<std::vector<int32_t>> vertex_segments_;
    std::vector<std::vector<int32_t>> vertex_quads_;
    std::vector<std::vector<int32_t>> segment_angles_;

    void validate_config() const;
    void build_segments(const ysc_create_desc& desc);
    void build_quads(const ysc_create_desc& desc);
    void build_angles();
    void initialize_orientations_from_geometry();
    [[nodiscard]] std::vector<Vec3> geometry_vertex_normals(const std::vector<Vec3>& positions) const;

    void predict(const Vec3& gravity);
    void position_sweep(float time_step);
    void orientation_sweep();
    void project_body_contacts(const int32_t* candidates, int32_t count);
    void project_self_contacts(const int32_t* candidates, int32_t count);
    void project_seams();
    void ratchet_seams();
    void finish_substep(float time_step);

    [[nodiscard]] Vec3 closest_triangle_point(
        const Vec3& point,
        const Vec3& a,
        const Vec3& b,
        const Vec3& c) const;
    [[nodiscard]] float maximum_edge_strain() const;
    void compute_energy(float& stretch, float& bend, float& shear, float& area) const;
    void require_finite_state() const;
};

ysc_config default_config();

}  // namespace ysc
