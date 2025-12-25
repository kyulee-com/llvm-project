// Provide __hash_memory implementation for ThinLTO cross-compilation builds
#include <functional>

_LIBCPP_BEGIN_NAMESPACE_STD

// Provide the missing symbol by implementing it using the inline hash functions
size_t __hash_memory(_LIBCPP_NOESCAPE const void* ptr, size_t size) _NOEXCEPT {
  return __murmur2_or_cityhash<size_t>()(ptr, size);
}

_LIBCPP_END_NAMESPACE_STD
