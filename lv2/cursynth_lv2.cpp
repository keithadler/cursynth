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

/* cursynth as an LV2 plugin.
 *
 * Asked for in 2014, in the second issue this project ever had, and still
 * sitting in the README's list of things to do. The engine was always separate
 * from the terminal: the ncurses side only ever drew controls and passed notes
 * along, so nothing had to be pulled apart to get here.
 *
 * The ports are not written out by hand. They are read off the engine's own
 * control map, and the Turtle file beside this is generated from the same
 * list, so a control added to the synth cannot appear in one and not in the
 * other. */

#include "cursynth_engine.h"
#include "cursynth_lv2.h"

#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/core/lv2.h>
#include <lv2/midi/midi.h>
#include <lv2/urid/urid.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace {

struct Cursynth {
  mopo::CursynthEngine engine;
  std::vector<mopo::Control*> controls;     /* in port order */
  std::vector<float*> control_ports;
  std::vector<float> last_seen;

  const LV2_Atom_Sequence* midi_in;
  float* out_left;
  float* out_right;

  LV2_URID midi_event_urid;

  Cursynth() : midi_in(0), out_left(0), out_right(0), midi_event_urid(0) {}
};

/* The engine's controls, always in the same order: the map is sorted by name,
 * so the plugin and the Turtle file agree without either having to be told. */
std::vector<std::string> controlNames(mopo::CursynthEngine& engine) {
  std::vector<std::string> names;
  mopo::control_map controls = engine.getControls();
  for (mopo::control_map::iterator it = controls.begin();
       it != controls.end(); ++it)
    names.push_back(it->first);
  return names;
}

LV2_Handle instantiate(const LV2_Descriptor*, double rate,
                       const char*, const LV2_Feature* const* features) {
  Cursynth* self = new Cursynth();

  LV2_URID_Map* map = 0;
  for (int i = 0; features && features[i]; ++i) {
    if (!strcmp(features[i]->URI, LV2_URID__map))
      map = static_cast<LV2_URID_Map*>(features[i]->data);
  }
  if (!map) {                 /* a host that cannot map URIs cannot send MIDI */
    delete self;
    return 0;
  }
  self->midi_event_urid = map->map(map->handle, LV2_MIDI__MidiEvent);

  self->engine.setSampleRate(static_cast<int>(rate));
  self->engine.setBufferSize(mopo::MAX_BUFFER_SIZE);

  mopo::control_map controls = self->engine.getControls();
  const std::vector<std::string> names = controlNames(self->engine);
  for (size_t i = 0; i < names.size(); ++i) {
    self->controls.push_back(controls[names[i]]);
    self->control_ports.push_back(0);
    self->last_seen.push_back(
        static_cast<float>(controls[names[i]]->current_value()));
  }
  return self;
}

void connect_port(LV2_Handle instance, uint32_t port, void* data) {
  Cursynth* self = static_cast<Cursynth*>(instance);
  switch (port) {
    case CURSYNTH_PORT_MIDI_IN:
      self->midi_in = static_cast<const LV2_Atom_Sequence*>(data);
      break;
    case CURSYNTH_PORT_OUT_LEFT:
      self->out_left = static_cast<float*>(data);
      break;
    case CURSYNTH_PORT_OUT_RIGHT:
      self->out_right = static_cast<float*>(data);
      break;
    default: {
      const uint32_t index = port - CURSYNTH_PORT_FIRST_CONTROL;
      if (index < self->control_ports.size())
        self->control_ports[index] = static_cast<float*>(data);
      break;
    }
  }
}

void handleMidi(Cursynth* self, const uint8_t* msg, uint32_t size) {
  if (size < 2)
    return;
  const int status = msg[0] & 0xf0;
  switch (status) {
    case LV2_MIDI_MSG_NOTE_ON:
      if (size > 2 && msg[2] > 0)
        self->engine.noteOn(msg[1], msg[2] / 127.0);
      else if (size > 2)
        self->engine.noteOff(msg[1]);        /* nothing on is something off */
      break;
    case LV2_MIDI_MSG_NOTE_OFF:
      self->engine.noteOff(msg[1]);
      break;
    case LV2_MIDI_MSG_BENDER:
      if (size > 2) {
        const int value = (msg[2] << 7) | msg[1];
        self->engine.setPitchWheel((value - 8192.0) / 8192.0);
      }
      break;
    case LV2_MIDI_MSG_CONTROLLER:
      if (size > 2) {
        if (msg[1] == LV2_MIDI_CTL_MSB_MODWHEEL)
          self->engine.setModWheel(msg[2] / 127.0);
        else if (msg[1] == LV2_MIDI_CTL_SUSTAIN)
          msg[2] >= 64 ? self->engine.sustainOn() : self->engine.sustainOff();
        else if (msg[1] == LV2_MIDI_CTL_ALL_NOTES_OFF ||
                 msg[1] == LV2_MIDI_CTL_ALL_SOUNDS_OFF) {
          /* The engine has no panic of its own, so lift the pedal and release
           * every note it could be holding. */
          self->engine.sustainOff();
          for (int note = 0; note < 128; ++note)
            self->engine.noteOff(note);
        }
      }
      break;
    default:
      break;
  }
}

void run(LV2_Handle instance, uint32_t n_samples) {
  Cursynth* self = static_cast<Cursynth*>(instance);
  if (!self->out_left || !self->out_right)
    return;

  /* A control the host moved is passed on before anything is played, so a note
   * is heard with the settings it was given rather than the ones before. */
  for (size_t i = 0; i < self->controls.size(); ++i) {
    if (!self->control_ports[i])
      continue;
    const float value = *self->control_ports[i];
    if (value != self->last_seen[i]) {
      self->controls[i]->set(value);
      self->last_seen[i] = value;
    }
  }

  uint32_t written = 0;
  const LV2_Atom_Event* event = 0;
  if (self->midi_in)
    event = lv2_atom_sequence_begin(&self->midi_in->body);

  while (written < n_samples) {
    /* Notes land on the sample the host said, not at the top of the block. */
    uint32_t until = n_samples;
    if (self->midi_in) {
      while (!lv2_atom_sequence_is_end(&self->midi_in->body,
                                       self->midi_in->atom.size, event)) {
        if (static_cast<uint32_t>(event->time.frames) > written) {
          until = static_cast<uint32_t>(event->time.frames);
          if (until > n_samples)
            until = n_samples;
          break;
        }
        if (event->body.type == self->midi_event_urid) {
          handleMidi(self, reinterpret_cast<const uint8_t*>(event + 1),
                     event->body.size);
        }
        event = lv2_atom_sequence_next(event);
      }
    }

    uint32_t todo = until - written;
    while (todo > 0) {
      const uint32_t block =
          todo > mopo::MAX_BUFFER_SIZE ? mopo::MAX_BUFFER_SIZE : todo;
      self->engine.setBufferSize(block);
      self->engine.process();
      const mopo::mopo_float* buffer = self->engine.output()->buffer;
      for (uint32_t i = 0; i < block; ++i) {
        const float v = static_cast<float>(buffer[i]);
        self->out_left[written + i] = v;
        self->out_right[written + i] = v;
      }
      written += block;
      todo -= block;
    }
    if (until >= n_samples)
      break;
  }
}

void cleanup(LV2_Handle instance) {
  delete static_cast<Cursynth*>(instance);
}

const LV2_Descriptor descriptor = {
  CURSYNTH_URI,
  instantiate,
  connect_port,
  0,            /* activate */
  run,
  0,            /* deactivate */
  cleanup,
  0             /* extension_data */
};

}  // namespace

extern "C" LV2_SYMBOL_EXPORT
const LV2_Descriptor* lv2_descriptor(uint32_t index) {
  return index == 0 ? &descriptor : 0;
}
