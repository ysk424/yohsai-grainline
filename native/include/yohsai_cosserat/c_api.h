// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <stdint.h>

#if defined(_WIN32)
#  if defined(YSC_BUILD_DLL)
#    define YSC_API __declspec(dllexport)
#  else
#    define YSC_API __declspec(dllimport)
#  endif
#else
#  define YSC_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define YSC_API_VERSION 2

typedef void* ysc_handle;

typedef enum ysc_status {
    YSC_STATUS_OK = 0,
    YSC_STATUS_INVALID_ARGUMENT = 1,
    YSC_STATUS_OUT_OF_RANGE = 2,
    YSC_STATUS_NONFINITE_STATE = 3,
    YSC_STATUS_INTERNAL_ERROR = 4
} ysc_status;

typedef struct ysc_config {
    float time_step;
    int32_t substeps;
    int32_t iterations;
    float stretch_stiffness;
    float bend_stiffness;
    float quad_shear_stiffness;
    float quad_area_stiffness;
    float straight_pair_cosine;
    int32_t seam_projection_passes;
    float velocity_damping_per_second;
    float maximum_speed;
    float maximum_position_correction;
    float contact_thickness;
} ysc_config;

typedef struct ysc_create_desc {
    int32_t vertex_count;
    const float* positions;
    const float* velocities;
    const float* rest_frame_positions;
    const float* material_rest_positions;
    const float* inverse_masses;
    const int32_t* locked;

    int32_t edge_count;
    const int32_t* edges;
    const float* edge_rest_lengths;

    int32_t quad_count;
    const int32_t* quads;

    int32_t seam_count;
    const int32_t* seams;

    int32_t face_count;
    const int32_t* faces;

    int32_t body_vertex_count;
    const float* body_positions;
    int32_t body_face_count;
    const int32_t* body_faces;
} ysc_create_desc;

typedef struct ysc_advance_desc {
    float gravity[3];
    float seam_closure;
    int32_t iterations;

    int32_t body_candidate_count;
    const int32_t* body_candidates;
    int32_t self_candidate_count;
    const int32_t* self_candidates;
} ysc_advance_desc;

typedef struct ysc_stats {
    int32_t substeps;
    int32_t iterations;
    int32_t segment_count;
    int32_t angle_count;
    int32_t quad_count;
    int32_t body_candidate_count;
    int32_t self_candidate_count;
    float maximum_displacement;
    float maximum_edge_strain;
    float stretch_energy;
    float bend_energy;
    float shear_energy;
    float area_energy;
} ysc_stats;

YSC_API int32_t ysc_get_api_version(void);
YSC_API ysc_status ysc_default_config(ysc_config* out_config);

YSC_API ysc_status ysc_create(
    const ysc_create_desc* desc,
    const ysc_config* config,
    ysc_handle* out_handle,
    char* error_message,
    int32_t error_capacity);

YSC_API void ysc_destroy(ysc_handle handle);

YSC_API ysc_status ysc_get_counts(
    ysc_handle handle,
    int32_t* vertex_count,
    int32_t* segment_count,
    int32_t* angle_count,
    int32_t* quad_count,
    int32_t* seam_count,
    char* error_message,
    int32_t error_capacity);

YSC_API ysc_status ysc_replace_state(
    ysc_handle handle,
    const float* positions,
    const float* velocities,
    const int32_t* locked,
    int32_t reinitialize_orientations,
    char* error_message,
    int32_t error_capacity);

YSC_API ysc_status ysc_copy_state(
    ysc_handle handle,
    float* positions,
    float* velocities,
    char* error_message,
    int32_t error_capacity);

YSC_API ysc_status ysc_replace_orientations(
    ysc_handle handle,
    const float* quaternions_wxyz,
    char* error_message,
    int32_t error_capacity);

YSC_API ysc_status ysc_copy_orientations(
    ysc_handle handle,
    float* quaternions_wxyz,
    char* error_message,
    int32_t error_capacity);

YSC_API ysc_status ysc_replace_seam_state(
    ysc_handle handle,
    const float* seam_maximum_lengths,
    char* error_message,
    int32_t error_capacity);

YSC_API ysc_status ysc_copy_seam_state(
    ysc_handle handle,
    float* seam_maximum_lengths,
    char* error_message,
    int32_t error_capacity);

YSC_API ysc_status ysc_advance(
    ysc_handle handle,
    const ysc_advance_desc* desc,
    ysc_stats* out_stats,
    char* error_message,
    int32_t error_capacity);

#ifdef __cplusplus
}
#endif
