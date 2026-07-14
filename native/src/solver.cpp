// SPDX-License-Identifier: GPL-3.0-or-later
#include "solver.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
#include <utility>

namespace ysc {
namespace {

Vec3 read_vec3(const float* values, int32_t index) {
    return {values[index * 3], values[index * 3 + 1], values[index * 3 + 2]};
}

void write_vec3(float* values, int32_t index, const Vec3& value) {
    values[index * 3] = value.x;
    values[index * 3 + 1] = value.y;
    values[index * 3 + 2] = value.z;
}

Quat read_quat(const float* values, int32_t index) {
    return {
        values[index * 4],
        values[index * 4 + 1],
        values[index * 4 + 2],
        values[index * 4 + 3],
    };
}

void write_quat(float* values, int32_t index, const Quat& value) {
    values[index * 4] = value.w;
    values[index * 4 + 1] = value.x;
    values[index * 4 + 2] = value.y;
    values[index * 4 + 3] = value.z;
}

void validate_index(int32_t index, int32_t size, const char* label) {
    if (index < 0 || index >= size) {
        throw std::out_of_range(std::string(label) + " index is out of range");
    }
}

float quaternion_distance_squared(const Quat& left, const Quat& right) {
    return length_squared(left - right);
}

bool project_to_triangle_interior(
    const Vec3& point,
    const Vec3& a,
    const Vec3& b,
    const Vec3& c,
    Vec3& normal,
    float& signed_distance) {
    const Vec3 ab = b - a;
    const Vec3 ac = c - a;
    const Vec3 unnormalized_normal = cross(ab, ac);
    const float normal_squared = length_squared(unnormalized_normal);
    if (!(normal_squared > 1.0e-16F)) {
        return false;
    }
    normal = unnormalized_normal / std::sqrt(normal_squared);
    signed_distance = dot(point - a, normal);
    const Vec3 projected = point - signed_distance * normal;
    const Vec3 ap = projected - a;
    const float d00 = dot(ab, ab);
    const float d01 = dot(ab, ac);
    const float d11 = dot(ac, ac);
    const float d20 = dot(ap, ab);
    const float d21 = dot(ap, ac);
    const float denominator = d00 * d11 - d01 * d01;
    if (!(std::abs(denominator) > 1.0e-16F)) {
        return false;
    }
    const float v = (d11 * d20 - d01 * d21) / denominator;
    const float w = (d00 * d21 - d01 * d20) / denominator;
    const float u = 1.0F - v - w;
    constexpr float kInteriorMargin = 1.0e-5F;
    return u > kInteriorMargin && v > kInteriorMargin && w > kInteriorMargin;
}

}  // namespace

ysc_config default_config() {
    ysc_config config{};
    config.time_step = 1.0F / 240.0F;
    config.substeps = 8;
    config.iterations = 16;
    config.stretch_stiffness = 2.0e6F;
    config.bend_stiffness = 2.0e-4F;
    config.quad_shear_stiffness = 2.0e5F;
    config.quad_area_stiffness = 2.0e5F;
    config.straight_pair_cosine = -0.65F;
    config.seam_projection_passes = 4;
    config.velocity_damping_per_second = 4.0F;
    config.maximum_speed = 1.0F;
    config.maximum_position_correction = 0.005F;
    config.contact_thickness = 0.005F;
    return config;
}

Solver::Solver(const ysc_create_desc& desc, const ysc_config& config) : config_(config) {
    validate_config();
    if (
        desc.vertex_count <= 0 || desc.positions == nullptr || desc.rest_frame_positions == nullptr ||
        desc.material_rest_positions == nullptr) {
        throw std::invalid_argument("create descriptor has no vertex data");
    }
    if (desc.edge_count <= 0 || desc.edges == nullptr || desc.edge_rest_lengths == nullptr) {
        throw std::invalid_argument("create descriptor has no edge data");
    }
    if (desc.seam_count < 0 || (desc.seam_count > 0 && desc.seams == nullptr)) {
        throw std::invalid_argument("create descriptor has invalid seam data");
    }
    if (desc.quad_count < 0 || (desc.quad_count > 0 && desc.quads == nullptr)) {
        throw std::invalid_argument("create descriptor has invalid quad data");
    }
    if (desc.face_count < 0 || (desc.face_count > 0 && desc.faces == nullptr)) {
        throw std::invalid_argument("create descriptor has invalid face data");
    }
    if (
        desc.body_vertex_count < 0 || desc.body_face_count < 0 ||
        (desc.body_vertex_count > 0 && desc.body_positions == nullptr) ||
        (desc.body_face_count > 0 && desc.body_faces == nullptr)) {
        throw std::invalid_argument("create descriptor has invalid Body data");
    }

    vertices_.resize(static_cast<size_t>(desc.vertex_count));
    rest_positions_.resize(static_cast<size_t>(desc.vertex_count));
    material_rest_positions_.resize(static_cast<size_t>(desc.vertex_count));
    for (int32_t index = 0; index < desc.vertex_count; ++index) {
        Vertex& vertex = vertices_[static_cast<size_t>(index)];
        vertex.position = read_vec3(desc.positions, index);
        vertex.previous = vertex.position;
        vertex.predicted = vertex.position;
        vertex.velocity = desc.velocities != nullptr ? read_vec3(desc.velocities, index) : Vec3{};
        vertex.unlocked_inverse_mass = desc.inverse_masses != nullptr ? desc.inverse_masses[index] : 1.0F;
        vertex.inverse_mass = vertex.unlocked_inverse_mass;
        vertex.locked = desc.locked != nullptr && desc.locked[index] != 0;
        if (vertex.locked) {
            vertex.inverse_mass = 0.0F;
            vertex.velocity = {};
        }
        rest_positions_[static_cast<size_t>(index)] = read_vec3(desc.rest_frame_positions, index);
        material_rest_positions_[static_cast<size_t>(index)] = read_vec3(desc.material_rest_positions, index);
        if (
            !finite(vertex.position) || !finite(vertex.velocity) || !finite(rest_positions_[static_cast<size_t>(index)]) ||
            !finite(material_rest_positions_[static_cast<size_t>(index)]) ||
            !std::isfinite(vertex.unlocked_inverse_mass) || vertex.unlocked_inverse_mass < 0.0F) {
            throw std::invalid_argument("create descriptor contains non-finite or negative vertex data");
        }
    }

    faces_.reserve(static_cast<size_t>(desc.face_count));
    for (int32_t index = 0; index < desc.face_count; ++index) {
        Face face{
            desc.faces[index * 3],
            desc.faces[index * 3 + 1],
            desc.faces[index * 3 + 2],
        };
        validate_index(face[0], desc.vertex_count, "face vertex");
        validate_index(face[1], desc.vertex_count, "face vertex");
        validate_index(face[2], desc.vertex_count, "face vertex");
        if (face[0] == face[1] || face[1] == face[2] || face[2] == face[0]) {
            throw std::invalid_argument("create descriptor contains a degenerate face");
        }
        faces_.push_back(face);
    }

    body_positions_.reserve(static_cast<size_t>(desc.body_vertex_count));
    for (int32_t index = 0; index < desc.body_vertex_count; ++index) {
        const Vec3 value = read_vec3(desc.body_positions, index);
        if (!finite(value)) {
            throw std::invalid_argument("Body contains a non-finite vertex");
        }
        body_positions_.push_back(value);
    }
    body_faces_.reserve(static_cast<size_t>(desc.body_face_count));
    for (int32_t index = 0; index < desc.body_face_count; ++index) {
        Face face{
            desc.body_faces[index * 3],
            desc.body_faces[index * 3 + 1],
            desc.body_faces[index * 3 + 2],
        };
        validate_index(face[0], desc.body_vertex_count, "Body face vertex");
        validate_index(face[1], desc.body_vertex_count, "Body face vertex");
        validate_index(face[2], desc.body_vertex_count, "Body face vertex");
        body_faces_.push_back(face);
    }

    build_segments(desc);
    build_quads(desc);
    initialize_orientations_from_geometry();
    build_angles();

    seams_.reserve(static_cast<size_t>(desc.seam_count));
    for (int32_t index = 0; index < desc.seam_count; ++index) {
        const int32_t a = desc.seams[index * 2];
        const int32_t b = desc.seams[index * 2 + 1];
        validate_index(a, desc.vertex_count, "seam vertex");
        validate_index(b, desc.vertex_count, "seam vertex");
        if (a == b) {
            throw std::invalid_argument("seam endpoints must be distinct");
        }
        seams_.push_back({a, b, length(vertices_[static_cast<size_t>(b)].position - vertices_[static_cast<size_t>(a)].position)});
    }
    require_finite_state();
}

int32_t Solver::vertex_count() const noexcept {
    return static_cast<int32_t>(vertices_.size());
}

int32_t Solver::segment_count() const noexcept {
    return static_cast<int32_t>(segments_.size());
}

int32_t Solver::angle_count() const noexcept {
    return static_cast<int32_t>(angles_.size());
}

int32_t Solver::quad_count() const noexcept {
    return static_cast<int32_t>(quads_.size());
}

int32_t Solver::seam_count() const noexcept {
    return static_cast<int32_t>(seams_.size());
}

void Solver::validate_config() const {
    if (
        !(config_.time_step > 0.0F) || config_.substeps <= 0 || config_.iterations <= 0 ||
        !(config_.stretch_stiffness > 0.0F) || config_.bend_stiffness < 0.0F ||
        config_.quad_shear_stiffness < 0.0F || config_.quad_area_stiffness < 0.0F ||
        !std::isfinite(config_.quad_shear_stiffness) || !std::isfinite(config_.quad_area_stiffness) ||
        config_.straight_pair_cosine < -1.0F || config_.straight_pair_cosine > 1.0F ||
        config_.seam_projection_passes < 0 || config_.velocity_damping_per_second < 0.0F ||
        !(config_.maximum_speed > 0.0F) || !(config_.maximum_position_correction > 0.0F) ||
        !(config_.contact_thickness > 0.0F)) {
        throw std::invalid_argument("solver configuration contains an invalid value");
    }
}

void Solver::build_segments(const ysc_create_desc& desc) {
    segments_.reserve(static_cast<size_t>(desc.edge_count));
    vertex_segments_.resize(vertices_.size());
    for (int32_t index = 0; index < desc.edge_count; ++index) {
        const int32_t a = desc.edges[index * 2];
        const int32_t b = desc.edges[index * 2 + 1];
        validate_index(a, desc.vertex_count, "edge vertex");
        validate_index(b, desc.vertex_count, "edge vertex");
        const float rest_length = desc.edge_rest_lengths[index];
        if (a == b || !(rest_length > kEpsilon) || !std::isfinite(rest_length)) {
            throw std::invalid_argument("edge has invalid endpoints or rest length");
        }
        Segment segment;
        segment.a = a;
        segment.b = b;
        segment.rest_length = rest_length;
        segment.stretch_stiffness = config_.stretch_stiffness * rest_length;
        segments_.push_back(segment);
        vertex_segments_[static_cast<size_t>(a)].push_back(index);
        vertex_segments_[static_cast<size_t>(b)].push_back(index);
    }
}

void Solver::build_quads(const ysc_create_desc& desc) {
    quads_.reserve(static_cast<size_t>(desc.quad_count));
    vertex_quads_.resize(vertices_.size());
    for (int32_t index = 0; index < desc.quad_count; ++index) {
        Quad quad;
        for (int32_t corner = 0; corner < 4; ++corner) {
            quad.vertices[static_cast<size_t>(corner)] = desc.quads[index * 4 + corner];
            validate_index(
                quad.vertices[static_cast<size_t>(corner)], desc.vertex_count, "quad vertex");
        }
        std::set<int32_t> unique(quad.vertices.begin(), quad.vertices.end());
        if (unique.size() != 4) {
            throw std::invalid_argument("quad vertices must be distinct");
        }

        const Vec3& p0 = material_rest_positions_[static_cast<size_t>(quad.vertices[0])];
        const Vec3& p1 = material_rest_positions_[static_cast<size_t>(quad.vertices[1])];
        const Vec3& p2 = material_rest_positions_[static_cast<size_t>(quad.vertices[2])];
        const Vec3& p3 = material_rest_positions_[static_cast<size_t>(quad.vertices[3])];
        const Vec3 u = 0.5F * ((p1 - p0) + (p2 - p3));
        const Vec3 v = 0.5F * ((p3 - p0) + (p2 - p1));
        const Vec3 normal = cross(u, v);
        quad.rest_product = length(u) * length(v);
        quad.rest_area = length(normal);
        if (
            !(quad.rest_product > kEpsilon) || !(quad.rest_area > kEpsilon) ||
            !std::isfinite(quad.rest_product) || !std::isfinite(quad.rest_area)) {
            throw std::invalid_argument("quad has a degenerate material rest shape");
        }
        quad.rest_shear = dot(u, v) / quad.rest_product;
        quad.rest_normal = normal / quad.rest_area;
        quad.shear_stiffness = config_.quad_shear_stiffness * quad.rest_area;
        quad.area_stiffness = config_.quad_area_stiffness * quad.rest_area;
        if (!std::isfinite(quad.rest_shear) || !finite(quad.rest_normal)) {
            throw std::invalid_argument("quad has invalid material rest data");
        }

        const int32_t quad_index = static_cast<int32_t>(quads_.size());
        quads_.push_back(quad);
        for (const int32_t vertex : quad.vertices) {
            vertex_quads_[static_cast<size_t>(vertex)].push_back(quad_index);
        }
    }
}

std::vector<Vec3> Solver::geometry_vertex_normals(const std::vector<Vec3>& positions) const {
    std::vector<Vec3> normals(positions.size(), Vec3{});
    for (const Face& face : faces_) {
        const Vec3& a = positions[static_cast<size_t>(face[0])];
        const Vec3& b = positions[static_cast<size_t>(face[1])];
        const Vec3& c = positions[static_cast<size_t>(face[2])];
        const Vec3 normal = cross(b - a, c - a);
        if (length_squared(normal) <= 1.0e-16F) {
            continue;
        }
        normals[static_cast<size_t>(face[0])] += normal;
        normals[static_cast<size_t>(face[1])] += normal;
        normals[static_cast<size_t>(face[2])] += normal;
    }
    for (Vec3& normal : normals) {
        normal = normalized(normal, {0.0F, 1.0F, 0.0F});
    }
    return normals;
}

void Solver::initialize_orientations_from_geometry() {
    std::vector<Vec3> current_positions;
    current_positions.reserve(vertices_.size());
    for (const Vertex& vertex : vertices_) {
        current_positions.push_back(vertex.position);
    }
    const std::vector<Vec3> rest_normals = geometry_vertex_normals(rest_positions_);
    const std::vector<Vec3> current_normals = geometry_vertex_normals(current_positions);
    for (Segment& segment : segments_) {
        const Vec3 rest_tangent = rest_positions_[static_cast<size_t>(segment.b)] - rest_positions_[static_cast<size_t>(segment.a)];
        const Vec3 rest_normal = rest_normals[static_cast<size_t>(segment.a)] + rest_normals[static_cast<size_t>(segment.b)];
        segment.rest_orientation = frame_from_tangent_normal(rest_tangent, rest_normal);

        const Vec3 current_tangent = vertices_[static_cast<size_t>(segment.b)].position - vertices_[static_cast<size_t>(segment.a)].position;
        const Vec3 current_normal = current_normals[static_cast<size_t>(segment.a)] + current_normals[static_cast<size_t>(segment.b)];
        segment.orientation = frame_from_tangent_normal(current_tangent, current_normal);
    }
}

void Solver::build_angles() {
    angles_.clear();
    segment_angles_.clear();
    segment_angles_.resize(segments_.size());
    std::set<std::pair<int32_t, int32_t>> accepted;

    for (int32_t vertex_index = 0; vertex_index < vertex_count(); ++vertex_index) {
        const std::vector<int32_t>& incident = vertex_segments_[static_cast<size_t>(vertex_index)];
        if (incident.size() < 2) {
            continue;
        }
        std::vector<int32_t> best(incident.size(), -1);
        std::vector<float> best_dot(incident.size(), std::numeric_limits<float>::infinity());
        for (size_t left_index = 0; left_index < incident.size(); ++left_index) {
            const Segment& left = segments_[static_cast<size_t>(incident[left_index])];
            const int32_t left_other = left.a == vertex_index ? left.b : left.a;
            const Vec3 left_direction = normalized(
                rest_positions_[static_cast<size_t>(left_other)] - rest_positions_[static_cast<size_t>(vertex_index)]);
            for (size_t right_index = 0; right_index < incident.size(); ++right_index) {
                if (left_index == right_index) {
                    continue;
                }
                const Segment& right = segments_[static_cast<size_t>(incident[right_index])];
                const int32_t right_other = right.a == vertex_index ? right.b : right.a;
                const Vec3 right_direction = normalized(
                    rest_positions_[static_cast<size_t>(right_other)] - rest_positions_[static_cast<size_t>(vertex_index)]);
                const float alignment = dot(left_direction, right_direction);
                if (alignment < best_dot[left_index]) {
                    best_dot[left_index] = alignment;
                    best[left_index] = static_cast<int32_t>(right_index);
                }
            }
        }

        for (size_t left_index = 0; left_index < incident.size(); ++left_index) {
            const int32_t right_index = best[left_index];
            if (
                right_index < 0 || best_dot[left_index] > config_.straight_pair_cosine ||
                best[static_cast<size_t>(right_index)] != static_cast<int32_t>(left_index)) {
                continue;
            }
            int32_t first = incident[left_index];
            int32_t second = incident[static_cast<size_t>(right_index)];
            if (first > second) {
                std::swap(first, second);
            }
            if (!accepted.emplace(first, second).second) {
                continue;
            }
            const Segment& first_segment = segments_[static_cast<size_t>(first)];
            const Segment& second_segment = segments_[static_cast<size_t>(second)];
            const float average_length = 0.5F * (first_segment.rest_length + second_segment.rest_length);
            Angle angle;
            angle.a = first;
            angle.b = second;
            angle.bend_stiffness = average_length > kEpsilon ? 4.0F * config_.bend_stiffness / average_length : 0.0F;
            angle.rest_relative = normalized(conjugate(first_segment.rest_orientation) * second_segment.rest_orientation);
            const int32_t angle_index = static_cast<int32_t>(angles_.size());
            angles_.push_back(angle);
            segment_angles_[static_cast<size_t>(first)].push_back(angle_index);
            segment_angles_[static_cast<size_t>(second)].push_back(angle_index);
        }
    }
}

void Solver::replace_state(
    const float* positions,
    const float* velocities,
    const int32_t* locked,
    bool reinitialize_orientations) {
    if (positions == nullptr || velocities == nullptr || locked == nullptr) {
        throw std::invalid_argument("replacement state pointer is null");
    }
    for (int32_t index = 0; index < vertex_count(); ++index) {
        Vertex& vertex = vertices_[static_cast<size_t>(index)];
        vertex.position = read_vec3(positions, index);
        vertex.previous = vertex.position;
        vertex.predicted = vertex.position;
        vertex.velocity = read_vec3(velocities, index);
        vertex.locked = locked[index] != 0;
        vertex.inverse_mass = vertex.locked ? 0.0F : vertex.unlocked_inverse_mass;
        if (vertex.locked) {
            vertex.velocity = {};
        }
    }
    require_finite_state();
    if (reinitialize_orientations) {
        initialize_orientations_from_geometry();
    }
}

void Solver::copy_state(float* positions, float* velocities) const {
    if (positions == nullptr || velocities == nullptr) {
        throw std::invalid_argument("state output pointer is null");
    }
    for (int32_t index = 0; index < vertex_count(); ++index) {
        const Vertex& vertex = vertices_[static_cast<size_t>(index)];
        write_vec3(positions, index, vertex.position);
        write_vec3(velocities, index, vertex.velocity);
    }
}

void Solver::replace_orientations(const float* quaternions_wxyz) {
    if (quaternions_wxyz == nullptr) {
        throw std::invalid_argument("orientation input pointer is null");
    }
    for (int32_t index = 0; index < segment_count(); ++index) {
        const Quat value = read_quat(quaternions_wxyz, index);
        if (!finite(value) || length(value) <= kEpsilon) {
            throw std::invalid_argument("orientation input contains an invalid quaternion");
        }
        segments_[static_cast<size_t>(index)].orientation = normalized(value);
    }
}

void Solver::copy_orientations(float* quaternions_wxyz) const {
    if (quaternions_wxyz == nullptr) {
        throw std::invalid_argument("orientation output pointer is null");
    }
    for (int32_t index = 0; index < segment_count(); ++index) {
        write_quat(quaternions_wxyz, index, segments_[static_cast<size_t>(index)].orientation);
    }
}

void Solver::replace_seam_state(const float* maximum_lengths) {
    if (maximum_lengths == nullptr && !seams_.empty()) {
        throw std::invalid_argument("seam state input pointer is null");
    }
    for (size_t index = 0; index < seams_.size(); ++index) {
        const float value = maximum_lengths[index];
        if (value < 0.0F || !std::isfinite(value)) {
            throw std::invalid_argument("seam state contains an invalid length");
        }
        seams_[index].maximum_length = value;
    }
}

void Solver::copy_seam_state(float* maximum_lengths) const {
    if (maximum_lengths == nullptr && !seams_.empty()) {
        throw std::invalid_argument("seam state output pointer is null");
    }
    for (size_t index = 0; index < seams_.size(); ++index) {
        maximum_lengths[index] = seams_[index].maximum_length;
    }
}

void Solver::predict(const Vec3& gravity) {
    const float time_step = config_.time_step;
    for (Vertex& vertex : vertices_) {
        vertex.previous = vertex.position;
        if (vertex.locked || vertex.inverse_mass <= 0.0F) {
            vertex.velocity = {};
            vertex.predicted = vertex.position;
            continue;
        }
        vertex.predicted = vertex.position + time_step * vertex.velocity + (time_step * time_step) * gravity;
        vertex.position = vertex.predicted;
    }
}

void Solver::position_sweep(float time_step) {
    static constexpr std::array<float, 4> kWeftCoefficient{-0.5F, 0.5F, 0.5F, -0.5F};
    static constexpr std::array<float, 4> kWarpCoefficient{-0.5F, -0.5F, 0.5F, 0.5F};
    const float inverse_h_squared = 1.0F / (time_step * time_step);
    for (int32_t vertex_index = 0; vertex_index < vertex_count(); ++vertex_index) {
        Vertex& vertex = vertices_[static_cast<size_t>(vertex_index)];
        if (vertex.locked || vertex.inverse_mass <= 0.0F) {
            continue;
        }
        const float mass = 1.0F / vertex.inverse_mass;
        Vec3 gradient = (mass * inverse_h_squared) * (vertex.position - vertex.predicted);
        float hessian = mass * inverse_h_squared;
        for (const int32_t segment_index : vertex_segments_[static_cast<size_t>(vertex_index)]) {
            const Segment& segment = segments_[static_cast<size_t>(segment_index)];
            const Vec3 difference =
                vertices_[static_cast<size_t>(segment.b)].position - vertices_[static_cast<size_t>(segment.a)].position;
            const Vec3 director = rotate(segment.orientation, {0.0F, 0.0F, 1.0F});
            const Vec3 constraint = difference / segment.rest_length - director;
            const float gradient_scale = segment.stretch_stiffness / segment.rest_length;
            gradient += (vertex_index == segment.a ? -gradient_scale : gradient_scale) * constraint;
            hessian += segment.stretch_stiffness / (segment.rest_length * segment.rest_length);
        }
        for (const int32_t quad_index : vertex_quads_[static_cast<size_t>(vertex_index)]) {
            const Quad& quad = quads_[static_cast<size_t>(quad_index)];
            const auto corner_iterator = std::find(
                quad.vertices.begin(), quad.vertices.end(), vertex_index);
            if (corner_iterator == quad.vertices.end()) {
                throw std::runtime_error("quad adjacency is inconsistent");
            }
            const size_t corner = static_cast<size_t>(corner_iterator - quad.vertices.begin());
            const Vec3& p0 = vertices_[static_cast<size_t>(quad.vertices[0])].position;
            const Vec3& p1 = vertices_[static_cast<size_t>(quad.vertices[1])].position;
            const Vec3& p2 = vertices_[static_cast<size_t>(quad.vertices[2])].position;
            const Vec3& p3 = vertices_[static_cast<size_t>(quad.vertices[3])].position;
            const Vec3 weft = 0.5F * ((p1 - p0) + (p2 - p3));
            const Vec3 warp = 0.5F * ((p3 - p0) + (p2 - p1));
            const float weft_coefficient = kWeftCoefficient[corner];
            const float warp_coefficient = kWarpCoefficient[corner];

            if (quad.shear_stiffness > 0.0F) {
                const float constraint = dot(weft, warp) / quad.rest_product - quad.rest_shear;
                const Vec3 constraint_gradient =
                    (weft_coefficient * warp + warp_coefficient * weft) / quad.rest_product;
                gradient += (quad.shear_stiffness * constraint) * constraint_gradient;
                hessian += quad.shear_stiffness * length_squared(constraint_gradient);
            }

            if (quad.area_stiffness > 0.0F) {
                const Vec3 area_vector = cross(weft, warp);
                const float current_area = length(area_vector);
                const Vec3 normal = normalized(area_vector, quad.rest_normal);
                const float constraint = current_area / quad.rest_area - 1.0F;
                const Vec3 weft_gradient = cross(warp, normal) / quad.rest_area;
                const Vec3 warp_gradient = cross(normal, weft) / quad.rest_area;
                const Vec3 constraint_gradient =
                    weft_coefficient * weft_gradient + warp_coefficient * warp_gradient;
                gradient += (quad.area_stiffness * constraint) * constraint_gradient;
                hessian += quad.area_stiffness * length_squared(constraint_gradient);
            }
        }
        if (!(hessian > kEpsilon) || !std::isfinite(hessian)) {
            throw std::runtime_error("local VBD Hessian is invalid");
        }
        Vec3 correction = (-1.0F / hessian) * gradient;
        correction = clamp_length(correction, config_.maximum_position_correction);
        vertex.position += correction;
    }
}

void Solver::orientation_sweep() {
    const Quat material_axis = pure({0.0F, 0.0F, 1.0F});
    for (int32_t segment_index = 0; segment_index < segment_count(); ++segment_index) {
        Segment& segment = segments_[static_cast<size_t>(segment_index)];
        const Vec3 difference =
            vertices_[static_cast<size_t>(segment.b)].position - vertices_[static_cast<size_t>(segment.a)].position;
        const Vec3 v = (-2.0F * segment.stretch_stiffness / segment.rest_length) * difference;
        Quat b{0.0F, 0.0F, 0.0F, 0.0F};
        for (const int32_t angle_index : segment_angles_[static_cast<size_t>(segment_index)]) {
            const Angle& angle = angles_[static_cast<size_t>(angle_index)];
            const Segment& first = segments_[static_cast<size_t>(angle.a)];
            const Segment& second = segments_[static_cast<size_t>(angle.b)];
            const Quat current_relative = normalized(conjugate(first.orientation) * second.orientation);
            const float phi = dot(current_relative, angle.rest_relative) >= 0.0F ? 1.0F : -1.0F;
            if (segment_index == angle.a) {
                b += (angle.bend_stiffness * phi) * (second.orientation * conjugate(angle.rest_relative));
            } else {
                b += (angle.bend_stiffness * phi) * (first.orientation * angle.rest_relative);
            }
        }

        Quat updated;
        if (length(b) <= kEpsilon) {
            const Vec3 tangent = normalized(difference, rotate(segment.orientation, {0.0F, 0.0F, 1.0F}));
            const Vec3 old_director = rotate(segment.orientation, {0.0F, 0.0F, 1.0F});
            updated = normalized(from_to(old_director, tangent) * segment.orientation);
        } else {
            const float lambda = length(v) + length(b);
            const Quat numerator = pure(v) * b * material_axis + lambda * b;
            updated = normalized(numerator, segment.orientation);
        }
        if (dot(updated, segment.orientation) < 0.0F) {
            updated *= -1.0F;
        }
        segment.orientation = updated;
    }
}

Vec3 Solver::closest_triangle_point(
    const Vec3& point,
    const Vec3& a,
    const Vec3& b,
    const Vec3& c) const {
    const Vec3 ab = b - a;
    const Vec3 ac = c - a;
    const Vec3 ap = point - a;
    const float d1 = dot(ab, ap);
    const float d2 = dot(ac, ap);
    if (d1 <= 0.0F && d2 <= 0.0F) {
        return a;
    }
    const Vec3 bp = point - b;
    const float d3 = dot(ab, bp);
    const float d4 = dot(ac, bp);
    if (d3 >= 0.0F && d4 <= d3) {
        return b;
    }
    const float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0F && d1 >= 0.0F && d3 <= 0.0F) {
        const float value = d1 / (d1 - d3);
        return a + value * ab;
    }
    const Vec3 cp = point - c;
    const float d5 = dot(ab, cp);
    const float d6 = dot(ac, cp);
    if (d6 >= 0.0F && d5 <= d6) {
        return c;
    }
    const float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0F && d2 >= 0.0F && d6 <= 0.0F) {
        const float value = d2 / (d2 - d6);
        return a + value * ac;
    }
    const float va = d3 * d6 - d5 * d4;
    if (va <= 0.0F && (d4 - d3) >= 0.0F && (d5 - d6) >= 0.0F) {
        const float value = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return b + value * (c - b);
    }
    const float denominator = 1.0F / (va + vb + vc);
    const float v = vb * denominator;
    const float w = vc * denominator;
    return a + v * ab + w * ac;
}

void Solver::project_body_contacts(const int32_t* candidates, int32_t count) {
    if (count <= 0) {
        return;
    }
    if (candidates == nullptr) {
        throw std::invalid_argument("Body candidate pointer is null");
    }
    std::vector<Vec3> corrections(vertices_.size(), Vec3{});
    std::vector<int32_t> correction_counts(vertices_.size(), 0);
    for (int32_t index = 0; index < count; ++index) {
        const int32_t vertex_index = candidates[index * 2];
        const int32_t face_index = candidates[index * 2 + 1];
        validate_index(vertex_index, vertex_count(), "Body candidate vertex");
        validate_index(face_index, static_cast<int32_t>(body_faces_.size()), "Body candidate face");
        Vertex& vertex = vertices_[static_cast<size_t>(vertex_index)];
        if (vertex.locked) {
            continue;
        }
        const Face& face = body_faces_[static_cast<size_t>(face_index)];
        const Vec3& a = body_positions_[static_cast<size_t>(face[0])];
        const Vec3& b = body_positions_[static_cast<size_t>(face[1])];
        const Vec3& c = body_positions_[static_cast<size_t>(face[2])];
        const Vec3 normal = normalized(cross(b - a, c - a));
        const Vec3 closest = closest_triangle_point(vertex.position, a, b, c);
        const Vec3 separation = vertex.position - closest;
        const float signed_distance = dot(separation, normal);
        // Python only supplies nearby vertices or vertices confirmed inside a
        // closed Body.  Do not discard a deep penetration by Euclidean depth:
        // the capped correction below lets VBD resolve it without tearing the
        // local edge network in a single projection.
        if (signed_distance < config_.contact_thickness) {
            corrections[static_cast<size_t>(vertex_index)] +=
                normal * (config_.contact_thickness - signed_distance);
            ++correction_counts[static_cast<size_t>(vertex_index)];
        }
    }
    for (size_t index = 0; index < vertices_.size(); ++index) {
        if (correction_counts[index] > 0) {
            Vec3 correction = corrections[index] / static_cast<float>(correction_counts[index]);
            // Contact is intentionally continued over several Kitsuke clicks.
            // Applying the full VBD correction cap on every inner iteration can
            // pull a panel through an entire torso-sized depth in one click and
            // locally tear a fine triangulation before strain can redistribute.
            correction = clamp_length(correction, config_.maximum_position_correction * 0.1F);
            vertices_[index].position += correction;
            vertices_[index].predicted += correction;
        }
    }
}

void Solver::project_self_contacts(const int32_t* candidates, int32_t count) {
    if (count <= 0) {
        return;
    }
    if (candidates == nullptr) {
        throw std::invalid_argument("self-contact candidate pointer is null");
    }
    std::vector<Vec3> corrections(vertices_.size(), Vec3{});
    std::vector<int32_t> correction_counts(vertices_.size(), 0);
    for (int32_t index = 0; index < count; ++index) {
        const int32_t vertex_index = candidates[index * 2];
        const int32_t face_index = candidates[index * 2 + 1];
        validate_index(vertex_index, vertex_count(), "self-contact candidate vertex");
        validate_index(face_index, static_cast<int32_t>(faces_.size()), "self-contact candidate face");
        Vertex& vertex = vertices_[static_cast<size_t>(vertex_index)];
        if (vertex.locked) {
            continue;
        }
        const Face& face = faces_[static_cast<size_t>(face_index)];
        const Vec3& a = vertices_[static_cast<size_t>(face[0])].position;
        const Vec3& b = vertices_[static_cast<size_t>(face[1])].position;
        const Vec3& c = vertices_[static_cast<size_t>(face[2])].position;
        Vec3 normal;
        float signed_distance = 0.0F;
        if (project_to_triangle_interior(vertex.position, a, b, c, normal, signed_distance)) {
            const float distance = std::abs(signed_distance);
            if (distance >= config_.contact_thickness) {
                continue;
            }
            const Vec3 direction = signed_distance >= 0.0F ? normal : -normal;
            corrections[static_cast<size_t>(vertex_index)] +=
                direction * (config_.contact_thickness - distance);
            ++correction_counts[static_cast<size_t>(vertex_index)];
        }
    }
    for (size_t index = 0; index < vertices_.size(); ++index) {
        if (correction_counts[index] > 0) {
            Vec3 correction = corrections[index] / static_cast<float>(correction_counts[index]);
            correction = clamp_length(correction, config_.maximum_position_correction);
            vertices_[index].position += correction;
            vertices_[index].predicted += correction;
        }
    }
}

void Solver::project_seams() {
    std::vector<Vec3> corrections(vertices_.size(), Vec3{});
    std::vector<int32_t> correction_counts(vertices_.size(), 0);
    for (const Seam& seam : seams_) {
        Vertex& a = vertices_[static_cast<size_t>(seam.a)];
        Vertex& b = vertices_[static_cast<size_t>(seam.b)];
        const Vec3 difference = b.position - a.position;
        const float distance = length(difference);
        if (!(distance > seam.maximum_length) || distance <= kEpsilon) {
            continue;
        }
        const int32_t unlocked = static_cast<int32_t>(!a.locked) + static_cast<int32_t>(!b.locked);
        if (unlocked == 0) {
            continue;
        }
        const Vec3 correction = difference * (((distance - seam.maximum_length) / static_cast<float>(unlocked)) / distance);
        if (!a.locked) {
            corrections[static_cast<size_t>(seam.a)] += correction;
            ++correction_counts[static_cast<size_t>(seam.a)];
        }
        if (!b.locked) {
            corrections[static_cast<size_t>(seam.b)] -= correction;
            ++correction_counts[static_cast<size_t>(seam.b)];
        }
    }
    for (size_t index = 0; index < vertices_.size(); ++index) {
        if (correction_counts[index] > 0) {
            vertices_[index].position += corrections[index] / static_cast<float>(correction_counts[index]);
        }
    }
}

void Solver::ratchet_seams() {
    for (Seam& seam : seams_) {
        const float distance = length(
            vertices_[static_cast<size_t>(seam.b)].position - vertices_[static_cast<size_t>(seam.a)].position);
        seam.maximum_length = std::min(seam.maximum_length, distance);
    }
}

void Solver::finish_substep(float time_step) {
    const float damping = std::exp(-config_.velocity_damping_per_second * time_step);
    for (Vertex& vertex : vertices_) {
        if (vertex.locked || vertex.inverse_mass <= 0.0F) {
            vertex.velocity = {};
            continue;
        }
        Vec3 velocity = (vertex.position - vertex.previous) / time_step;
        velocity = clamp_length(velocity, config_.maximum_speed);
        vertex.velocity = damping * velocity;
    }
}

float Solver::maximum_edge_strain() const {
    float result = 0.0F;
    for (const Segment& segment : segments_) {
        const float current_length = length(
            vertices_[static_cast<size_t>(segment.b)].position - vertices_[static_cast<size_t>(segment.a)].position);
        result = std::max(result, std::abs(current_length / segment.rest_length - 1.0F));
    }
    return result;
}

void Solver::compute_energy(float& stretch, float& bend, float& shear, float& area) const {
    stretch = 0.0F;
    bend = 0.0F;
    shear = 0.0F;
    area = 0.0F;
    for (const Segment& segment : segments_) {
        const Vec3 difference =
            vertices_[static_cast<size_t>(segment.b)].position - vertices_[static_cast<size_t>(segment.a)].position;
        const Vec3 constraint =
            difference / segment.rest_length - rotate(segment.orientation, {0.0F, 0.0F, 1.0F});
        stretch += 0.5F * segment.stretch_stiffness * length_squared(constraint);
    }
    for (const Angle& angle : angles_) {
        const Quat relative = normalized(
            conjugate(segments_[static_cast<size_t>(angle.a)].orientation) *
            segments_[static_cast<size_t>(angle.b)].orientation);
        const float phi = dot(relative, angle.rest_relative) >= 0.0F ? 1.0F : -1.0F;
        bend += 0.5F * angle.bend_stiffness * quaternion_distance_squared(relative, phi * angle.rest_relative);
    }
    for (const Quad& quad : quads_) {
        const Vec3& p0 = vertices_[static_cast<size_t>(quad.vertices[0])].position;
        const Vec3& p1 = vertices_[static_cast<size_t>(quad.vertices[1])].position;
        const Vec3& p2 = vertices_[static_cast<size_t>(quad.vertices[2])].position;
        const Vec3& p3 = vertices_[static_cast<size_t>(quad.vertices[3])].position;
        const Vec3 weft = 0.5F * ((p1 - p0) + (p2 - p3));
        const Vec3 warp = 0.5F * ((p3 - p0) + (p2 - p1));
        const float shear_constraint = dot(weft, warp) / quad.rest_product - quad.rest_shear;
        const float area_constraint = length(cross(weft, warp)) / quad.rest_area - 1.0F;
        shear += 0.5F * quad.shear_stiffness * shear_constraint * shear_constraint;
        area += 0.5F * quad.area_stiffness * area_constraint * area_constraint;
    }
}

void Solver::require_finite_state() const {
    for (const Vertex& vertex : vertices_) {
        if (!finite(vertex.position) || !finite(vertex.velocity)) {
            throw std::runtime_error("solver state contains a non-finite vertex");
        }
    }
    for (const Segment& segment : segments_) {
        if (!finite(segment.orientation) || std::abs(length(segment.orientation) - 1.0F) > 2.0e-3F) {
            throw std::runtime_error("solver state contains an invalid orientation");
        }
    }
    for (const Seam& seam : seams_) {
        if (!std::isfinite(seam.maximum_length) || seam.maximum_length < 0.0F) {
            throw std::runtime_error("solver state contains an invalid seam length");
        }
    }
}

ysc_stats Solver::advance(const ysc_advance_desc& desc) {
    if (
        !std::isfinite(desc.gravity[0]) || !std::isfinite(desc.gravity[1]) || !std::isfinite(desc.gravity[2]) ||
        !std::isfinite(desc.seam_closure) || desc.seam_closure < 0.0F ||
        desc.body_candidate_count < 0 || desc.self_candidate_count < 0) {
        throw std::invalid_argument("advance descriptor contains an invalid value");
    }
    if (desc.body_candidate_count > 0 && desc.body_candidates == nullptr) {
        throw std::invalid_argument("advance descriptor has no Body candidates");
    }
    if (desc.self_candidate_count > 0 && desc.self_candidates == nullptr) {
        throw std::invalid_argument("advance descriptor has no self-contact candidates");
    }
    const int32_t iterations = desc.iterations > 0 ? desc.iterations : config_.iterations;
    const Vec3 gravity{desc.gravity[0], desc.gravity[1], desc.gravity[2]};
    std::vector<Vec3> click_start;
    click_start.reserve(vertices_.size());
    for (const Vertex& vertex : vertices_) {
        click_start.push_back(vertex.position);
    }

    const float seam_closure_per_substep = desc.seam_closure / static_cast<float>(config_.substeps);
    for (int32_t substep = 0; substep < config_.substeps; ++substep) {
        for (Seam& seam : seams_) {
            seam.maximum_length = std::max(0.0F, seam.maximum_length - seam_closure_per_substep);
        }
        ratchet_seams();
        predict(gravity);
        ratchet_seams();
        for (int32_t iteration = 0; iteration < iterations; ++iteration) {
            project_body_contacts(desc.body_candidates, desc.body_candidate_count);
            project_self_contacts(desc.self_candidates, desc.self_candidate_count);
            for (int32_t pass = 0; pass < config_.seam_projection_passes; ++pass) {
                project_seams();
            }
            // Finish every alternating iteration with the smooth material
            // solve.  Leaving a local seam/contact projection as the final
            // operation creates a visible, high-strain spike at fine edges.
            position_sweep(config_.time_step);
            orientation_sweep();
            ratchet_seams();
        }
        // A few constraint-free sweeps propagate the last local projections
        // through the rod graph instead of exporting their strain at the seam
        // or Body boundary.  The ratcheted targets remain active next substep.
        for (int32_t pass = 0; pass < config_.seam_projection_passes; ++pass) {
            position_sweep(config_.time_step);
            orientation_sweep();
        }
        finish_substep(config_.time_step);
        require_finite_state();
    }

    ysc_stats stats{};
    stats.substeps = config_.substeps;
    stats.iterations = iterations;
    stats.segment_count = segment_count();
    stats.angle_count = angle_count();
    stats.quad_count = quad_count();
    stats.body_candidate_count = desc.body_candidate_count;
    stats.self_candidate_count = desc.self_candidate_count;
    for (size_t index = 0; index < vertices_.size(); ++index) {
        stats.maximum_displacement = std::max(
            stats.maximum_displacement,
            length(vertices_[index].position - click_start[index]));
    }
    stats.maximum_edge_strain = maximum_edge_strain();
    compute_energy(stats.stretch_energy, stats.bend_energy, stats.shear_energy, stats.area_energy);
    return stats;
}

}  // namespace ysc
