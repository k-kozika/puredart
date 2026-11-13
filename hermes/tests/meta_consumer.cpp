#include <type_traits>

#include "counter_meta.h"

static_assert(std::is_same_v<decltype(purehermes_generated::updater),
                             const purehermes::FunctionSelector>);
static_assert(std::is_same_v<decltype(purehermes_generated::title),
                             const purehermes::StringSelector>);
static_assert(purehermes_generated::updater.id == 1);
static_assert(sizeof(purehermes_generated::bytes) == EXPECTED_BYTES);
