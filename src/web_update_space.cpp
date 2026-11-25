#include "web_update_space.h"
namespace WebUpdateSpace {
Estimate estimate(uint32_t total, uint32_t used, uint32_t payload) {
    Estimate result{};
    if (!total || used > total) { result.error = "storage-info"; return result; }
    result.freeBytes = total - used;
    if (!payload || payload > 384UL * 1024) { result.error = "payload-size"; return result; }
    result.requiredBytes = payload + ManifestReserve + AllocationReserve + OperationReserve;
    result.sufficient = result.freeBytes >= result.requiredBytes;
    if (!result.sufficient) result.error = "insufficient-space";
    return result;
}
}
