#include <purehermes/purehermes.h>
#ifdef WITH_DART
#include <puredart/puredart.h>
#endif

int main() {
    if (purehermes::supported_versions().size() != 4) return 1;
#ifdef WITH_DART
    if (puredart::hook_count() != 0) return 1;
#endif
    return 0;
}
