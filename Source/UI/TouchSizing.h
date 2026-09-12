#pragma once

namespace openguitarmultifx::touch
{

/**
    Minimum comfortable tap target, in logical pixels, for anything
    interactive anywhere in this app: buttons, menu rows, list rows,
    knobs, selector tiles. This product's actual target is a ~10" touch
    panel operated with a fingertip, not a mouse -- see AGENT.md's UI/UX
    Design Philosophy. Never size a tappable element below this on either
    axis, and prefer noticeably larger for primary/frequent actions. It's
    fine for a menu or list to end up physically large as a result --
    that's the point, not a problem to work around.
*/
constexpr int minTapTarget = 48;

} // namespace openguitarmultifx::touch
