# LED-Cube-3x3x3
This code was written for the 3x3x3-cube sold by German electronics discounter "Pollin".
It is based on an ATtiny2313 . However it should be relatively easy to adjust the code
to any other 3x3x3 cube with different wiring, and even different MCU, as the hardware
dependent code is in a few dedicated functions, only. The code will be of rather little
use for 4x4x4 or larger cubes, however, as it is quite optimized for th3 3x3x3 case.

## Basic design
At the heart of this sketch a timer interrupt is used to "scan" through the three layers
or LEDs. At any time, the LEDs on only one layer are lit up, but due to a fast refresh rate,
and persistence of vision, the LEDs appear to be lit on all three layers at once, i.e.
as 27 LEDs addressable in parallel.

## Programming animations
Since memory is scarce, an effort is made to store the animations in as few bytes as
possible. Mostly we rely on "packed Frames", which include bits for all 27 LEDS, and
a delay timing inside a 32bit storage. Two workhorse functions are provided in this
file: playMovie() to play a sequence of arbitrary frames, and dropDown() for layers
dropping down from the top.

Animations can also be "programmed" using for()-loops, custom functions, and whatever
else you can think up. However, keep in mind that there is always a trade off between
the flash saved on data, vs. the flash used for code. If you want to squeeze the most into
flash, it is recommended to compile often, keeping a close eye on the reported flash
usage after each change.

For low-level access to the scan buffer, the setLayer() function can be used.

## Further reading
Further documentation is available inline in the sketch.

## Licence and credits
Written Thomas Friedrichsmeier, provided under the GNU GPL v3.0. Should you find this
code useful, I'll be happy to accept donations via paypal to thomas.friedrichsmeier@gmx.de .

Should you port this to a different MCU, consider sharing your work.
