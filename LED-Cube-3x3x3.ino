/** Animations, and framework for a 3x3x3 LED-Cube.
 *  
 *  Copyright (C) 2018 Thomas Friedrichsmeier
 *  
 *  This code was written for the 3x3x3-cube sold by German electronics discounter "Pollin".
 *  It is based on an ATtiny2313 . However it should be relatively easy to adjust the code
 *  to any other 3x3x3 cube with different wiring, and even different MCU, as the hardware
 *  dependent code is in a few dedicated functions, only. The code will be of rather little
 *  use for 4x4x4 or larger cubes, however, as it is quite optimized for th3 3x3x3 case.
 *  
 *  At the heart of this sketch a timer interrupt is used to "scan" through the three layers
 *  or LEDs. At any time, the LEDs on only one layer are lit up, but due to a fast refresh rate,
 *  and persistence of vision, the LEDs appear to be lit on all three layers at once, i.e.
 *  as 27 LEDs addressable in parallel.
 *  
 *  Since memory is scarce, an effort is made to store the animations in as few bytes as
 *  possible. Mostly we rely on "packed Frames", which include bits for all 27 LEDS, and
 *  a delay timing inside a 32bit storage. Two workhorse functions are provided in this
 *  file: playMovie() to play a sequence of arbitrary frames, and dropDown() for layers
 *  dropping down from the top.
 *
 *  Animations can also be "programmed" using for()-loops, custom functions, and whatever
 *  else you can think up. However, keep in mind that there is always a trade off between
 *  the flash saved on data, vs. the flash used for code.
 *  
 *  For low-level access to the scan buffer, the setLayer() function can be used.
 */

/** Hardware abstraction part 1:
 *  The following function map bits from a 12bit (well 16 bit) uint to the relevant
 *  output registers. The upper three bits signify the layer, the following nine bits
 *  signify the individual columns.
 *  
 *  I did not design the wiring, so don' ask me, why this particular arrangement of
 *  output pins was chosen.
 */
#define bitsToPortA(bits) (((bits & 0b00100000) >> 5) | ((bits & 0b100000000) >> 7))
#define bitsToPortB(bits) ((bits & 0b00011100) | ((bits & 0b00000001) << 1) | ((bits & 0b000000010) >> 1))
#define bitsToPortD(bits) (((bits & 0b111000000000) >> 6) | ((bits & 0b01000000) >> 5) | ((bits & 0b10000000) >> 1))
#define clearZ() (PORTD = 0);

/** Hardware abstraction part 2:
 *  Set all needed pins as output pins, and set up a timer (interrupt)
 */
void setup() {
  // set all output pins to output mode
  DDRA = bitsToPortA(0b111111111111);
  DDRB = bitsToPortB(0b111111111111);
  DDRD = bitsToPortD(0b111111111111);

  // set up a timer for refresh / scanning
  TCCR1A &= ~((1<<WGM11) | (1<<WGM10));
  TCCR1B |= (1<<WGM12);
  TCCR1B &= ~(1<<WGM13);
  TCCR1B |= (1<<CS11) | (1<<CS10);     // Prescaler 1/64
  TIMSK |= (1<<OCIE1A);
  OCR1A = 50;        // Timer compare. Lower value -> faster refresh
  sei();
}

/** The scan buffer. Somewhat hardware dependent, as for efficiency, we
 *  store the layers in terms of the output register values. The exact wiring
 *  is defined in the bitsToPortX() macros, above, however, so as long
 *  as three ports are enough, you don't have to change this.
 */
struct Layer {
  byte porta;
  byte portb;
  byte portd;
} layer[3];

/** Hardware abstraction part 3:
 *  The timer interrupt. On each iteration, we light up one layer from the buffer.
 *  One the next iteration, the next layer is lit up, etc. The "frame_delay" counter is
 *  descreased every three layers, i.e. once for each full scan. This can be used as
 *  a cheap delay().
 */
volatile byte frame_delay;
ISR(TIMER1_COMPA_vect) {
    static byte frame = 2;
    if (++frame > 2) {
      frame = 0;
      if (frame_delay) --frame_delay;
    }

    clearZ();  // avoid cross-talk, by clearing the layer transistors, first.
    PORTA = layer[frame].porta;
    PORTB = layer[frame].portb;
    PORTD = layer[frame].portd;
}

/**
 * Helper functions and structs - part1:
 * Packing and unpacking data. What this does and how to use this is best explained by example,
 * near the end of the sketch.
 */
#include <avr/pgmspace.h>
#define packed_movie_t const uint32_t

// compiler calculates this
#define packFrame(l1, l2, l3, duration) (uint32_t ((uint32_t) l1 | ((uint32_t) l2 << 9) | ((uint32_t) l3 << 18) | (duration > 31 ? (31 << 27) :(uint32_t) duration << 27)))

struct UnpackedFrame {
  uint16_t l1;
  uint16_t l2;
  uint16_t l3;
  uint8_t delay;
};

void setLayer(byte l, word bits);
void unpackFrame(packed_movie_t *movie, uint8_t frame, UnpackedFrame* buffer);

void unpackFrame(packed_movie_t *movie, uint8_t frame, UnpackedFrame* buffer) {
  uint32_t packed = pgm_read_dword_near(movie + frame);
  buffer->l1 = (packed >> 18) & (0x1FF);
  buffer->l2 = (packed >> 9) & (0x1FF);
  buffer->l3 = packed & (0x1FF);
  buffer->delay = (packed >> 27) << 3;
}

/** This function can be used to set a single layer in the current buffer.
 *  First parameter: layer (0, 1, or 2), second parameter the 9 bits for the
 *  nine LEDs in the layer.
 */
void setLayer(byte l, word bits) {
  bits |= 1 << (l + 9);
  layer[l].porta = bitsToPortA(bits);
  layer[l].portb = bitsToPortB(bits);
  layer[l].portd = bitsToPortD(bits);
}

/** 
 * Function to play a movie from packed frames. Parameters: Array of packed frames,
 * length of the array.
 * 
 * Using this function the first time adds around 114 bytes to your code size.
 */
void playMovie(packed_movie_t packed_frames[], byte length) {
  byte i = 0;
  while (i < length) {
   UnpackedFrame f;
   unpackFrame (packed_frames, i, &f);
   setLayer(0, f.l1);
   setLayer(1, f.l2);
   setLayer(2, f.l3);
   noInterrupts();
   frame_delay = f.delay;
   interrupts();
   while (frame_delay > 0) {};
   ++i;
  }
}

/** 
 * Function to play a drop-down movie from packed frames. Parameters: Array of packed frames,
 * length of the array.
 * 
 * Using this function the first time adds around 426 bytes to your code size, 
 * in considerable part due to the three modulo operations, which weigh in a 28 bytes, each!
 */
void dropDown (packed_movie_t packed_frames[], byte length) {
  // declaring these static, so it is possible to loop through an animation
  static uint16_t buffer[6] = {0};
  static uint8_t pos = 0;

  uint8_t i = 0;
  uint8_t frame_delay_buf = 32;
  while (i < length) {
    if (pos >= 6) pos = 0;
    {
      // load next frame into buffer
      UnpackedFrame f;
      unpackFrame (packed_frames, i, &f);
      uint8_t npos = pos + 3;
      if (npos > 3) npos = 0;
      buffer[npos] = f.l3;  // note: reversing here, so first layer defined in packFrame() shows first.
      buffer[npos+1] = f.l2;
      buffer[npos+2] = f.l1;
      // a matter of taste, but for dropDown(), faster looks better, IMO
      frame_delay_buf = f.delay >> 1;
      ++i;
    }

    // shift downwards
    for (int j = 3; j > 0; --j) {
      setLayer (0, buffer[pos % 6]);
      setLayer (1, buffer[(pos+1) % 6]);
      setLayer (2, buffer[(pos+2) % 6]);

      noInterrupts();
      frame_delay = frame_delay_buf;
      interrupts();
      while (frame_delay > 0) {};
      ++pos;
    }
  }
}

/** Here are some sample movies. The last parameter controls the speed. */
PROGMEM packed_movie_t movie1[] = { packFrame(0b111000000, 0b000000000, 0b000000000, 4),
                      packFrame(0b000000000, 0b111000000, 0b000000000, 4),
                      packFrame(0b000000000, 0b000000000, 0b111000000, 4),
                      packFrame(0b000000000, 0b000000000, 0b000111000, 4),
                      packFrame(0b000000000, 0b000000000, 0b000000111, 4),
                      packFrame(0b000000000, 0b000000111, 0b000000000, 4),
                      packFrame(0b000000111, 0b000000000, 0b000000000, 4),
                      packFrame(0b000111000, 0b000000000, 0b000000000, 4),
                      packFrame(0b000000000, 0b000111000, 0b000000000, 12),
                      };

PROGMEM packed_movie_t movie2[] = { packFrame(0b100000000, 0b010000000, 0b001000000, 2),
                      packFrame(0b000100000, 0b000010000, 0b000001000, 2),
                      packFrame(0b000000100, 0b000000010, 0b000000001, 2),
                    };

PROGMEM packed_movie_t movie3[] = { packFrame(0b111000000, 0b000111000, 0b000000111, 3),
                      packFrame(0b000000111, 0b000111000, 0b111000000, 3),
                    };

PROGMEM packed_movie_t movie4[] = {
                      packFrame(0, 0, 0, 4),
                      packFrame(0b101101101, 0, 0, 4),
                      packFrame(0b101010101, 0, 0, 4),
                      packFrame(0b101000101, 0, 0, 4),
                      packFrame(0b001010100, 0, 0, 4),
                      packFrame(0b001000100, 0, 0, 4),
                      packFrame(0b000010000, 0, 0, 4),
                      packFrame(0, 0, 0, 4),
                    };

/** And here's a short porgram playing the four movies. As you can see, the same movie data
 * can be used in several calls. */
void loop() {
  dropDown (movie4, 8);
  playMovie(movie1, 9);
  for (int i = 0; i < 8; ++i) {
    dropDown (movie2, 3);
  }
  for (int i = 0; i < 10; ++i) {
    dropDown (movie3, 2);
  }
  for (int i = 0; i < 10; ++i) {
    dropDown (movie3, 1);
  }
}

