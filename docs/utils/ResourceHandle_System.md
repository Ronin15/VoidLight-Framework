# Resource Handle System

**Code:** `include/utils/ResourceHandle.hpp`, `include/managers/ResourceTemplateManager.hpp`, `src/managers/ResourceTemplateManager.cpp`, `src/gameStates/GamePlayState.cpp`, `src/entities/Player.cpp`, `src/entities/NPC.cpp`

## Overview

The **Resource Handle System** is a high-performance architecture for resource identification and access in VoidLight-Framework. It replaces string-based resource lookups with lightweight, cache-friendly 64-bit handles for optimal runtime performance.

## Architecture Principles

### Two-Phase Design

The system operates in two distinct phases:

1. **Data Load/Validation Phase** (Initialization)
   - Name-based lookups allowed
   - JSON loading and parsing
   - Resource validation and duplicate detection
   - Name-to-handle conversion

2. **Runtime Phase** (Gameplay)
   - Handle-based operations only
   - Fast integer comparisons and lookups
   - Cache-optimized data access
   - No string operations

### Performance Benefits

| Operation | String-Based | Handle-Based | Improvement |
|-----------|--------------|--------------|-------------|
| Resource lookup | O(1) hash + string compare | O(1) integer lookup | ~10x faster |
| Memory per ID | ~50-200 bytes | 6 bytes | ~25x less memory |
| Cache efficiency | Poor (scattered strings) | Excellent (dense integers) | Better locality |
| Type safety | Runtime errors | Compile-time safety | Fewer bugs |

## ResourceHandle Structure

```cpp
namespace VoidLight {
    class ResourceHandle {
    private:
        uint32_t m_id;          // 32-bit unique identifier
        uint16_t m_generation;  // 16-bit generation counter
        
    public:
        // Validity checking
        constexpr bool isValid() const noexcept;
        
        // Comparison operators
        constexpr bool operator==(const ResourceHandle& other) const noexcept;
        constexpr bool operator!=(const ResourceHandle& other) const noexcept;
        constexpr bool operator<(const ResourceHandle& other) const noexcept;
        
        // Hash support for containers
        std::size_t hash() const noexcept;
        
        // Debug string representation
        std::string toString() const;
        
        // Accessors
        constexpr HandleId getId() const noexcept;
        constexpr Generation getGeneration() const noexcept;
    };
}
```

### Key Features
- **48-bit total size**: Fits efficiently in CPU registers and cache lines
- **Generation counter**: Prevents stale reference bugs (future enhancement)
- **Hash support**: Works with `std::unordered_map` and `std::unordered_set`
- **Type safety**: Cannot accidentally mix handles with other integers
- **Debug support**: `toString()` for logging and debugging
- **Constexpr operations**: Compile-time handle operations where possible

## Integration Examples

### GamePlayState Pattern
```cpp
class GamePlayState : public GameState {
private:
    // Store handles as member variables
    VoidLight::ResourceHandle m_goldHandle;
    VoidLight::ResourceHandle m_healthPotionHandle;
    
public:
    void init() override {
        // Phase 1: Convert stable JSON IDs to handles during initialization
        const auto& rtm = ResourceTemplateManager::Instance();
        m_goldHandle = rtm.getHandleById("gold_coins");
        m_healthPotionHandle = rtm.getHandleById("health_potion");
    }
    
    void updateInventoryUI() {
        // Phase 2: Use handles for runtime operations
        const auto& rtm = ResourceTemplateManager::Instance();
        
        if (m_goldHandle.isValid()) {
            int goldAmount = player->getInventory()->getResourceQuantity(m_goldHandle);
            ui->updateGoldDisplay(goldAmount);
        }
    }
    
    void addDemoResource() {
        // Runtime: Fast handle-based operations
        if (m_healthPotionHandle.isValid()) {
            player->getInventory()->addResource(m_healthPotionHandle, 1);
        }
    }
};
```

### Player Equipment Pattern
```cpp
class Player : public Entity {
private:
    // Store handles instead of item names
    std::unordered_map<std::string, VoidLight::ResourceHandle> m_equippedItems;
    
public:
    bool equipItem(VoidLight::ResourceHandle itemHandle) {
        // Handle-based validation and equipment
        if (!itemHandle.isValid()) {
            return false;
        }
        
        const auto& rtm = ResourceTemplateManager::Instance();
        auto itemTemplate = rtm.getResourceTemplate(itemHandle);
        if (!itemTemplate || itemTemplate->getType() != ResourceType::Equipment) {
            return false;
        }
        
        // Fast equipment logic using handles
        std::string slotName = determineSlot(itemHandle);
        m_equippedItems[slotName] = itemHandle;
        return true;
    }
    
    VoidLight::ResourceHandle getEquippedItem(const std::string& slotName) const {
        auto it = m_equippedItems.find(slotName);
        return (it != m_equippedItems.end()) ? it->second : VoidLight::ResourceHandle{};
    }
};
```

### NPC Loot System Pattern
```cpp
class NPC : public Entity {
private:
    // Handle-based loot drops
    std::unordered_map<VoidLight::ResourceHandle, float> m_dropRates;
    
public:
    void initializeLootDrops() {
        // Phase 1: Convert stable JSON IDs to handles during setup
        const auto& rtm = ResourceTemplateManager::Instance();
        
        auto goldHandle = rtm.getHandleById("gold_coins");
        if (goldHandle.isValid()) {
            m_dropRates[goldHandle] = 0.8f; // 80% drop rate
        }
        
        auto potionHandle = rtm.getHandleById("health_potion");
        if (potionHandle.isValid()) {
            m_dropRates[potionHandle] = 0.3f; // 30% drop rate
        }
    }
    
    void dropLoot() {
        // Phase 2: Runtime operations using handles
        for (const auto& [itemHandle, dropRate] : m_dropRates) {
            if (shouldDrop(dropRate)) {
                dropSpecificItem(itemHandle, calculateQuantity(itemHandle));
            }
        }
    }
};
```

## Best Practices

### DO: Handle-Based Runtime Operations
```cpp
// ✅ GOOD: Fast, cache-friendly operations
void fastResourceAccess() {
    VoidLight::ResourceHandle handle = getResourceHandle();
    
    // Direct handle operations
    if (handle.isValid()) {
        int stackSize = rtm.getMaxStackSize(handle);
        float value = rtm.getValue(handle);
        ResourcePtr resource = rtm.createResource(handle);
    }
    
    // Bulk operations for even better performance
    std::vector<VoidLight::ResourceHandle> handles = getAllHandles();
    auto values = rtm.getValues(handles);  // Single optimized call
}
```

### DON'T: String-Based Runtime Operations
```cpp
// ❌ BAD: Slow string operations during gameplay
void slowResourceAccess() {
    // String lookup every time (slow)
    ResourcePtr resource = rtm.getResourceById("gold_coins");
    if (resource) {
        float value = resource->getValue();  // Shared_ptr dereferencing
    }
    
    // String-based inventory operations (slow)
    inventory->addResource("health_potion", 1);  // String lookup required
}
```

### Handle Caching Patterns

#### Member Variable Caching
```cpp
class InventoryUI {
private:
    VoidLight::ResourceHandle m_goldHandle;
    VoidLight::ResourceHandle m_healthPotionHandle;
    
public:
    void initialize() {
        // Cache handles once during initialization
        const auto& rtm = ResourceTemplateManager::Instance();
        m_goldHandle = rtm.getHandleById("gold_coins");
        m_healthPotionHandle = rtm.getHandleById("health_potion");
    }
};
```

#### Static Handle Caching
```cpp
class ResourceConstants {
public:
    static VoidLight::ResourceHandle getGoldHandle() {
        static VoidLight::ResourceHandle s_goldHandle =
            ResourceTemplateManager::Instance().getHandleById("gold_coins");
        return s_goldHandle;
    }
};
```

## Migration Guide

### Converting String-Based Code

#### Before (String-Based)
```cpp
bool Player::equipItem(const std::string& itemId) {
    // Name-based lookup (slow)
    const auto& rtm = ResourceTemplateManager::Instance();
    auto resource = rtm.getResourceByName(itemId);
    if (!resource) return false;
    
    auto handle = resource->getHandle();
    // ... rest of logic
}
```

#### After (Handle-Based)
```cpp
bool Player::equipItem(VoidLight::ResourceHandle itemHandle) {
    // Handle validation (fast)
    if (!itemHandle.isValid()) return false;
    
    const auto& rtm = ResourceTemplateManager::Instance();
    auto itemTemplate = rtm.getResourceTemplate(itemHandle);
    if (!itemTemplate) return false;
    
    // ... rest of logic
}
```

### Updating Method Signatures

| Component | Old Signature | New Signature |
|-----------|---------------|---------------|
| Equipment | `equipItem(const std::string& itemId)` | `equipItem(VoidLight::ResourceHandle handle)` |
| Inventory | `addResource(const std::string& name, int qty)` | `addResource(VoidLight::ResourceHandle handle, int qty)` |
| Trading | `canTrade(const std::string& itemId)` | `canTrade(VoidLight::ResourceHandle handle)` |
| Loot | `dropSpecificItem(const std::string& itemId)` | `dropSpecificItem(VoidLight::ResourceHandle handle)` |

## Error Handling

### Handle Validation
```cpp
void safeResourceOperation(VoidLight::ResourceHandle handle) {
    // Always validate handles before use
    if (!handle.isValid()) {
        GAME_ERROR("Invalid resource handle provided");
        return;
    }
    
    const auto& rtm = ResourceTemplateManager::Instance();
    if (!rtm.isValidHandle(handle)) {
        GAME_ERROR("Resource handle not found in template manager");
        return;
    }
    
    // Safe to use handle
    auto resource = rtm.getResourceTemplate(handle);
}
```

### Graceful Degradation
```cpp
int getResourceStackSize(VoidLight::ResourceHandle handle) {
    const auto& rtm = ResourceTemplateManager::Instance();
    
    // ResourceTemplateManager returns sensible defaults for invalid handles
    return rtm.getMaxStackSize(handle);  // Returns 1 for invalid handles
}
```

## Performance Monitoring

### Metrics to Track
```cpp
void performanceAudit() {
    const auto& rtm = ResourceTemplateManager::Instance();
    
    // Monitor resource system performance
    auto stats = rtm.getStats();
    size_t memoryUsage = rtm.getMemoryUsage();
    size_t templateCount = rtm.getResourceTemplateCount();
    
    PERF_LOG("Resource templates: %zu", templateCount);
    PERF_LOG("Memory usage: %zu bytes", memoryUsage);
    PERF_LOG("Templates loaded: %llu", stats.templatesLoaded.load());
    PERF_LOG("Resources created: %llu", stats.resourcesCreated.load());
}
```

### Profiling Name-Based Lookups
```cpp
// Use this to identify remaining string-based operations
#ifdef DEBUG
#define NAME_LOOKUP_WARNING(name) \
    GAME_DEBUG("String-based lookup detected: %s", (name).c_str())
#else
#define NAME_LOOKUP_WARNING(name)
#endif

ResourcePtr getResourceByName(const std::string& name) {
    NAME_LOOKUP_WARNING(name);
    // ... implementation
}
```

## See Also
- [ResourceTemplateManager Documentation](../managers/ResourceTemplateManager.md)
- [EntityDataManager Inventory APIs](../managers/EntityDataManager.md)
- [InventoryController](../controllers/InventoryController.md)
- [Resource System Tests](../../tests/resources/ResourceTemplateManagerTests.cpp)
