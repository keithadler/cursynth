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

#pragma once
#ifndef CURSYNTH_GUI_H
#define CURSYNTH_GUI_H

#include "mopo.h"
#include "cursynth_common.h"

#include <map>
#include <ncurses.h>
#include <string>

namespace mopo {
  class Value;

  // Stores information on how and where to draw a control.
  struct DisplayDetails {
    int x, y, width;
    std::string label;
    bool bipolar;
  };

  class CursynthGui {
    public:
      enum ColorIds {
        BG_COLOR = 1,
        SLIDER_FG_COLOR,
        SLIDER_BG_COLOR,
        LOGO_COLOR,
        SELECTED_COLOR,
        PATCH_LOAD_COLOR,
        CONTROL_TEXT_COLOR
      };

      CursynthGui() : control_index_(0), pad_(0), pad_top_(0), pad_left_(0) { }

      // Start and stop the GUI.
      /* False when the terminal is too small to draw on, having already said
       * so and put the terminal back as it found it. */
      bool start();

      /* The size the display needs, so a caller can say what is wrong without
       * knowing how the screen is laid out. */
      static int neededWidth();
      static int neededHeight();

      /* True when there is room to draw. Checked again after a resize, since
       * a window can be made smaller while the program is running. */
      static bool fits();

      /* Fills the screen with the reason there is nothing else on it. */
      void drawTooSmall() const;

      /*
       * Everything is drawn into a pad the size of the whole display, and what
       * the terminal can show is a window onto it. That is what lets cursynth
       * run in a terminal smaller than its layout: the part you are working on
       * is brought into view instead of the rest being thrown away.
       */
      void showPad() const;

      /* Moves the view so that a rectangle of the layout is on screen. */
      void bringIntoView(int y, int x, int height, int width) const;
      void stop();
      void redrawBase();

      // Add all of the controls for drawing.
      void addControls(const control_map& controls);

      void drawHelp();
      void drawMain();
      void drawSlider(const DisplayDetails* slider,
                      float percentage, bool active);
      void drawControl(const Control* control, bool active);
      void drawControlStatus(const Control* control, bool armed);
      void drawPatchLoading(std::vector<std::string> patches, int index);
      void drawPatchSaving(std::string patch_name);

      void clearPatches();

      // Linear navigation of controls.
      std::string getCurrentControl();
      std::string getNextControl();
      std::string getPrevControl();

    private:
      void drawLogo();
      void drawModulationMatrix();
      void drawMidi(std::string status);
      void drawStatus(std::string status);
      void drawText(const DisplayDetails* details,
                    std::string text, bool active);

      // Creates control details and places control in the control order.
      DisplayDetails* initControl(std::string name, const Control* control);

      // Place a given control with text display at a location and width.
      void placeControl(std::string name, const Control* control,
                        int x, int y, int width);

      // Place a given control (slider only) at a location and width.
      void placeMinimalControl(std::string name, const Control* control,
                               int x, int y, int width);

      WINDOW* pad_;
      mutable int pad_top_, pad_left_;

      std::map<const Control*, DisplayDetails*> details_lookup_;
      std::vector<std::string> control_order_;
      int control_index_;
  };
} // namespace mopo

#endif // CURSYNTH_GUI_H
