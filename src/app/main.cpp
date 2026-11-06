// OSTLERTEKKERS HEN - app/main.cpp
// Shared entry point for the PS4 build and the desktop build. The platform
// backend provides createPlatform() and platformSleepMs().
#ifdef HENU_HAS_MAIN

#include <time.h>

#include <cstdio>

#include "app/app.h"

namespace {
double nowSeconds() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<double>(ts.tv_sec) + ts.tv_nsec / 1e9;
}
}  // namespace

int main(int, char**) {
    using namespace henu;
    auto platform = createPlatform();
    if (!platform) {
        std::printf("[henu] platform init failed: %s\n", platform.error().message.c_str());
        return 1;
    }
    Platform p = std::move(platform.value());

    AppOptions opts;
    opts.bootSequence = true;
#ifdef HENU_PS4
    opts.onConsole = true;
#endif
    App app(p, opts);
    Status st = app.init();
    if (!st) {
        std::printf("[henu] init failed: %s\n", st.error().message.c_str());
        return 1;
    }

    double last = nowSeconds();
    bool running = true;
    while (running) {
        const double now = nowSeconds();
        running = app.frame(static_cast<float>(now - last));
        last = now;
        // When nothing was drawn there is no vsync wait: poll at ~60 Hz.
        if (!app.lastFrameDrawn()) platformSleepMs(16);
    }
    return 0;
}

#endif  // HENU_HAS_MAIN
