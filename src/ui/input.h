// PS4 HEN ULTIMATE - ui/input.h
// Turns raw pad states into logical actions: edge detection, key repeat for
// directions, analog stick with deadzone, and the X/O confirm swap.
#pragma once

#include <vector>

#include "platform/platform.h"

namespace henu {

enum class Action {
    None, Up, Down, Left, Right,
    Confirm,      // X (or O when swapped)
    Back,         // O (or X when swapped)
    PrevSection,  // L1
    NextSection,  // R1
    Context,      // Triangle
    Search,       // Square
    Menu,         // OPTIONS
};

const char* toString(Action a);

struct InputEvent {
    Action action = Action::None;
    bool repeat = false;
};

struct InputConfig {
    bool circleConfirms = false;
    float stickDeadzone = 0.25f;
    float repeatDelay = 0.35f;   // seconds
    float repeatRate = 0.09f;    // seconds
};

class InputMapper {
public:
    void configure(const InputConfig& c) { cfg_ = c; }
    const InputConfig& config() const { return cfg_; }

    // dt: seconds since the previous call.
    std::vector<InputEvent> update(const PadState& pad, float dt);

private:
    InputConfig cfg_;
    uint32_t prevLogical_ = 0;
    int heldDir_ = -1;          // index into direction list, -1 = none
    float heldTime_ = 0;
    float nextRepeat_ = 0;
};

}  // namespace henu
