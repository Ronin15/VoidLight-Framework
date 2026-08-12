# Tests for the VoidLight_Template project

# Set the C++ standard
set(CMAKE_CXX_STANDARD 20) # Match the main project's C++ standard
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Check if we can build tests - require Boost Test for test framework
# Try to find both the unit_test_framework (used by tests) and container components.
# If the unit_test_framework component is not found, skip tests.
find_package(Boost COMPONENTS unit_test_framework container)
if(NOT Boost_UNIT_TEST_FRAMEWORK_FOUND)
    message(STATUS "Skipping all tests - Boost Test not found (required)")
    return()
endif()

message(STATUS "Building tests with Boost Test support")

# --- Test Executables ---

set(ALL_TESTS
    save_manager_tests
    settings_manager_tests
    thread_system_tests
    game_state_manager_tests
    ai_optimization_tests
    crowd_runtime_tests
    buffer_utilization_tests
    resource_template_manager_tests
    resource_factory_tests
    resource_template_manager_json_tests
    world_resource_manager_tests
    resource_change_event_tests
    resource_path_tests
    frame_profiler_tests
    resource_edge_case_tests
    json_reader_tests
    world_generator_tests
    world_manager_tests
    world_manager_event_integration_tests
    collision_system_tests
    pathfinding_system_tests
    pathfinder_manager_tests
    camera_tests
    input_manager_tests
    input_manager_command_tests
    simd_correctness_tests
    simd_performance_benchmark
    buffer_reuse_tests
    rendering_pipeline_tests
    loading_state_tests
    manager_runtime_tests
    ui_manager_functional_tests
    integrated_system_benchmark
    game_time_manager_tests
    game_time_manager_calendar_tests
    game_time_manager_season_tests
    entity_state_manager_tests
    entity_data_manager_tests
    npc_memory_tests
    background_simulation_manager_tests
    collision_manager_edm_integration_tests
    event_manager_tests
    projectile_manager_tests
    projectile_scaling_benchmark
    ai_scaling_benchmark
    event_manager_scaling_benchmark
    background_simulation_manager_benchmark
    thread_safe_ai_manager_tests
    thread_safe_ai_integration_tests
    event_types_tests
    event_manager_behavior_tests
    particle_manager_core_tests
    particle_manager_performance_tests
    particle_manager_weather_tests
    particle_manager_threading_tests
    weather_event_tests
    behavior_functionality_tests
    collision_pathfinding_integration_tests
    collision_scaling_benchmark
    adaptive_threading_analysis
    pathfinder_benchmark
    pathfinder_ai_contention_tests
    event_coordination_integration_tests
    ai_collision_integration_tests
    weather_controller_tests
    day_night_controller_tests
    controller_registry_tests
    npc_render_controller_tests
    harvest_controller_tests
    inventory_controller_tests
    combat_controller_tests
    hud_controller_tests
    social_controller_tests
    resource_render_controller_tests
    ai_manager_edm_integration_tests
    pathfinder_manager_edm_integration_tests
    resource_integration_tests
    resource_architecture_tests
    sparse_sidecar_tests
    knockback_sidecar_tests
    timestep_manager_tests
    worker_budget_tests
    projectile_render_controller_tests
)

# Create all test executables
foreach(test_name ${ALL_TESTS})
    set(test_source "")

    # Determine source file based on test name
    if(${test_name} STREQUAL "save_manager_tests")
        set(test_source "SaveManagerTests.cpp")
    elseif(${test_name} STREQUAL "settings_manager_tests")
        set(test_source "SettingsManagerTests.cpp")
    elseif(${test_name} STREQUAL "thread_system_tests")
        set(test_source "ThreadSystemTests.cpp")
    elseif(${test_name} STREQUAL "game_state_manager_tests")
        set(test_source "GameStateManagerTests.cpp")
    elseif(${test_name} STREQUAL "ai_optimization_tests")
        set(test_source "AIOptimizationTest.cpp")
    elseif(${test_name} STREQUAL "crowd_runtime_tests")
        set(test_source "ai/CrowdRuntimeTests.cpp")
    elseif(${test_name} STREQUAL "buffer_utilization_tests")
        set(test_source "BufferUtilizationTest.cpp")
    elseif(${test_name} MATCHES "^resource_")
        string(REPLACE "resource_" "" suffix ${test_name})
        string(REPLACE "_tests" "" suffix ${suffix})
        string(REPLACE "_" "" suffix ${suffix})
        if(${suffix} STREQUAL "templatemanager")
            set(test_source "resources/ResourceTemplateManagerTests.cpp")
        elseif(${suffix} STREQUAL "factory")
            set(test_source "resources/ResourceFactoryTests.cpp")
        elseif(${suffix} STREQUAL "templatemanagerjson")
            set(test_source "resources/ResourceTemplateManagerJsonTests.cpp")
        elseif(${suffix} STREQUAL "changeevent")
            set(test_source "resources/ResourceChangeEventTests.cpp")
        elseif(${suffix} STREQUAL "path")
            set(test_source "utils/ResourcePathTests.cpp")
        elseif(${suffix} STREQUAL "edgecase")
            set(test_source "resources/ResourceEdgeCaseTests.cpp")
        elseif(${suffix} STREQUAL "integration")
            set(test_source "resources/ResourceIntegrationTests.cpp")
        elseif(${suffix} STREQUAL "architecture")
            set(test_source "resources/ResourceArchitectureTests.cpp")
        elseif(${suffix} STREQUAL "rendercontroller")
            set(test_source "controllers/ResourceRenderControllerTests.cpp")
        endif()
    elseif(${test_name} STREQUAL "world_resource_manager_tests")
        set(test_source "resources/WorldResourceManagerTests.cpp")
    elseif(${test_name} STREQUAL "json_reader_tests")
        set(test_source "utils/JsonReaderTest.cpp")
    elseif(${test_name} STREQUAL "frame_profiler_tests")
        set(test_source "utils/FrameProfilerTests.cpp")
    elseif(${test_name} STREQUAL "world_generator_tests")
        set(test_source "world/WorldGeneratorTests.cpp")
    elseif(${test_name} STREQUAL "world_manager_tests")
        set(test_source "world/WorldManagerTests.cpp")
    elseif(${test_name} STREQUAL "world_manager_event_integration_tests")
        set(test_source "world/WorldManagerEventIntegrationTests.cpp")
    elseif(${test_name} STREQUAL "collision_system_tests")
        set(test_source "collisions/CollisionSystemTests.cpp")
    elseif(${test_name} STREQUAL "pathfinding_system_tests")
        set(test_source "pathfinding/PathfindingSystemTests.cpp")
    elseif(${test_name} STREQUAL "pathfinder_manager_tests")
        set(test_source "PathfinderManagerTests.cpp")
    elseif(${test_name} STREQUAL "camera_tests")
        set(test_source "CameraTests.cpp")
    elseif(${test_name} STREQUAL "input_manager_tests")
        set(test_source "InputManagerTests.cpp")
    elseif(${test_name} STREQUAL "input_manager_command_tests")
        set(test_source "InputManagerCommandTests.cpp")
    elseif(${test_name} STREQUAL "simd_correctness_tests")
        set(test_source "SIMDCorrectnessTests.cpp")
    elseif(${test_name} STREQUAL "simd_performance_benchmark")
        set(test_source "performance/SIMDBenchmark.cpp")
    elseif(${test_name} STREQUAL "buffer_reuse_tests")
        set(test_source "BufferReuseTests.cpp")
    elseif(${test_name} STREQUAL "rendering_pipeline_tests")
        set(test_source "RenderingPipelineTests.cpp")
    elseif(${test_name} STREQUAL "loading_state_tests")
        set(test_source "LoadingStateTests.cpp")
    elseif(${test_name} STREQUAL "manager_runtime_tests")
        set(test_source "managers/ManagerRuntimeTests.cpp")
    elseif(${test_name} STREQUAL "ui_manager_functional_tests")
        set(test_source "UIManagerFunctionalTests.cpp")
    elseif(${test_name} STREQUAL "integrated_system_benchmark")
        set(test_source "performance/IntegratedSystemBenchmark.cpp")
    elseif(${test_name} STREQUAL "game_time_manager_tests")
        set(test_source "managers/GameTimeManagerTests.cpp")
    elseif(${test_name} STREQUAL "game_time_manager_calendar_tests")
        set(test_source "managers/GameTimeManagerCalendarTests.cpp")
    elseif(${test_name} STREQUAL "game_time_manager_season_tests")
        set(test_source "managers/GameTimeManagerSeasonTests.cpp")
    elseif(${test_name} STREQUAL "entity_state_manager_tests")
        set(test_source "EntityStateManagerTests.cpp")
    elseif(${test_name} STREQUAL "entity_data_manager_tests")
        set(test_source "managers/EntityDataManagerTests.cpp")
    elseif(${test_name} STREQUAL "npc_memory_tests")
        set(test_source "managers/NPCMemoryTests.cpp")
    elseif(${test_name} STREQUAL "background_simulation_manager_tests")
        set(test_source "managers/BackgroundSimulationManagerTests.cpp")
    elseif(${test_name} STREQUAL "collision_manager_edm_integration_tests")
        set(test_source "collisions/CollisionManagerEDMIntegrationTests.cpp")
    elseif(${test_name} STREQUAL "event_manager_tests")
        set(test_source "events/EventManagerTest.cpp")
    elseif(${test_name} STREQUAL "projectile_manager_tests")
        set(test_source "managers/ProjectileManagerTests.cpp")
    elseif(${test_name} STREQUAL "projectile_scaling_benchmark")
        set(test_source "performance/ProjectileScalingBenchmark.cpp")
    elseif(${test_name} STREQUAL "ai_scaling_benchmark")
        set(test_source "AIScalingBenchmark.cpp")
    elseif(${test_name} STREQUAL "event_manager_scaling_benchmark")
        set(test_source "EventManagerScalingBenchmark.cpp")
    elseif(${test_name} STREQUAL "background_simulation_manager_benchmark")
        set(test_source "managers/BackgroundSimulationManagerBenchmark.cpp")
    elseif(${test_name} STREQUAL "thread_safe_ai_manager_tests")
        set(test_source "ThreadSafeAIManagerTest.cpp")
    elseif(${test_name} STREQUAL "thread_safe_ai_integration_tests")
        set(test_source "ThreadSafeAIIntegrationTest.cpp")
    elseif(${test_name} STREQUAL "event_types_tests")
        set(test_source "events/EventTypesTest.cpp")
    elseif(${test_name} STREQUAL "event_manager_behavior_tests")
        set(test_source "events/EventManagerBehaviorTests.cpp")
    elseif(${test_name} STREQUAL "weather_event_tests")
        set(test_source "events/WeatherEventTest.cpp")
    elseif(${test_name} STREQUAL "behavior_functionality_tests")
        set(test_source "BehaviorFunctionalityTest.cpp")
    elseif(${test_name} STREQUAL "collision_pathfinding_integration_tests")
        set(test_source "CollisionPathfindingIntegrationTests.cpp")
    elseif(${test_name} STREQUAL "collision_scaling_benchmark")
        set(test_source "performance/CollisionScalingBenchmark.cpp")
    elseif(${test_name} STREQUAL "adaptive_threading_analysis")
        set(test_source "performance/AdaptiveThreadingAnalysis.cpp")
    elseif(${test_name} STREQUAL "pathfinder_benchmark")
        set(test_source "performance/PathfinderBenchmark.cpp")
    elseif(${test_name} STREQUAL "pathfinder_ai_contention_tests")
        set(test_source "integration/PathfinderAIContentionTests.cpp")
    elseif(${test_name} STREQUAL "event_coordination_integration_tests")
        set(test_source "integration/EventCoordinationIntegrationTests.cpp")
    elseif(${test_name} STREQUAL "ai_collision_integration_tests")
        set(test_source "integration/AICollisionIntegrationTests.cpp")
    elseif(${test_name} MATCHES "^particle_manager_")
        string(REPLACE "particle_manager_" "" suffix ${test_name})
        string(REPLACE "_tests" "" suffix ${suffix})
        if(${suffix} STREQUAL "core")
            set(test_source "particle/ParticleManagerCoreTest.cpp")
        elseif(${suffix} STREQUAL "performance")
            set(test_source "particle/ParticleManagerPerformanceTest.cpp")
        elseif(${suffix} STREQUAL "weather")
            set(test_source "particle/ParticleManagerWeatherTest.cpp")
        elseif(${suffix} STREQUAL "threading")
            set(test_source "particle/ParticleManagerThreadingTest.cpp")
        endif()
    elseif(${test_name} STREQUAL "weather_controller_tests")
        set(test_source "controllers/WeatherControllerTests.cpp")
    elseif(${test_name} STREQUAL "day_night_controller_tests")
        set(test_source "controllers/DayNightControllerTests.cpp")
    elseif(${test_name} STREQUAL "controller_registry_tests")
        set(test_source "controllers/ControllerRegistryTests.cpp")
    elseif(${test_name} STREQUAL "npc_render_controller_tests")
        set(test_source "controllers/NPCRenderControllerTests.cpp")
    elseif(${test_name} STREQUAL "harvest_controller_tests")
        set(test_source "controllers/HarvestControllerTests.cpp")
    elseif(${test_name} STREQUAL "inventory_controller_tests")
        set(test_source "controllers/InventoryControllerTests.cpp")
    elseif(${test_name} STREQUAL "combat_controller_tests")
        set(test_source "controllers/CombatControllerTests.cpp")
    elseif(${test_name} STREQUAL "hud_controller_tests")
        set(test_source "controllers/HudControllerTests.cpp")
    elseif(${test_name} STREQUAL "social_controller_tests")
        set(test_source "controllers/SocialControllerTests.cpp")
    elseif(${test_name} STREQUAL "ai_manager_edm_integration_tests")
        set(test_source "managers/AIManagerEDMIntegrationTests.cpp")
    elseif(${test_name} STREQUAL "pathfinder_manager_edm_integration_tests")
        set(test_source "managers/PathfinderManagerEDMIntegrationTests.cpp")
    elseif(${test_name} STREQUAL "sparse_sidecar_tests")
        set(test_source "managers/SparseSidecarTest.cpp")
    elseif(${test_name} STREQUAL "knockback_sidecar_tests")
        set(test_source "integration/KnockbackSidecarTest.cpp")
    elseif(${test_name} STREQUAL "timestep_manager_tests")
        set(test_source "core/TimestepManagerTests.cpp")
    elseif(${test_name} STREQUAL "worker_budget_tests")
        set(test_source "core/WorkerBudgetTests.cpp")
    elseif(${test_name} STREQUAL "projectile_render_controller_tests")
        set(test_source "controllers/ProjectileRenderControllerTests.cpp")
    endif()

    if("${test_source}" STREQUAL "")
        message(FATAL_ERROR "No test source mapped for ${test_name}")
    endif()

    add_executable(${test_name} ${test_source})
    target_link_libraries(${test_name} PRIVATE VoidLightLib Boost::unit_test_framework)
    target_compile_definitions(${test_name} PRIVATE BOOST_TEST_NO_SIGNAL_HANDLING)
    if(${test_name} STREQUAL "rendering_pipeline_tests")
        target_compile_definitions(${test_name} PRIVATE
            VOIDLIGHT_PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    endif()
endforeach()

# --- Tests needing EventManagerTestAccess ---
set(EVENT_ACCESS_TESTS
    event_manager_tests
    event_manager_behavior_tests
    weather_event_tests
    harvest_controller_tests
    inventory_controller_tests
    hud_controller_tests
    social_controller_tests
    resource_render_controller_tests
    weather_controller_tests
    day_night_controller_tests
)
foreach(test_name ${EVENT_ACCESS_TESTS})
    target_sources(${test_name} PRIVATE events/EventManagerTestAccess.cpp)
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/events
        ${CMAKE_CURRENT_SOURCE_DIR}/controllers)
endforeach()

# --- Tests needing special defines ---
target_compile_definitions(event_manager_tests PRIVATE RUNNING_TESTS)
target_compile_definitions(event_types_tests PRIVATE RUNNING_TESTS)

# --- Tests needing MockPlayer (serialization tests) ---
target_sources(save_manager_tests PRIVATE mocks/MockPlayer.cpp)
target_include_directories(save_manager_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/mocks)

# resource_factory_tests uses test access helper from tests/resources
target_sources(resource_factory_tests PRIVATE resources/ResourceTestAccess.cpp)

# --- CTest Registration ---
foreach(test_name ${ALL_TESTS})
    # Benchmark tests need specific test filters to avoid running all (slow) tests
    if(${test_name} STREQUAL "ai_scaling_benchmark")
        # Only run the primary scaling test by default
        add_test(NAME ${test_name} COMMAND ${test_name} --run_test=AIScalingTests/AIEntityScaling)
    elseif(${test_name} STREQUAL "collision_scaling_benchmark")
        # Only run MM scaling test by default (fast representative test)
        add_test(NAME ${test_name} COMMAND ${test_name} --run_test=CollisionScalingTests/MMScaling)
    elseif(${test_name} STREQUAL "projectile_scaling_benchmark")
        # Only run primary scaling test by default
        add_test(NAME ${test_name} COMMAND ${test_name} --run_test=ProjectileScalingTests/ProjectileScaling)
    elseif(${test_name} STREQUAL "integrated_system_benchmark")
        # Only run quick 60FPS test for ctest (full suite takes ~90s due to sustained perf test)
        add_test(NAME ${test_name} COMMAND ${test_name} --run_test=IntegratedSystemBenchmarkSuite/TestRealisticGameSimulation60FPS)
    else()
        add_test(NAME ${test_name} COMMAND ${test_name})
    endif()
    # Set working directory to project root so tests can find res/data/
    set_tests_properties(${test_name} PROPERTIES
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endforeach()

# --- Power Profiling Tool (Standalone, no Boost.Test dependency) ---
# This is a headless benchmark for measuring CPU power consumption via race-to-idle strategy
add_executable(PowerProfile power_profile.cpp)
target_link_libraries(PowerProfile PRIVATE VoidLightLib)
target_include_directories(PowerProfile PRIVATE ${CMAKE_SOURCE_DIR}/include)

# PowerProfile is not a test - it's a utility executable for power measurements
# Set output directory to match other executables
set_target_properties(PowerProfile PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
)

# --- GPU Tests ---
message(STATUS "Building GPU tests")

# GPU test executables
set(GPU_UNIT_TESTS
        gpu_types_tests
        gpu_pipeline_config_tests
    )

    set(GPU_INTEGRATION_TESTS
        gpu_device_tests
        gpu_shader_manager_tests
        gpu_resource_tests
        gpu_vertex_pool_tests
        sprite_batch_tests
    )

    set(GPU_SYSTEM_TESTS
        gpu_renderer_tests
    )

    add_executable(gpu_frame_timing_benchmark gpu/GPUFrameTimingBenchmark.cpp)
    target_link_libraries(gpu_frame_timing_benchmark PRIVATE VoidLightLib)
    target_include_directories(gpu_frame_timing_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/gpu)
    set_target_properties(gpu_frame_timing_benchmark PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
    )

    # Source file mapping for GPU unit tests (no GPU required)
    foreach(test_name ${GPU_UNIT_TESTS})
        set(test_source "")
        if(${test_name} STREQUAL "gpu_types_tests")
            set(test_source "gpu/GPUTypesTests.cpp")
        elseif(${test_name} STREQUAL "gpu_pipeline_config_tests")
            set(test_source "gpu/GPUPipelineConfigTests.cpp")
        endif()

        if(test_source STREQUAL "")
            message(FATAL_ERROR "No source mapping found for GPU unit test '${test_name}'")
        endif()

        add_executable(${test_name} ${test_source})
        target_link_libraries(${test_name} PRIVATE VoidLightLib Boost::unit_test_framework)
        target_include_directories(${test_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/gpu)
        target_compile_definitions(${test_name} PRIVATE BOOST_TEST_NO_SIGNAL_HANDLING)
        add_test(NAME ${test_name} COMMAND ${test_name})
        set_tests_properties(${test_name} PROPERTIES
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            LABELS "GPU;Unit")
    endforeach()

    # Source file mapping for GPU integration tests (require GPU)
    foreach(test_name ${GPU_INTEGRATION_TESTS})
        set(test_source "")
        if(${test_name} STREQUAL "gpu_device_tests")
            set(test_source "gpu/GPUDeviceTests.cpp")
        elseif(${test_name} STREQUAL "gpu_shader_manager_tests")
            set(test_source "gpu/GPUShaderManagerTests.cpp")
        elseif(${test_name} STREQUAL "gpu_resource_tests")
            set(test_source "gpu/GPUResourceTests.cpp")
        elseif(${test_name} STREQUAL "gpu_vertex_pool_tests")
            set(test_source "gpu/GPUVertexPoolTests.cpp")
        elseif(${test_name} STREQUAL "sprite_batch_tests")
            set(test_source "gpu/SpriteBatchTests.cpp")
        endif()

        if(test_source STREQUAL "")
            message(FATAL_ERROR "No source mapping found for GPU integration test '${test_name}'")
        endif()

        add_executable(${test_name} ${test_source})
        target_link_libraries(${test_name} PRIVATE VoidLightLib Boost::unit_test_framework)
        target_include_directories(${test_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/gpu)
        target_compile_definitions(${test_name} PRIVATE BOOST_TEST_NO_SIGNAL_HANDLING)
        add_test(NAME ${test_name} COMMAND ${test_name})
        set_tests_properties(${test_name} PROPERTIES
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            LABELS "GPU;Integration")
    endforeach()

    # Source file mapping for GPU system tests (full frame flow)
    foreach(test_name ${GPU_SYSTEM_TESTS})
        set(test_source "")
        if(${test_name} STREQUAL "gpu_renderer_tests")
            set(test_source "gpu/GPURendererTests.cpp")
        endif()

        if(test_source STREQUAL "")
            message(FATAL_ERROR "No source mapping found for GPU system test '${test_name}'")
        endif()

        add_executable(${test_name} ${test_source})
        target_link_libraries(${test_name} PRIVATE VoidLightLib Boost::unit_test_framework)
        target_include_directories(${test_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/gpu)
        target_compile_definitions(${test_name} PRIVATE BOOST_TEST_NO_SIGNAL_HANDLING)
        add_test(NAME ${test_name} COMMAND ${test_name})
        set_tests_properties(${test_name} PROPERTIES
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            LABELS "GPU;System")
    endforeach()
