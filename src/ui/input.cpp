// OSTLERTEKKERS HEN - ui/input.cpp
#include "ui/input.h"

#include <cmath>

namespace henu {

const char* toString(Action a) {
    switch (a) {
        case Action::None: return "none";
        case Action::Up: return "up";
        case Action::Down: return "down";
        case Action::Left: return "left";
        case Action::Right: return "right";
        case Action::Confirm: return "confirm";
        case Action::Back: return "back";
        case Action::PrevSection: return "prev_section";
        case Action::NextSection: return "next_section";
        case Action::Context: return "context";
        case Action::Search: return "search";
        case Action::Menu: return "menu";
    }
    return "none";
}

namespace {
// Logical bit layout (internal).
enum : uint32_t {
    kUp = 1u << 0, kDown = 1u << 1, kLeft = 1u << 2, kRight = 1u << 3,
    kConfirm = 1u << 4, kBack = 1u << 5, kL1 = 1u << 6, kR1 = 1u << 7,
    kTri = 1u << 8, kSq = 1u << 9, kOpt = 1u << 10,
};
const uint32_t kDirBits[4] = {kUp, kDown, kLeft, kRight};
const Action kDirActions[4] = {Action::Up, Action::Down, Action::Left, Action::Right};

bool has(uint32_t mask, Button b) { return (mask & static_cast<uint32_t>(b)) != 0; }
}  // namespace

std::vector<InputEvent> InputMapper::update(const PadState& pad, float dt) {
    uint32_t cur = 0;
    const uint32_t b = pad.connected ? pad.buttons : 0;

    // Directions: D-pad has priority, stick only if D-pad idle.
    if (has(b, Button::Up)) cur |= kUp;
    if (has(b, Button::Down)) cur |= kDown;
    if (has(b, Button::Left)) cur |= kLeft;
    if (has(b, Button::Right)) cur |= kRight;
    if ((cur & (kUp | kDown | kLeft | kRight)) == 0 && pad.connected) {
        const float ax = std::fabs(pad.lx), ay = std::fabs(pad.ly);
        const float m = ax > ay ? ax : ay;
        if (m >= cfg_.stickDeadzone) {
            if (ax > ay) cur |= pad.lx > 0 ? kRight : kLeft;
            else cur |= pad.ly > 0 ? kDown : kUp;   // stick down = positive y
        }
    }
    // Only one direction at a time (avoid diagonal double moves).
    for (int i = 0; i < 4; ++i) {
        if (cur & kDirBits[i]) {
            cur &= ~(kUp | kDown | kLeft | kRight);
            cur |= kDirBits[i];
            break;
        }
    }

    const bool cross = has(b, Button::Cross), circle = has(b, Button::Circle);
    if (cfg_.circleConfirms) {
        if (circle) cur |= kConfirm;
        if (cross) cur |= kBack;
    } else {
        if (cross) cur |= kConfirm;
        if (circle) cur |= kBack;
    }
    if (has(b, Button::L1)) cur |= kL1;
    if (has(b, Button::R1)) cur |= kR1;
    if (has(b, Button::Triangle)) cur |= kTri;
    if (has(b, Button::Square)) cur |= kSq;
    if (has(b, Button::Options)) cur |= kOpt;

    std::vector<InputEvent> out;
    const uint32_t pressed = cur & ~prevLogical_;

    // Direction edge + repeat
    int dir = -1;
    for (int i = 0; i < 4; ++i) if (cur & kDirBits[i]) dir = i;
    if (dir < 0) {
        heldDir_ = -1;
    } else if (dir != heldDir_ || (pressed & kDirBits[dir])) {
        heldDir_ = dir;
        heldTime_ = 0;
        nextRepeat_ = cfg_.repeatDelay;
        out.push_back({kDirActions[dir], false});
    } else {
        heldTime_ += dt;
        int guard = 0;
        while (heldTime_ >= nextRepeat_ && guard++ < 4) {   // cap catch-up after a stall
            out.push_back({kDirActions[dir], true});
            nextRepeat_ += cfg_.repeatRate;
        }
        if (heldTime_ >= nextRepeat_) nextRepeat_ = heldTime_ + cfg_.repeatRate;
    }

    // Buttons: edge only
    struct M { uint32_t bit; Action a; } map[] = {
        {kConfirm, Action::Confirm}, {kBack, Action::Back}, {kL1, Action::PrevSection},
        {kR1, Action::NextSection},  {kTri, Action::Context}, {kSq, Action::Search},
        {kOpt, Action::Menu},
    };
    for (const auto& m : map)
        if (pressed & m.bit) out.push_back({m.a, false});

    prevLogical_ = cur;
    return out;
}

}  // namespace henu
