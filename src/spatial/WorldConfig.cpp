#include "WorldConfig.h"
#include "SpatialManager.h"

namespace VulkanMon {

BoundingBox WorldConfig::getBoundingBox() const {
    return BoundingBox(minBounds, maxBounds);
}

}