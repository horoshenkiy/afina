#include <afina/allocator/Pointer.h>

namespace Afina {
namespace Allocator {



Pointer::Pointer(const Pointer &other) {
    _ptr = other._ptr;
}
Pointer::Pointer(Pointer &&other) {
    _ptr = other._ptr;
    other._ptr = nullptr;
}

Pointer &Pointer::operator=(const Pointer &other) {
    _ptr = other._ptr;
    return *this;
}
Pointer &Pointer::operator=(Pointer &&other) {
    _ptr = other._ptr;
    other._ptr = nullptr;
    return *this;
}

void* Pointer::get() const {
    return (_ptr == nullptr) ? nullptr : *_ptr;
}

} // namespace Allocator
} // namespace Afina
