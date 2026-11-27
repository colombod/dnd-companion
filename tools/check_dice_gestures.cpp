#include "../firmware/Companion/dice_gestures.h"
#include <cassert>
#include <iostream>

int main() {
    play::DieHold hold;
    hold.begin(1, 0);
    for (unsigned now = 0; now <= 1600; now++)
        assert(hold.due(1, now) == (now == 500 || now == 1000 || now == 1500));
    assert(hold.consumed && !hold.due(-1, 1800) && !hold.due(1, 2100));
    hold.begin(2, 2200);
    assert(!hold.due(3, 2600) && !hold.due(2, 3000) && !hold.consumed);
    hold.begin(1, UINT32_MAX - 250);
    assert(!hold.due(1, 248) && hold.due(1, 249));
    assert(hold.due(1, 2800) && !hold.due(1, 2801));

    play::SpinMotion spin;
    spin.begin(1, 0);
    auto slow = spin.duration();
    spin.sample(1, 155);
    assert(spin.duration() == slow && spin.direction == 1);
    spin.sample(1, 175);
    assert(spin.duration() > slow && spin.duration() < 800);
    spin.begin(-1, 900);
    assert(spin.direction == -1 && spin.duration() == slow);
    spin.sample(-4, 980);
    assert(spin.strength == 4 && spin.direction == -1);
    spin.sample(1, 1200);
    assert(spin.direction == -1);
    std::cout << "Continuous hold, drift, cancellation, clock wrap and spin envelopes passed.\n";
}
