#pragma once
#include <stddef.h>
namespace WeatherDiagBudget {
constexpr size_t Stack = 6200;
constexpr size_t Input = 512 + 325;
constexpr size_t Output = 512 + 85;
constexpr size_t Reserve = 3072; // Policy, not a measured TLS overhead.
constexpr size_t minimum(size_t ssl, size_t x509, size_t client, bool stackAllocated) {
    return (stackAllocated ? 0 : Stack) + Input + Output + ssl + x509 + client;
}
constexpr size_t required(size_t ssl, size_t x509, size_t client, bool stackAllocated) {
    return minimum(ssl, x509, client, stackAllocated) + Reserve;
}
constexpr bool allowed(size_t heap, size_t block, size_t ssl, size_t x509, size_t client, bool stackAllocated) {
    return heap >= required(ssl, x509, client, stackAllocated) && block >= (stackAllocated ? 0 : Stack + 256) && block >= ssl + 256 && block >= x509 + 256;
}
}
