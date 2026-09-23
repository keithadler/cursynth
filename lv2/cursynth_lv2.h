/* Shared between the plugin and the program that writes its Turtle file, so
 * the two cannot disagree about which port is which. */
#ifndef CURSYNTH_LV2_H
#define CURSYNTH_LV2_H

#define CURSYNTH_URI "https://github.com/keithadler/cursynth"

#define CURSYNTH_PORT_MIDI_IN        0
#define CURSYNTH_PORT_OUT_LEFT       1
#define CURSYNTH_PORT_OUT_RIGHT      2
#define CURSYNTH_PORT_FIRST_CONTROL  3

#endif
