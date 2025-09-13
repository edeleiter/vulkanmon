#pragma once

#include <cstdint>

namespace VulkanMon {

// Simple Entity ID - just a 32-bit unsigned integer
using EntityID = uint32_t;

// Reserved invalid entity ID
static constexpr EntityID INVALID_ENTITY = 0;

// Component type identification
struct ComponentBase {
    static inline uint32_t nextTypeId = 1;

    template<typename T>
    static uint32_t getTypeId() {
        static uint32_t typeId = nextTypeId++;
        return typeId;
    }
};

// Macro to make component types easier to define
#define VKMON_COMPONENT(ClassName) \
    static uint32_t getComponentTypeId() { \
        return ComponentBase::getTypeId<ClassName>(); \
    }

} // namespace VulkanMon