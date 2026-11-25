#pragma once
#include <stdint.h>
namespace WebUpdateSpace {
constexpr uint32_t ManifestReserve = 8192;
constexpr uint32_t AllocationReserve = 65536;
constexpr uint32_t OperationReserve = 131072;
struct Estimate {
    uint32_t freeBytes;
    uint32_t requiredBytes;
    bool sufficient;
    const char* error;
};
// Pure calculation, no filesystem access or modification.
Estimate estimate(uint32_t total, uint32_t used, uint32_t payload);
}
