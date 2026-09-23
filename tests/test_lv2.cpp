/* Copyright 2013-2015 Matt Tytel
 *
 * cursynth is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * cursynth is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with cursynth.  If not, see <http://www.gnu.org/licenses/>.
 */

/* A very small LV2 host, so the plugin is played rather than only compiled.
 *
 * It loads the binary the build just produced, reads the port defaults out of
 * the Turtle file the build just generated, sends a note on, and listens. Then
 * it moves the volume port and checks the level follows it, because a plugin
 * that loads and makes a noise but ignores its controls has only got halfway.
 */
#include <lv2/core/lv2.h>
#include <lv2/atom/atom.h>
#include <lv2/midi/midi.h>
#include <lv2/urid/urid.h>
#include <dlfcn.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>
#include <map>

static std::map<std::string, LV2_URID> g_urids;
static LV2_URID g_next = 1;
static LV2_URID map_uri(LV2_URID_Map_Handle, const char* uri) {
  if (!g_urids.count(uri)) g_urids[uri] = g_next++;
  return g_urids[uri];
}

int main(int argc, char** argv) {
  const char* so = argv[1];
  void* lib = dlopen(so, RTLD_NOW);
  if (!lib) { printf("dlopen failed: %s\n", dlerror()); return 1; }
  typedef const LV2_Descriptor* (*DescFn)(uint32_t);
  DescFn get = (DescFn)dlsym(lib, "lv2_descriptor");
  if (!get) { printf("no lv2_descriptor\n"); return 1; }
  const LV2_Descriptor* d = get(0);
  if (!d) { printf("no descriptor 0\n"); return 1; }
  printf("URI: %s\n", d->URI);

  LV2_URID_Map map = { 0, map_uri };
  LV2_Feature map_feature = { LV2_URID__map, &map };
  const LV2_Feature* features[] = { &map_feature, 0 };

  LV2_Handle h = d->instantiate(d, 44100.0, "", features);
  if (!h) { printf("instantiate returned nothing\n"); return 1; }

  const uint32_t N = 512;
  std::vector<float> left(N, 0.f), right(N, 0.f);
  std::vector<uint8_t> seq(4096, 0);

  /* Start every control where the plugin says its default is, then the one
   * under test can be moved on its own. */
  std::vector<float> controls;
  {
    FILE* f = fopen(argv[2], "r");
    if (!f) { printf("no defaults file\n"); return 1; }
    int idx; float dflt, mn, mx; char name[256];
    while (fscanf(f, "%d %f %f %f %255[^\n]\n", &idx, &dflt, &mn, &mx, name) == 5)
      controls.push_back(dflt);
    fclose(f);
  }
  int n_controls = (int)controls.size();
  const int override_index = argc > 3 ? atoi(argv[3]) : -1;
  const float override_value = argc > 4 ? atof(argv[4]) : 0.f;
  if (override_index >= 3 && override_index - 3 < n_controls)
    controls[override_index - 3] = override_value;
  // read the defaults the plugin advertises straight out of the ttl? keep it
  // simple: the plugin starts them at its own defaults, so feed those back by
  // connecting and leaving them, except make sure there is volume.
  d->connect_port(h, 0, seq.data());
  d->connect_port(h, 1, left.data());
  d->connect_port(h, 2, right.data());
  for (int i = 0; i < n_controls; i++) d->connect_port(h, 3 + i, &controls[i]);

  // an empty sequence first, so the control values get taken as given
  LV2_Atom_Sequence* s = (LV2_Atom_Sequence*)seq.data();
  s->atom.size = sizeof(LV2_Atom_Sequence_Body);
  s->atom.type = map_uri(0, LV2_ATOM__Sequence);
  s->body.unit = 0; s->body.pad = 0;
  if (d->run) d->run(h, N);

  // now a note on at frame 0
  const LV2_URID midi_urid = map_uri(0, LV2_MIDI__MidiEvent);
  uint8_t* p = seq.data() + sizeof(LV2_Atom_Sequence);
  LV2_Atom_Event* ev = (LV2_Atom_Event*)p;
  ev->time.frames = 0;
  ev->body.size = 3;
  ev->body.type = midi_urid;
  uint8_t* msg = (uint8_t*)(ev + 1);
  msg[0] = 0x90; msg[1] = 60; msg[2] = 100;
  s->atom.size = sizeof(LV2_Atom_Sequence_Body) + sizeof(LV2_Atom_Event) + 8;

  double peak = 0.0, settled = 0.0;
  for (int block = 0; block < 40; block++) {
    d->run(h, N);
    double bpk = 0.0;
    for (uint32_t i = 0; i < N; i++) bpk = fmax(bpk, fabs(left[i]));
    peak = fmax(peak, bpk);
    if (block >= 30) settled = fmax(settled, bpk);   /* once the ramp is over */
    s->atom.size = sizeof(LV2_Atom_Sequence_Body);   // only send it once
  }
  printf("peak %.6f   settled %.6f\n", peak, settled);
  const bool told_what_to_expect = argc > 5;
  const double want = told_what_to_expect ? atof(argv[5]) : 0.0;
  bool good;
  if (told_what_to_expect) {
    /* When a level was asked for, that is the whole question: silence is the
     * right answer for a volume of nothing, and the "did it make a noise"
     * check would call that a failure. */
    const double tol = want * 0.15 + 1e-4;
    good = fabs(settled - want) <= tol;
    if (!good)
      printf("FAIL: wanted about %.6f, got %.6f\n", want, settled);
  } else {
    good = settled > 1e-4;
  }
  peak = settled;
  bool stereo_same = true;
  for (uint32_t i = 0; i < N; i++) if (left[i] != right[i]) stereo_same = false;
  printf("both channels identical: %s\n", stereo_same ? "yes" : "no");
  d->cleanup(h);
  printf("%s\n", peak > 1e-4 ? "PLAYS" : "SILENT");
  return good ? 0 : 1;
}
