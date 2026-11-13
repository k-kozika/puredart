#pragma once

#include "puredart/bootstrap.h"
#include "puredart/context.h"
#include "puredart/hook.h"
#include "puredart/meta.h"
#include "puredart/vm.h"

namespace puredart {

// Validates the linked metadata. The native hook engine needs no initialization.
// Repeated calls return the result of the first call.
bool install() noexcept;
bool installed() noexcept;

}  // namespace puredart
