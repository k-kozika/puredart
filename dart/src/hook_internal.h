#pragma once

#include "puredart/hook.h"

namespace puredart::detail {

void dispatch_hook(size_t slot, HookContext& context) noexcept;

#if defined(PUREDART_TESTING)
bool publish_hook_callback_for_testing(size_t slot, HookCallback callback) noexcept;
void clear_hook_callback_for_testing(size_t slot) noexcept;
#endif

}  // namespace puredart::detail
