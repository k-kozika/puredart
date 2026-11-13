#include <android/log.h>

#include <memory>
#include <string>

#include "counter_meta.h"
#include "purehermes/purehermes.h"
extern "C" __attribute__((visibility("default"))) int purehermes_demo_start(uintptr_t base) {
    try {
        purehermes::HookPlan plan(purehermes_generated::bundle());
        plan.on_return(purehermes_generated::counter_updater,
                       [](purehermes::ReturnContext& c) { c.set_number(c.number() + 9); });
        plan.map_literal(purehermes_generated::counter_title, "+10");
        auto result = plan.install(purehermes::module_at(base));
        __android_log_print(result ? ANDROID_LOG_INFO : ANDROID_LOG_ERROR, "PurehermesDemo", "%s",
                            result.message.c_str());
        return result ? 0 : -1;
    } catch (const std::exception& error) {
        __android_log_print(ANDROID_LOG_ERROR, "PurehermesDemo", "%s", error.what());
        return -2;
    }
}
