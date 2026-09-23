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
 * along with cursynth.  If not, see <http://www.gnu.org/licenses/>.
 */

/* The oscillator, judged by its spectrum.
 *
 * An oscillator is only worth what a listener hears, so these measure the
 * sound rather than the code: how much of the power sits at frequencies the
 * note has no business producing, and whether the tone changes smoothly as the
 * note climbs. Both checks come with a control that has to fail, because a
 * measurement with nothing that can fail it measures nothing.
 */

#include "wave.h"

#include <cmath>
#include <complex>
#include <cstdio>
#include <algorithm>
#include <cstdarg>
#include <vector>

namespace {

const double kRate = 44100.0;
const int kN = 1 << 15;          /* 32768, a power of two for the transform */

int passed = 0, failed = 0;

void ok(bool cond, const char* fmt, ...) {
  if (cond) { passed++; return; }
  failed++;
  va_list ap;
  va_start(ap, fmt);
  printf("  FAIL: ");
  vprintf(fmt, ap);
  printf("\n");
  va_end(ap);
}

/* Radix 2, in place. Small enough to read, which matters more here than speed. */
void fft(std::vector<std::complex<double> >& a) {
  const size_t n = a.size();
  for (size_t i = 1, j = 0; i < n; ++i) {
    size_t bit = n >> 1;
    for (; j & bit; bit >>= 1)
      j ^= bit;
    j ^= bit;
    if (i < j)
      std::swap(a[i], a[j]);
  }
  for (size_t len = 2; len <= n; len <<= 1) {
    const double ang = -2 * M_PI / len;
    const std::complex<double> wl(cos(ang), sin(ang));
    for (size_t i = 0; i < n; i += len) {
      std::complex<double> w(1.0, 0.0);
      for (size_t k = 0; k < len / 2; ++k) {
        const std::complex<double> u = a[i + k], v = a[i + k + len / 2] * w;
        a[i + k] = u + v;
        a[i + k + len / 2] = u - v;
        w *= wl;
      }
    }
  }
}

/* A frequency sitting exactly on a bin, so a pure tone lands in one place and
 * its skirt does not get counted as something the oscillator did wrong. */
double onBin(double freq) {
  const double bin = kRate / kN;
  return floor(freq / bin + 0.5) * bin;
}

enum Source { kBandLimited, kNaiveRamp, kPureSine };

std::vector<double> render(mopo::Wave::Type type, double freq, Source src) {
  std::vector<double> out(kN);
  double t = 0.0;
  const double inc = freq / kRate;
  for (int i = 0; i < kN; ++i) {
    if (src == kBandLimited)
      out[i] = mopo::Wave::blwave(type, t, freq);
    else if (src == kNaiveRamp)
      out[i] = mopo::Wave::wave(mopo::Wave::kUpSaw, t);
    else
      out[i] = mopo::Wave::wave(mopo::Wave::kSin, t);
    t += inc;
    if (t >= 1.0)
      t -= 1.0;
  }
  return out;
}

std::vector<double> power(const std::vector<double>& x) {
  std::vector<std::complex<double> > a(x.size());
  for (size_t i = 0; i < x.size(); ++i) {
    /* a Hann window, so a partial that is not exactly on a bin does not smear
     * across the whole spectrum and read as noise */
    const double w = 0.5 * (1.0 - cos(2 * M_PI * i / (x.size() - 1)));
    a[i] = std::complex<double>(x[i] * w, 0.0);
  }
  fft(a);
  std::vector<double> p(a.size() / 2 + 1);
  for (size_t i = 0; i < p.size(); ++i)
    p[i] = std::norm(a[i]);
  return p;
}

/* Share of the power that is not at the note or one of its partials. */
double offHarmonic(const std::vector<double>& x, double freq) {
  const std::vector<double> p = power(x);
  const double bin = kRate / kN;
  const double k0 = freq / bin;
  std::vector<bool> harmonic(p.size(), false);
  for (int h = 1; h * k0 < p.size(); ++h) {
    const int c = static_cast<int>(h * k0 + 0.5);
    for (int d = -2; d <= 2; ++d)
      if (c + d >= 0 && c + d < static_cast<int>(p.size()))
        harmonic[c + d] = true;
  }
  harmonic[0] = harmonic[1] = harmonic[2] = true;   /* leave DC out of it */
  double total = 0.0, off = 0.0;
  for (size_t i = 0; i < p.size(); ++i) {
    total += p[i];
    if (!harmonic[i])
      off += p[i];
  }
  return total > 0.0 ? 100.0 * off / total : 0.0;
}

/* Share of the power above 5 kHz: how bright the tone is. */
double brightness(const std::vector<double>& x) {
  const std::vector<double> p = power(x);
  const double bin = kRate / kN;
  double total = 0.0, high = 0.0;
  for (size_t i = 0; i < p.size(); ++i) {
    total += p[i];
    if (i * bin > 5000.0)
      high += p[i];
  }
  return total > 0.0 ? 100.0 * high / total : 0.0;
}

/* The amplitude at a given partial, relative to the loudest thing present. */
double partialLevel(const std::vector<double>& p, double freq, int n) {
  const double bin = kRate / kN;
  const int c = static_cast<int>(n * freq / bin + 0.5);
  if (c < 0 || c >= static_cast<int>(p.size()))
    return 0.0;
  double best = 0.0;
  for (int d = -2; d <= 2; ++d)
    if (c + d >= 0 && c + d < static_cast<int>(p.size()))
      best = std::max(best, p[c + d]);
  return sqrt(best);
}

/* The highest frequency still carrying anything, which says how far up the
 * waveform actually reaches. */
double topFrequency(const std::vector<double>& x) {
  const std::vector<double> p = power(x);
  double peak = 0.0;
  for (size_t i = 0; i < p.size(); ++i)
    peak = std::max(peak, p[i]);
  const double floor_level = peak * 1e-10;
  size_t top = 0;
  for (size_t i = 0; i < p.size(); ++i)
    if (p[i] > floor_level)
      top = i;
  return top * (kRate / kN);
}

const char* name(mopo::Wave::Type t) {
  switch (t) {
    case mopo::Wave::kSin: return "sine";
    case mopo::Wave::kTriangle: return "triangle";
    case mopo::Wave::kSquare: return "square";
    case mopo::Wave::kDownSaw: return "down saw";
    case mopo::Wave::kUpSaw: return "up saw";
    case mopo::Wave::kThreeStep: return "three step";
    case mopo::Wave::kFourStep: return "four step";
    case mopo::Wave::kEightStep: return "eight step";
    case mopo::Wave::kThreePyramid: return "three pyramid";
    case mopo::Wave::kFivePyramid: return "five pyramid";
    case mopo::Wave::kNinePyramid: return "nine pyramid";
    default: return "?";
  }
}

}  // namespace

int main() {
  printf("-- the measurement can tell right from wrong --\n");
  {
    const double f = onBin(440.0);
    const double sine = offHarmonic(render(mopo::Wave::kSin, f, kPureSine), f);
    const double ramp = offHarmonic(render(mopo::Wave::kUpSaw, f, kNaiveRamp), f);
    ok(sine < 0.01, "a pure sine reads %.4f%% off harmonic, should be nothing", sine);
    ok(ramp > 0.5, "a plain mathematical ramp reads %.4f%%, should be plenty", ramp);
    ok(ramp > sine * 50.0, "and the two are far apart (%.4f vs %.4f)", ramp, sine);
  }

  printf("-- no waveform puts power where the note cannot reach --\n");
  {
    const mopo::Wave::Type types[] = {
      mopo::Wave::kTriangle, mopo::Wave::kSquare, mopo::Wave::kDownSaw,
      mopo::Wave::kUpSaw, mopo::Wave::kThreeStep, mopo::Wave::kFourStep,
      mopo::Wave::kEightStep, mopo::Wave::kThreePyramid,
      mopo::Wave::kFivePyramid, mopo::Wave::kNinePyramid
    };
    /* 27.5 Hz is the lowest A on a piano; 3520 is the highest. */
    const double freqs[] = { 27.5, 55.0, 110.0, 165.0, 196.0, 220.0,
                             440.0, 880.0, 1760.0, 3520.0 };
    for (size_t w = 0; w < sizeof(types) / sizeof(types[0]); ++w) {
      double worst = 0.0;
      double worst_at = 0.0;
      for (size_t i = 0; i < sizeof(freqs) / sizeof(freqs[0]); ++i) {
        const double f = onBin(freqs[i]);
        const double a = offHarmonic(render(types[w], f, kBandLimited), f);
        if (a > worst) { worst = a; worst_at = f; }
      }
      ok(worst < 0.05, "%s: worst %.4f%% off harmonic, at %.0f Hz",
         name(types[w]), worst, worst_at);
    }
  }

  printf("-- the tone climbs with the note, with no step in it --\n");
  {
    /* A saw got brighter up to 198 Hz, then abruptly duller, because below
     * that it was not the same oscillator. Walking across that boundary is
     * what catches it coming back. */
    const double freqs[] = { 165.0, 175.0, 185.0, 192.0, 196.0, 199.0,
                             204.0, 210.0, 220.0 };
    const int n = sizeof(freqs) / sizeof(freqs[0]);
    double bright[n];
    for (int i = 0; i < n; ++i)
      bright[i] = brightness(render(mopo::Wave::kUpSaw, onBin(freqs[i]), kBandLimited));

    double worst_drop = 0.0;
    double worst_at = 0.0;
    for (int i = 1; i < n; ++i) {
      const double step = bright[i - 1] - bright[i];   /* positive means duller */
      if (step > worst_drop) { worst_drop = step; worst_at = freqs[i]; }
    }
    ok(worst_drop < 0.15,
       "worst drop in brightness while going up is %.3f points, at %.0f Hz",
       worst_drop, worst_at);
    ok(bright[n - 1] > bright[0],
       "220 Hz is brighter than 165 Hz (%.2f%% against %.2f%%)",
       bright[n - 1], bright[0]);
  }

  printf("-- every note on a piano is band limited, not just the high ones --\n");
  {
    /* The lowest A on a piano is 27.5 Hz and wants 727 partials before it
     * reaches 20 kHz. The tables have to go that far or the note is either
     * dull or aliasing. */
    double worst = 0.0;
    double worst_at = 0.0;
    for (double f = 27.5; f < 500.0; f *= 1.05) {
      const double fb = onBin(f);
      const double a = offHarmonic(render(mopo::Wave::kUpSaw, fb, kBandLimited), fb);
      if (a > worst) { worst = a; worst_at = fb; }
    }
    ok(worst < 0.05, "worst across the bottom four octaves is %.4f%%, at %.1f Hz",
       worst, worst_at);
  }

  printf("-- a square is a square, a saw is a saw --\n");
  {
    /* Aliasing checks cannot see this: a square built with every partial
     * instead of only the odd ones is still made of partials of the note, so
     * nothing lands off harmonic. It is simply not a square any more. */
    const double f = onBin(440.0);

    const std::vector<double> sq = power(render(mopo::Wave::kSquare, f, kBandLimited));
    const double sq1 = partialLevel(sq, f, 1);
    const double sq2 = partialLevel(sq, f, 2);
    const double sq3 = partialLevel(sq, f, 3);
    ok(sq2 < sq1 * 0.02, "a square has no second partial (%.4f against %.4f)",
       sq2 / sq1, 1.0);
    ok(fabs(sq3 / sq1 - 1.0 / 3.0) < 0.05,
       "a square's third partial is a third of its first (%.3f)", sq3 / sq1);

    const std::vector<double> sw = power(render(mopo::Wave::kUpSaw, f, kBandLimited));
    const double sw1 = partialLevel(sw, f, 1);
    ok(fabs(partialLevel(sw, f, 2) / sw1 - 1.0 / 2.0) < 0.05,
       "a saw falls off as one over n at the second partial (%.3f)",
       partialLevel(sw, f, 2) / sw1);
    ok(fabs(partialLevel(sw, f, 3) / sw1 - 1.0 / 3.0) < 0.05,
       "and at the third (%.3f)", partialLevel(sw, f, 3) / sw1);

    const std::vector<double> tr = power(render(mopo::Wave::kTriangle, f, kBandLimited));
    const double tr1 = partialLevel(tr, f, 1);
    ok(partialLevel(tr, f, 2) < tr1 * 0.02, "a triangle has no second partial");
    ok(fabs(partialLevel(tr, f, 3) / tr1 - 1.0 / 9.0) < 0.02,
       "a triangle falls off as one over n squared (%.4f, wanted %.4f)",
       partialLevel(tr, f, 3) / tr1, 1.0 / 9.0);
  }

  printf("-- a low note still reaches the top of the range --\n");
  {
    /* The other half of band limiting. Cutting everything above the
     * fundamental would pass every aliasing check ever written and leave the
     * bass sounding like a flute. */
    struct { double freq; double least; } wanted[] = {
      { 27.5, 15000.0 }, { 55.0, 17000.0 }, { 110.0, 18000.0 },
      { 220.0, 18000.0 }, { 440.0, 18000.0 }
    };
    for (size_t i = 0; i < sizeof(wanted) / sizeof(wanted[0]); ++i) {
      const double f = onBin(wanted[i].freq);
      const double top = topFrequency(render(mopo::Wave::kUpSaw, f, kBandLimited));
      ok(top > wanted[i].least,
         "a saw at %.1f Hz reaches %.0f Hz, wanted past %.0f",
         f, top, wanted[i].least);
    }
  }

  printf("-- the shape in time, not just the levels in the spectrum --\n");
  {
    /* A magnitude spectrum is blind to two things. Shuffle the signs of a
     * saw's partials and every level stays exactly where it was while the
     * shape stops being a ramp; halve the scale of a waveform and every ratio
     * between partials stays right while the thing gets quieter. Both of those
     * need looking at the samples. */
    struct Shape { mopo::Wave::Type type; const char* label; double peak; };
    const Shape shapes[] = {
      { mopo::Wave::kUpSaw,    "up saw",   1.156 },
      { mopo::Wave::kDownSaw,  "down saw", 1.156 },
      { mopo::Wave::kSquare,   "square",   1.179 },
      { mopo::Wave::kTriangle, "triangle", 0.990 },
      { mopo::Wave::kSin,      "sine",     1.000 }
    };
    for (size_t w = 0; w < sizeof(shapes) / sizeof(shapes[0]); ++w) {
      double lo = 1e9, hi = -1e9, dot = 0.0, ideal_sq = 0.0, got_sq = 0.0;
      for (int i = 0; i < 4096; ++i) {
        const double t = (i % 2048) / 2048.0;
        const double v = mopo::Wave::blwave(shapes[w].type, t, 440.0);
        lo = std::min(lo, v);
        hi = std::max(hi, v);

        double ideal = 0.0;
        switch (shapes[w].type) {
          case mopo::Wave::kUpSaw:   ideal = 2 * t - 1; break;
          case mopo::Wave::kDownSaw: ideal = 1 - 2 * t; break;
          case mopo::Wave::kSquare:  ideal = t < 0.5 ? 1.0 : -1.0; break;
          case mopo::Wave::kTriangle:
            ideal = t < 0.25 ? 4 * t : (t < 0.75 ? 2 - 4 * t : 4 * t - 4);
            break;
          default: ideal = sin(2 * M_PI * t); break;
        }
        dot += v * ideal;
        ideal_sq += ideal * ideal;
        got_sq += v * v;
      }
      const double corr = dot / sqrt(ideal_sq * got_sq);
      ok(corr > 0.99, "%s follows the shape it is named after (%.4f)",
         shapes[w].label, corr);
      ok(fabs(hi - shapes[w].peak) < 0.05 && fabs(lo + shapes[w].peak) < 0.05,
         "%s peaks near %+.3f, got %+.3f / %+.3f",
         shapes[w].label, shapes[w].peak, hi, lo);
    }
  }

  printf("\n%d passed, %d failed\n", passed, failed);
  return failed ? 1 : 0;
}
