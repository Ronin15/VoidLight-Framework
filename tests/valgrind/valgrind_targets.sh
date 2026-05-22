#!/bin/bash

# Shared Valgrind target manifest for VoidLight-Framework.
# Keep these names aligned with `ctest --test-dir build -N`.

VALGRIND_MEMCHECK_EDM_LIFECYCLE_TARGET_SPECS=(
    "edm_lifecycle_init|entity_data_manager_tests|LifecycleTests/TestInitialization"
    "edm_lifecycle_double_init|entity_data_manager_tests|LifecycleTests/TestDoubleInitialization"
    "edm_lifecycle_clean_reinit|entity_data_manager_tests|LifecycleTests/TestCleanAndReinit"
    "edm_lifecycle_prepare_transition|entity_data_manager_tests|LifecycleTests/TestPrepareForStateTransition"
    "edm_lifecycle_behavior_config_reuse|entity_data_manager_tests|LifecycleTests/TestDirectDestroyClearsBehaviorConfigForSlotReuse"
    "edm_lifecycle_behavior_pools|entity_data_manager_tests|LifecycleTests/TestStateTransitionClearsBehaviorStatePools"
)

VALGRIND_MEMCHECK_EDM_TARGET_SPECS=(
    "${VALGRIND_MEMCHECK_EDM_LIFECYCLE_TARGET_SPECS[@]}"
    "edm_destruction_queue|entity_data_manager_tests|DestructionQueueTests/*"
    "edm_slot_reuse|entity_data_manager_tests|SlotReuseTests/TestSlotReuseAfterDestruction"
    "edm_transition_cache_clear|entity_data_manager_tests|StateTransitionCachedIndicesTests/TestPrepareForStateTransitionClearsKindIndices"
    "edm_inventory_transfer|entity_data_manager_tests|NPCRenderDataTests/TestInventoryTransferMovesFullQuantityAtomically"
)

VALGRIND_MEMCHECK_TARGETS=(
    sparse_sidecar_tests
    knockback_sidecar_tests
    entity_state_manager_tests
    npc_memory_tests
    resource_factory_tests
    resource_template_manager_tests
    resource_template_manager_json_tests
    resource_integration_tests
    world_resource_manager_tests
    world_manager_tests
    world_manager_event_integration_tests
    event_manager_tests
    event_manager_behavior_tests
    projectile_manager_tests
    collision_manager_edm_integration_tests
    pathfinder_manager_edm_integration_tests
    manager_runtime_tests
    loading_state_tests
    background_simulation_manager_tests
)

VALGRIND_MEMCHECK_EXTENDED_TARGETS=(
    save_manager_tests
    settings_manager_tests
    game_state_manager_tests
    resource_edge_case_tests
    resource_architecture_tests
    pathfinder_ai_contention_tests
    collision_pathfinding_integration_tests
    "${VALGRIND_MEMCHECK_EDM_TARGET_SPECS[@]}"
)

VALGRIND_CACHE_TARGETS=(
    buffer_utilization_tests
    buffer_reuse_tests
    simd_correctness_tests
    ai_optimization_tests
    crowd_runtime_tests
    ai_manager_edm_integration_tests
    event_manager_tests
    event_coordination_integration_tests
    particle_manager_performance_tests
    projectile_manager_tests
    collision_system_tests
    collision_manager_edm_integration_tests
    pathfinding_system_tests
    pathfinder_manager_tests
    pathfinder_ai_contention_tests
    world_resource_manager_tests
    resource_template_manager_json_tests
    sparse_sidecar_tests
    knockback_sidecar_tests
)

VALGRIND_CACHE_BENCHMARK_TARGETS=(
    ai_scaling_benchmark
    collision_scaling_benchmark
    projectile_scaling_benchmark
    event_manager_scaling_benchmark
    integrated_system_benchmark
)

VALGRIND_CALLGRIND_AI_TARGETS=(
    ai_optimization_tests
    crowd_runtime_tests
    ai_collision_integration_tests
    ai_manager_edm_integration_tests
    behavior_functionality_tests
    entity_data_manager_tests
    entity_state_manager_tests
    npc_memory_tests
    thread_safe_ai_manager_tests
    thread_safe_ai_integration_tests
)

VALGRIND_CALLGRIND_EVENTS_TARGETS=(
    event_manager_tests
    event_manager_behavior_tests
    event_types_tests
    event_coordination_integration_tests
    weather_event_tests
    weather_controller_tests
)

VALGRIND_CALLGRIND_RESOURCES_TARGETS=(
    resource_architecture_tests
    resource_path_tests
    world_resource_manager_tests
    world_generator_tests
    world_manager_tests
    world_manager_event_integration_tests
    resource_template_manager_tests
    resource_integration_tests
    resource_change_event_tests
    inventory_controller_tests
    resource_factory_tests
    resource_template_manager_json_tests
    resource_edge_case_tests
    json_reader_tests
)

VALGRIND_CALLGRIND_COLLISION_PATHFINDING_TARGETS=(
    collision_system_tests
    collision_manager_edm_integration_tests
    projectile_manager_tests
    sparse_sidecar_tests
    knockback_sidecar_tests
    pathfinding_system_tests
    pathfinder_manager_tests
    pathfinder_manager_edm_integration_tests
    pathfinder_ai_contention_tests
    collision_pathfinding_integration_tests
)

VALGRIND_CALLGRIND_RUNTIME_MANAGER_TARGETS=(
    particle_manager_core_tests
    particle_manager_performance_tests
    particle_manager_threading_tests
    particle_manager_weather_tests
    buffer_utilization_tests
    buffer_reuse_tests
    frame_profiler_tests
    save_manager_tests
    settings_manager_tests
    game_state_manager_tests
    loading_state_tests
    manager_runtime_tests
    background_simulation_manager_tests
)

VALGRIND_CALLGRIND_RENDER_UI_TARGETS=(
    camera_tests
    rendering_pipeline_tests
    input_manager_tests
    input_manager_command_tests
    ui_manager_functional_tests
    controller_registry_tests
    npc_render_controller_tests
    harvest_controller_tests
    inventory_controller_tests
    combat_controller_tests
    hud_controller_tests
    social_controller_tests
    resource_render_controller_tests
    day_night_controller_tests
    game_time_manager_tests
    game_time_manager_calendar_tests
    game_time_manager_season_tests
)

VALGRIND_CALLGRIND_BENCHMARK_TARGETS=(
    ai_scaling_benchmark
    collision_scaling_benchmark
    projectile_scaling_benchmark
    event_manager_scaling_benchmark
    integrated_system_benchmark
)

valgrind_targets_for_callgrind_category() {
    case "$1" in
        ai)
            printf '%s\n' "${VALGRIND_CALLGRIND_AI_TARGETS[@]}"
            ;;
        events)
            printf '%s\n' "${VALGRIND_CALLGRIND_EVENTS_TARGETS[@]}"
            ;;
        resources)
            printf '%s\n' "${VALGRIND_CALLGRIND_RESOURCES_TARGETS[@]}"
            ;;
        collision_pathfinding)
            printf '%s\n' "${VALGRIND_CALLGRIND_COLLISION_PATHFINDING_TARGETS[@]}"
            ;;
        runtime_managers)
            printf '%s\n' "${VALGRIND_CALLGRIND_RUNTIME_MANAGER_TARGETS[@]}"
            ;;
        render_ui)
            printf '%s\n' "${VALGRIND_CALLGRIND_RENDER_UI_TARGETS[@]}"
            ;;
        benchmarks)
            printf '%s\n' "${VALGRIND_CALLGRIND_BENCHMARK_TARGETS[@]}"
            ;;
        all)
            printf '%s\n' \
                "${VALGRIND_CALLGRIND_AI_TARGETS[@]}" \
                "${VALGRIND_CALLGRIND_EVENTS_TARGETS[@]}" \
                "${VALGRIND_CALLGRIND_RESOURCES_TARGETS[@]}" \
                "${VALGRIND_CALLGRIND_COLLISION_PATHFINDING_TARGETS[@]}" \
                "${VALGRIND_CALLGRIND_RUNTIME_MANAGER_TARGETS[@]}" \
                "${VALGRIND_CALLGRIND_RENDER_UI_TARGETS[@]}"
            ;;
        *)
            return 1
            ;;
    esac
}
