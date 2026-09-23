/* Copyright 2013-2015 Matt Tytel
 *
 * mopo is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * mopo is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with mopo.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#ifndef WAVE_H
#define WAVE_H

#include "mopo.h"
#include <cmath>
#include <cstdlib>

#define LOOKUP_SIZE 2048
#define HIGH_FREQUENCY 20000

/* How many partials the largest table holds. A sawtooth at 20 Hz wants a
 * thousand of them before it runs out of room under 20 kHz, which is the
 * lowest note anybody is going to play. */
#define MAX_PARTIALS 1024

/* Tables are spaced geometrically rather than one per partial count. Spacing
 * them evenly meant a hundred tables that between them only reached down to
 * 198 Hz, and every note below that fell back to a plain mathematical ramp:
 * bright, aliasing, and audibly a different oscillator from the one an inch
 * up the keyboard. Geometric spacing reaches 19.5 Hz in fewer tables and less
 * memory, and the step between neighbours is about a tenth of an octave
 * instead of the whole way down to nothing. */
#define NUM_TABLES 64

namespace mopo {

  class WaveLookup {
    public:
      WaveLookup() {
        for (int i = 0; i < LOOKUP_SIZE + 1; ++i)
          sin_[i] = sin((2 * PI * i) / LOOKUP_SIZE);

        /* the ladder of partial counts, 1 up to MAX_PARTIALS */
        for (int t = 0; t < NUM_TABLES; ++t) {
          int n = static_cast<int>(pow(MAX_PARTIALS * 1.0,
                                       t / (NUM_TABLES - 1.0)) + 0.5);
          partials_[t] = n < 1 ? 1 : n;
        }
        /* and the reverse: how many partials you may have, which table to use.
         * Always the largest table that stays at or under the limit, so no
         * table can ever put a partial above where the caller said to stop. */
        for (int want = 0; want <= MAX_PARTIALS; ++want) {
          int chosen = 0;
          for (int t = 0; t < NUM_TABLES; ++t) {
            if (partials_[t] <= want)
              chosen = t;
          }
          table_for_[want] = static_cast<unsigned char>(chosen);
        }

        /* Each table is the one before it plus the partials in between, so
         * the whole ladder costs one pass over the partials rather than one
         * pass per table. */
        for (int i = 0; i < LOOKUP_SIZE + 1; ++i) {
          mopo_float square_sum = 0.0, saw_sum = 0.0, triangle_sum = 0.0;
          int n = 1;
          for (int t = 0; t < NUM_TABLES; ++t) {
            for (; n <= partials_[t]; ++n) {
              const mopo_float s = sin_[(n * i) % LOOKUP_SIZE];

              /* a saw carries every partial, alternating in sign */
              saw_sum += (n % 2 ? s : -s) / n;

              /* a square and a triangle carry only the odd ones */
              if (n % 2) {
                square_sum += s / n;
                triangle_sum += ((n % 4) == 1 ? s : -s) / (n * 1.0 * n);
              }
            }
            square_[t][i] = (4.0 / PI) * square_sum;
            triangle_[t][i] = (8.0 / (PI * PI)) * triangle_sum;
            saw_[t][(i + (LOOKUP_SIZE / 2)) % LOOKUP_SIZE] =
                (2.0 / PI) * saw_sum;
          }
        }
        /* the extra entry the interpolation reads off the end */
        for (int t = 0; t < NUM_TABLES; ++t)
          saw_[t][LOOKUP_SIZE] = saw_[t][0];
      }

      /* Which table may be used when no partial is allowed above `partials`. */
      inline int tableFor(int partials) const {
        if (partials >= MAX_PARTIALS)
          return NUM_TABLES - 1;
        if (partials < 1)
          return 0;
        return table_for_[partials];
      }

      /* Where in a table a phase lands.
       *
       * The phase has to be wrapped here rather than trusted. A step wave asks
       * for the phase of a saw running several times as fast, and hands over a
       * number as large as eight, which used to be multiplied by the table
       * size and used as an index directly. That read thousands of entries
       * past the end of the row, landing in the middle of a different table:
       * not a crash, because the tables sit next to each other in one array,
       * but the wrong waveform, and it came out as a whistle on high notes. */
      inline int indexOf(mopo_float t, mopo_float& fractional) const {
        double integral;
        fractional = modf(t * LOOKUP_SIZE, &integral);
        int index = static_cast<int>(integral) % LOOKUP_SIZE;
        if (index < 0) {
          index += LOOKUP_SIZE;
          if (fractional < 0.0) {
            fractional += 1.0;
            index = (index + LOOKUP_SIZE - 1) % LOOKUP_SIZE;
          }
        }
        return index;
      }

      inline mopo_float fullsin(mopo_float t) const {
        mopo_float fractional;
        const int index = indexOf(t, fractional);
        return INTERPOLATE(sin_[index], sin_[index + 1], fractional);
      }

      inline mopo_float square(mopo_float t, int harmonics) const {
        const int tb = tableFor(harmonics + 1);
        mopo_float fractional;
        const int index = indexOf(t, fractional);
        return INTERPOLATE(square_[tb][index],
                           square_[tb][index + 1], fractional);
      }

      inline mopo_float upsaw(mopo_float t, int harmonics) const {
        const int tb = tableFor(harmonics + 1);
        mopo_float fractional;
        const int index = indexOf(t, fractional);
        return INTERPOLATE(saw_[tb][index],
                           saw_[tb][index + 1], fractional);
      }

      inline mopo_float downsaw(mopo_float t, int harmonics) const {
        return -upsaw(t, harmonics);
      }

      inline mopo_float triangle(mopo_float t, int harmonics) const {
        const int tb = tableFor(harmonics + 1);
        mopo_float fractional;
        const int index = indexOf(t, fractional);
        return INTERPOLATE(triangle_[tb][index],
                           triangle_[tb][index + 1], fractional);
      }

      template<size_t steps>
      inline mopo_float step(mopo_float t, int harmonics) const {
        /* A step wave is a saw plus a second saw running `steps` times as
         * fast, so the fast one may only have a `steps`th as many partials.
         * Dividing the index rather than the count left it with one partial
         * too many, and at the top of the keyboard that one partial was above
         * half the sample rate and folded back down as a whistle. Above
         * 20 kHz the fast saw has nothing left to say at all. */
        const int fast_partials = (harmonics + 1) / static_cast<int>(steps);
        const mopo_float scale = (1.0 * steps) / (steps - 1);
        if (fast_partials < 1)
          return scale * upsaw(t, harmonics);
        return scale * (upsaw(t, harmonics) +
               downsaw(steps * t, fast_partials - 1) / steps);
      }

      template<size_t steps>
      inline mopo_float pyramid(mopo_float t, int harmonics) const {
        size_t squares = steps - 1;
        mopo_float phase_increment = 1.0 / (2.0 * squares);

        mopo_float phase = 0.5 + t;
        mopo_float out = 0.0;

        double integral;
        for (size_t i = 0; i < squares; ++i) {
          out += square(modf(phase, &integral), harmonics);
          phase += phase_increment;
        }
        out /= squares;
        return out;
      }

    private:
      // Make them 1 larger for wrapping.
      mopo_float sin_[LOOKUP_SIZE + 1];
      mopo_float square_[NUM_TABLES][LOOKUP_SIZE + 1];
      mopo_float saw_[NUM_TABLES][LOOKUP_SIZE + 1];
      mopo_float triangle_[NUM_TABLES][LOOKUP_SIZE + 1];
      int partials_[NUM_TABLES];
      unsigned char table_for_[MAX_PARTIALS + 1];
  };

  class Wave {
    public:
      enum Type {
        kSin,
        kTriangle,
        kSquare,
        kDownSaw,
        kUpSaw,
        kThreeStep,
        kFourStep,
        kEightStep,
        kThreePyramid,
        kFivePyramid,
        kNinePyramid,
        kWhiteNoise,
        kNumWaveforms
      };

      static inline mopo_float blwave(Type waveform, mopo_float t,
                                      mopo_float frequency) {
        if (fabs(frequency) < 1)
          return wave(waveform, t);
        int harmonics = HIGH_FREQUENCY / fabs(frequency) - 1;

        switch (waveform) {
          case kSin:
            return lookup_.fullsin(t);
          case kTriangle:
            return lookup_.triangle(t, harmonics);
          case kSquare:
            return lookup_.square(t, harmonics);
          case kDownSaw:
            return lookup_.downsaw(t, harmonics);
          case kUpSaw:
            return lookup_.upsaw(t, harmonics);
          case kThreeStep:
            return lookup_.step<3>(t, harmonics);
          case kFourStep:
            return lookup_.step<4>(t, harmonics);
          case kEightStep:
            return lookup_.step<8>(t, harmonics);
          case kThreePyramid:
            return lookup_.pyramid<3>(t, harmonics);
          case kFivePyramid:
            return lookup_.pyramid<5>(t, harmonics);
          case kNinePyramid:
            return lookup_.pyramid<9>(t, harmonics);
          default:
            return wave(waveform, t);
        }
      }

      static inline mopo_float wave(Type waveform, mopo_float t) {
        switch (waveform) {
          case kSin:
            return fullsin(t);
          case kSquare:
            return square(t);
          case kTriangle:
            return triangle(t);
          case kDownSaw:
            return downsaw(t);
          case kUpSaw:
            return upsaw(t);
          case kThreeStep:
            return step<3>(t);
          case kFourStep:
            return step<4>(t);
          case kEightStep:
            return step<8>(t);
          case kThreePyramid:
            return pyramid<3>(t);
          case kFivePyramid:
            return pyramid<5>(t);
          case kNinePyramid:
            return pyramid<9>(t);
          case kWhiteNoise:
            return whitenoise();
          default:
            return 0;
        }
      }

      static inline mopo_float nullwave() {
        return 0;
      }

      static inline mopo_float whitenoise() {
        return (2.0 * rand()) / RAND_MAX - 1;
      }

      static inline mopo_float fullsin(mopo_float t) {
        return lookup_.fullsin(t);
      }

      static inline mopo_float square(mopo_float t) {
        return t < 0.5 ? 1 : -1;
      }

      static inline mopo_float triangle(mopo_float t) {
        double integral;
        return fabsf(2.0f - 4.0f * modf(t + 0.75f, &integral)) - 1;
      }

      static inline mopo_float downsaw(mopo_float t) {
        return -upsaw(t);
      }

      static inline mopo_float upsaw(mopo_float t) {
        return t * 2 - 1;
      }

      static inline mopo_float hannwave(mopo_float t) {
        return 0.5f * (1.0f - cosf(2.0f * PI * t));
      }

      template<size_t steps>
      static inline mopo_float step(mopo_float t) {
        mopo_float section = (int)(steps * t);
        return 2 * section / (steps - 1) - 1;
      }

      template<size_t steps>
      static inline mopo_float pyramid(mopo_float t) {
        size_t squares = steps - 1;
        mopo_float phase_increment = 1.0 / (2.0 * squares);

        mopo_float phase = 0.5 + t;
        mopo_float out = 0.0;

        double integral;
        for (size_t i = 0; i < squares; ++i) {
          out += square(modf(phase, &integral));
          phase += phase_increment;
        }
        out /= squares;
        return out;
      }

    protected:
      static const WaveLookup lookup_;
  };
} // namespace mopo

#endif // WAVE_H
