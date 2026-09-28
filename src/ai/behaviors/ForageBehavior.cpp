/* Copyright (c) 2025 Hammer Forged Games
 * All rights reserved.
 * Licensed under the MIT License - see LICENSE file for details
 */

#include "ai/AICommandBus.hpp"
#include "ai/BehaviorExecutors.hpp"
#include "managers/EntityDataManager.hpp"
#include "managers/PathfinderManager.hpp"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <random>

namespace {

thread_local std::mt19937 s_rng{std::random_device{}()};
thread_local std::uniform_real_distribution<float> s_cooldownVariation{0.0f, 1.0f};

// Seconds without meaningful progress toward the target before the forager is stalled.
constexpr float FORAGE_STALL_SECONDS = 2.0f;
// Fraction of the per-frame travel distance that counts as progress.
constexpr float FORAGE_PROGRESS_FRACTION = 0.25f;
// Nav-path waypoint reached radius (half a tile).
constexpr float FORAGE_WAYPOINT_RADIUS = 32.0f;
// Per-entity re-search stagger (entityId % 200 -> up to 0.2 s) so retargeting
// foragers do not all scan the snapshot on the same frame.
constexpr uint64_t FORAGE_SEARCH_STAGGER_BUCKETS = 200;
constexpr float FORAGE_SEARCH_STAGGER_STEP = 0.001f;
// Resolution of the per-entity need seed (entityId % buckets over the window).
constexpr uint64_t NEED_STAGGER_BUCKETS = 1024;

float searchStagger(EntityHandle::IDType entityId) {
    return static_cast<float>(entityId % FORAGE_SEARCH_STAGGER_BUCKETS) * FORAGE_SEARCH_STAGGER_STEP;
}

static_assert(VoidLight::ForageBehaviorConfig{}.searchRadius ==
        VoidLight::HarvestCommit::SCARCITY_RADIUS,
    "Forage candidate radius must match the HarvestCommit reserve radius");

struct CellRange {
    uint32_t colMin{0};
    uint32_t colMax{0};
    uint32_t rowMin{0};
    uint32_t rowMax{0};
};

// Requires !view.empty().
CellRange cellsAround(const HarvestableSnapshotView& view, const Vector2D& position, float radius) {
    return {view.cellColumn(position.getX() - radius), view.cellColumn(position.getX() + radius),
        view.cellRow(position.getY() - radius), view.cellRow(position.getY() + radius)};
}

// True when at least NPC_HARVEST_RESERVE other snapshot entries lie within
// SCARCITY_RADIUS of entries[candidate]: the same area rule HarvestCommit applies
// at commit time, so NPCs never target a node the commit must reject.
bool keepsAreaReserve(const HarvestableSnapshotView& view, size_t candidate) {
    constexpr size_t RESERVE = VoidLight::HarvestCommit::NPC_HARVEST_RESERVE;
    if constexpr (RESERVE == 0) {
        return true;
    }
    constexpr float RADIUS = VoidLight::HarvestCommit::SCARCITY_RADIUS;
    constexpr float RADIUS_SQ = RADIUS * RADIUS;
    const Vector2D center = view.entries[candidate].position;
    const CellRange cells = cellsAround(view, center, RADIUS);
    size_t others = 0;
    for (uint32_t row = cells.rowMin; row <= cells.rowMax; ++row) {
        for (uint32_t col = cells.colMin; col <= cells.colMax; ++col) {
            const size_t cell = static_cast<size_t>(row) * view.cols + col;
            for (uint32_t i = view.cellStarts[cell]; i < view.cellStarts[cell + 1]; ++i) {
                if (i == candidate ||
                    (view.entries[i].position - center).lengthSquared() > RADIUS_SQ) {
                    continue;
                }
                if (++others >= RESERVE) {
                    return true;
                }
            }
        }
    }
    return false;
}

// Nearest snapshot entry within radius of position that is not `exclude` and
// keeps the area reserve; SIZE_MAX when none qualifies.
size_t findForageTarget(const HarvestableSnapshotView& view, const Vector2D& position,
    float radius, EntityHandle exclude) {
    if (view.empty()) {
        return SIZE_MAX;
    }
    const CellRange cells = cellsAround(view, position, radius);
    float bestSq = radius * radius;
    size_t best = SIZE_MAX;
    for (uint32_t row = cells.rowMin; row <= cells.rowMax; ++row) {
        for (uint32_t col = cells.colMin; col <= cells.colMax; ++col) {
            const size_t cell = static_cast<size_t>(row) * view.cols + col;
            for (uint32_t i = view.cellStarts[cell]; i < view.cellStarts[cell + 1]; ++i) {
                const HarvestableSnapshotEntry& entry = view.entries[i];
                const float distSq = (entry.position - position).lengthSquared();
                if (distSq > bestSq || (best != SIZE_MAX && distSq == bestSq) ||
                    entry.handle == exclude || !keepsAreaReserve(view, i)) {
                    continue;
                }
                bestSq = distSq;
                best = i;
            }
        }
    }
    return best;
}

// Entries are sorted by staticIndex inside each cell, and the target position is
// the snapshot position, so validity is a binary search in the target's cell.
bool isTargetAvailable(const HarvestableSnapshotView& view, const Vector2D& targetPos,
    uint32_t staticIndex, EntityHandle handle) {
    if (view.empty()) {
        return false;
    }
    const size_t cell =
        static_cast<size_t>(view.cellRow(targetPos.getY())) * view.cols + view.cellColumn(targetPos.getX());
    const auto first = view.entries.begin() + view.cellStarts[cell];
    const auto last = view.entries.begin() + view.cellStarts[cell + 1];
    const auto it = std::lower_bound(first, last, staticIndex,
        [](const HarvestableSnapshotEntry& entry, uint32_t index) {
            return entry.staticIndex < index;
        });
    return it != last && it->staticIndex == staticIndex && it->handle == handle;
}

void applyForageBackoff(NpcNeedData& need) {
    const uint8_t shift = std::min(need.failCount, Behaviors::FORAGE_MAX_BACKOFF_SHIFT);
    need.retryCooldown = Behaviors::FORAGE_RETRY_COOLDOWN * static_cast<float>(1u << shift);
    if (need.failCount < UINT8_MAX) {
        ++need.failCount;
    }
}

void retarget(BehaviorContext& ctx, const VoidLight::ForageBehaviorConfig& config,
    VoidLight::ForageStateData& state) {
    state.lastFailedTarget = state.targetHandle;
    state.targetHandle = EntityHandle{};
    state.targetStaticIndex = UINT32_MAX;
    state.harvestTimer = 0.0f;
    state.phase = VoidLight::ForagePhase::Searching;
    state.searchCooldown = config.updateInterval + searchStagger(ctx.entityId);
    ctx.transform.velocity = Vector2D(0.0f, 0.0f);
    if (ctx.pathData) {
        ctx.pathData->clear();
    }
}

// A rejected commit or a far stall counts against the episode. After
// FORAGE_MAX_FAILED_ATTEMPTS the forager backs off and resumes its origin role,
// so repeated rejections or unreachable nodes cannot keep it in Forage.
void failAttempt(BehaviorContext& ctx, const VoidLight::ForageBehaviorConfig& config,
    VoidLight::ForageStateData& state, NpcNeedData& need) {
    if (++state.failedAttempts >= Behaviors::FORAGE_MAX_FAILED_ATTEMPTS) {
        ctx.transform.velocity = Vector2D(0.0f, 0.0f);
        applyForageBackoff(need);
        Behaviors::switchBehavior(ctx.edmIndex, need.returnBehavior);
        return;
    }
    retarget(ctx, config, state);
}

void beginHarvest(BehaviorContext& ctx, VoidLight::ForageStateData& state) {
    state.phase = VoidLight::ForagePhase::Harvesting;
    state.harvestTimer = 0.0f;
    ctx.transform.velocity = Vector2D(0.0f, 0.0f);
    if (ctx.pathData) {
        ctx.pathData->clear();
    }
}

void moveTowardTarget(BehaviorContext& ctx, const VoidLight::ForageBehaviorConfig& config,
    const Vector2D& target, float speed) {
    const Vector2D currentPos = ctx.transform.position;
    if (ctx.pathData) {
        auto& pathData = *ctx.pathData;
        pathData.pathUpdateTimer += ctx.deltaTime;
        if (pathData.pathRequestCooldown > 0.0f) {
            pathData.pathRequestCooldown -= ctx.deltaTime;
        }

        const bool needsPath = !pathData.hasPath || pathData.navIndex >= pathData.pathLength ||
            pathData.pathUpdateTimer > config.pathRequestCooldown;
        if (needsPath && pathData.pathRequestCooldown <= 0.0f) {
            PathfinderManager::Instance().requestPathToEDM(ctx.edmIndex, currentPos, target,
                PathfinderManager::Priority::Normal);
            pathData.pathRequestCooldown = config.pathRequestCooldown +
                s_cooldownVariation(s_rng) * config.variation;
        }

        if (pathData.isFollowingPath()) {
            Vector2D toWaypoint = pathData.currentWaypoint - currentPos;
            float dist = toWaypoint.length();
            if (dist < FORAGE_WAYPOINT_RADIUS) {
                EntityDataManager::Instance().advanceWaypointWithCache(ctx.edmIndex);
                if (pathData.isFollowingPath()) {
                    toWaypoint = pathData.currentWaypoint - currentPos;
                    dist = toWaypoint.length();
                }
            }
            if (pathData.isFollowingPath() && dist > 0.001f) {
                ctx.transform.velocity = toWaypoint * (speed / dist);
                return;
            }
        }
    }

    // Direct movement: no path yet, path finished short of the node (obstacle
    // tiles), or no path data.
    const Vector2D toTarget = target - currentPos;
    const float dist = toTarget.length();
    ctx.transform.velocity = dist > 0.001f ? toTarget * (speed / dist) : Vector2D(0.0f, 0.0f);
}

} // anonymous namespace

namespace Behaviors {

void tickNeed(NpcNeedData& need, float deltaTime) {
    need.pressure = std::min(1.0f, need.pressure + NEED_PRESSURE_PER_SECOND * deltaTime);
    if (need.retryCooldown > 0.0f) {
        need.retryCooldown = std::max(0.0f, need.retryCooldown - deltaTime);
    }
}

bool shouldStartForage(BehaviorContext& ctx, BehaviorType currentType) {
    NpcNeedData* need = ctx.needs.get(static_cast<uint32_t>(ctx.edmIndex));
    if (!need || need->pressure < FORAGE_ENTER_THRESHOLD || need->retryCooldown > 0.0f) {
        return false;
    }

    // Pre-check: only enter Forage when some node in range can be taken without
    // breaking its area reserve; otherwise back off so an exhausted area causes
    // no Forage churn.
    if (findForageTarget(ctx.harvestables, ctx.transform.position,
            VoidLight::HarvestCommit::SCARCITY_RADIUS, EntityHandle{}) != SIZE_MAX) {
        need->returnBehavior = currentType;
        switchBehavior(ctx.edmIndex, BehaviorType::Forage);
        return true;
    }

    applyForageBackoff(*need);
    return false;
}

void seedNeed(NpcNeedData& need, EntityHandle::IDType entityId) {
    const float fraction = static_cast<float>(entityId % NEED_STAGGER_BUCKETS) /
        static_cast<float>(NEED_STAGGER_BUCKETS);
    need.pressure = NEED_PRESSURE_PER_SECOND * NEED_ENTRY_STAGGER_SECONDS * fraction;
}

void initForage(size_t edmIndex, const VoidLight::ForageBehaviorConfig&,
    VoidLight::ForageStateData& state) {
    auto& edm = EntityDataManager::Instance();
    edm.initBehaviorData(edmIndex, BehaviorType::Forage);
    auto& shared = edm.getBehaviorData(edmIndex);

    // Cache moveSpeed from CharacterData (one-time cost)
    shared.moveSpeed = edm.getCharacterDataByIndex(edmIndex).moveSpeed;

    // Forage reads its need entry through ctx.needs; guarantee it exists (main thread).
    edm.ensureNpcNeed(edmIndex);

    state = VoidLight::ForageStateData{};
    state.targetStaticIndex = UINT32_MAX;
    // Same stagger as retarget(): NPCs switching to Forage on one frame spread
    // their first snapshot search.
    state.searchCooldown = searchStagger(edm.getHandle(edmIndex).getId());
    shared.setInitialized(true);
}

void executeForage(BehaviorContext& ctx, const VoidLight::ForageBehaviorConfig& config,
    VoidLight::ForageStateData& state) {
    if (!ctx.sharedState.isValid()) return;

    auto& shared = ctx.sharedState;

    // Process pending behavior messages
    for (uint8_t i = 0; i < shared.pendingMessageCount; ++i) {
        switch (shared.pendingMessages[i].messageId) {
            case BehaviorMessage::PANIC:
                shared.pendingMessageCount = 0;
                switchBehavior(ctx.edmIndex, BehaviorType::Flee);
                return;
            case BehaviorMessage::CALM_DOWN:
                if (ctx.memoryData.isValid()) {
                    ctx.memoryData.emotions.fear = std::max(0.0f, ctx.memoryData.emotions.fear - 0.5f);
                }
                break;
            case BehaviorMessage::RAISE_ALERT:
                if (ctx.memoryData.personality.bravery < 0.4f) {
                    shared.pendingMessageCount = 0;
                    switchBehavior(ctx.edmIndex, BehaviorType::Flee);
                    return;
                }
                break;
            default: break;
        }
    }
    shared.pendingMessageCount = 0;

    // Combat reaction: brave+aggressive NPCs fight back, others flee
    if (isUnderRecentAttack(ctx, 2.0f)) {
        if (shouldRetaliate(ctx)) {
            switchBehavior(ctx.edmIndex, BehaviorType::Chase);
        } else {
            switchBehavior(ctx.edmIndex, BehaviorType::Flee);
        }
        return;
    }
    if (shouldFleeFromFear(ctx)) {
        switchBehavior(ctx.edmIndex, BehaviorType::Flee);
        return;
    }
    if (ctx.hasHostileInRow && tryEngageHostileInRange(ctx)) {
        return;
    }

    // initForage creates the entry on the main thread. Role assignment
    // (AIManager::syncNeedForRole) may remove entries on the main thread outside
    // AI batches, but never for Forage: it skips Forage, and reassignment away
    // from Forage changes the behavior first.
    NpcNeedData* need = ctx.needs.get(static_cast<uint32_t>(ctx.edmIndex));
    assert(need && "Forage requires an NPC need entry (created by initForage)");

    const Vector2D position = ctx.transform.position;

    switch (state.phase) {
        case VoidLight::ForagePhase::Searching: {
            ctx.transform.velocity = Vector2D(0.0f, 0.0f);
            if (state.searchCooldown > 0.0f) {
                state.searchCooldown -= ctx.deltaTime;
                return;
            }

            const size_t found = findForageTarget(ctx.harvestables, position,
                config.searchRadius, state.lastFailedTarget);
            if (found != SIZE_MAX) {
                const HarvestableSnapshotEntry& entry = ctx.harvestables.entries[found];
                state.targetHandle = entry.handle;
                state.targetStaticIndex = entry.staticIndex;
                state.targetPos = entry.position;
                state.lastTargetDistance = (entry.position - position).length();
                state.phase = VoidLight::ForagePhase::Moving;
                shared.separationTimer = 0.0f;
                if (ctx.pathData) {
                    ctx.pathData->clear();
                }
                return;
            }

            // Scarcity reaction: nothing to spare nearby. Back off and resume the
            // role the forager came from.
            applyForageBackoff(*need);
            switchBehavior(ctx.edmIndex, need->returnBehavior);
            return;
        }

        case VoidLight::ForagePhase::Moving: {
            if (!isTargetAvailable(ctx.harvestables, state.targetPos, state.targetStaticIndex,
                    state.targetHandle)) {
                retarget(ctx, config, state);
                return;
            }

            const float dist = (state.targetPos - position).length();
            if (dist <= VoidLight::HarvestCommit::HARVEST_RANGE) {
                beginHarvest(ctx, state);
                return;
            }

            // Stall detection on actual progress toward the node (collision
            // push-back against an obstacle tile leaves velocity non-zero).
            const float envSpeed = shared.moveSpeed * ctx.envSnapshot.moveSpeedScale;
            const float minProgress = envSpeed * ctx.deltaTime * FORAGE_PROGRESS_FRACTION;
            if (state.lastTargetDistance - dist < minProgress) {
                shared.separationTimer += ctx.deltaTime;
            } else {
                shared.separationTimer = 0.0f;
            }
            state.lastTargetDistance = dist;

            if (shared.separationTimer >= FORAGE_STALL_SECONDS) {
                shared.separationTimer = 0.0f;
                if (dist <= FORAGE_STALL_REACH) {
                    beginHarvest(ctx, state);
                } else {
                    failAttempt(ctx, config, state, *need);
                }
                return;
            }

            moveTowardTarget(ctx, config, state.targetPos, envSpeed);
            if (isOnAlert(ctx)) {
                ctx.transform.velocity = ctx.transform.velocity * 0.7f;
            }
            return;
        }

        case VoidLight::ForagePhase::Harvesting: {
            ctx.transform.velocity = Vector2D(0.0f, 0.0f);
            if (!isTargetAvailable(ctx.harvestables, state.targetPos, state.targetStaticIndex,
                    state.targetHandle)) {
                retarget(ctx, config, state);
                return;
            }

            state.harvestTimer += ctx.deltaTime;
            if (state.harvestTimer < config.harvestDuration) {
                return;
            }

            // Main thread validates handles, reach, and reserve in
            // AIManager::commitQueuedHarvests() via HarvestCommit.
            VoidLight::AICommandBus::Instance().enqueueHarvest(
                EntityDataManager::Instance().getHandle(ctx.edmIndex), ctx.edmIndex,
                state.targetHandle, state.targetStaticIndex);
            state.phase = VoidLight::ForagePhase::AwaitingCommit;
            return;
        }

        case VoidLight::ForagePhase::AwaitingCommit:
            // A successful commit switches the forager back to its origin role in
            // the same frame, so reaching this phase means the commit was rejected.
            failAttempt(ctx, config, state, *need);
            return;
    }
}

} // namespace Behaviors
