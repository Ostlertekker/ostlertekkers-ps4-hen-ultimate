// OSTLERTEKKERS HEN - app/launch.h
// Animation shown when a game is started from the app: the scenery of the
// chosen boot style, the game's icon flying in, a rotating pentagram ring,
// the title, then an impact flash and a fade to black. Original artwork.
// The actual start (ILauncher) happens at launchAt(), on a black frame.
#pragma once

#include <string>

#include "app/boot.h"
#include "audio/audio.h"
#include "ui/draw.h"

namespace henu {

class LaunchSequence {
public:
    // icon may be 0 (no icon available).
    LaunchSequence(std::string styleId, std::string title, std::string titleId, TextureId icon);
    const std::string& style() const { return boot_.style(); }
    const std::string& titleId() const { return titleId_; }
    double hit() const { return kLaunchHitSeconds; }
    double launchAt() const { return 2.25; }    // screen is black from here
    double duration() const { return 2.4; }
    void draw(IRenderer& r, const Fonts& f, double t, const std::string& statusText) const;

private:
    BootSequence boot_;
    std::string title_, titleId_;
    TextureId icon_;
};

}  // namespace henu
