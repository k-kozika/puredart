#if defined(USE_NATIVE)
#include <purenative/hook.h>
#endif
#if defined(USE_DART)
#include <puredart/puredart.h>
#endif

int main() {
#if defined(USE_NATIVE)
    if (purenative::remove({})) return 1;
#endif
#if defined(USE_DART)
    if (!puredart::meta::validate(puredart::meta::puredart_bundle())) return 1;
    const puredart::vm::Object zero(0, 0);
    if (!zero.valid() || zero.integer() != 0) return 1;
#endif
#if defined(USE_DART_RUNTIME)
    if (!puredart::install() || !puredart::installed()) return 1;
    if (puredart::hook_capacity() != 16 || puredart::hook_count() != 0) return 1;
#endif
    return 0;
}
