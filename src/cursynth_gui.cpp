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

#include "cursynth_gui.h"

#include "value.h"

#include "nls.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ncurses.h>
#include <sstream>
#include <unistd.h>

#define gettext_noop(String) String

#define WIDTH 120
#define HEIGHT 44

/* The least a terminal can be and still show something worth looking at. The
 * display is bigger than this and scrolls; these are the numbers below which
 * there is no point trying. */
#define MIN_WIDTH 24
#define MIN_HEIGHT 6

#define LOGO_WIDTH 44
#define LOGO_Y 0
#define SPACE 3
#define TOTAL_COLUMNS 3
#define MAX_STATUS_SIZE 20
#define MAX_SAVE_SIZE 40
#define SAVE_COLUMN 2
#define PATCH_BROWSER_ROWS 5
#define PATCH_BROWSER_WIDTH 26

namespace mopo {

  void CursynthGui::drawHelp() {
    werase(pad_);
    drawLogo();
    wmove(pad_, 7, 41);
    wattron(pad_, A_BOLD);
    wprintw(pad_, gettext("INFO"));
    wattroff(pad_, A_BOLD);
    wmove(pad_, 8, 43);
    wprintw(pad_, gettext("version"));
    wprintw(pad_, " - ");
    wprintw(pad_, VERSION);
    wmove(pad_, 9, 43);
    wprintw(pad_, gettext("website"));
    wprintw(pad_, " - gnu.org/software/cursynth");
    wmove(pad_, 10, 43);
    wprintw(pad_, gettext("contact"));
    wprintw(pad_, " - ");
    wprintw(pad_, PACKAGE_BUGREPORT);
    wmove(pad_, 12, 41);
    wattron(pad_, A_BOLD);
    wprintw(pad_, gettext("CONTROLS"));
    wattroff(pad_, A_BOLD);
    wmove(pad_, 14, 43);
    wprintw(pad_, "awsedftgyhujkolp;' - ");
    wprintw(pad_, gettext("a playable keyboard"));
    wmove(pad_, 16, 43);
    wprintw(pad_, "`1234567890 - ");
    wprintw(pad_, gettext("a slider for the current selected control"));
    wmove(pad_, 18, 43);
    wprintw(pad_, gettext("up/down"));
    wprintw(pad_, " - ");
    wprintw(pad_, gettext("previous/next control"));
    wmove(pad_, 20, 43);
    wprintw(pad_, gettext("left/right"));
    wprintw(pad_, " - ");
    wprintw(pad_, gettext("decrement/increment control"));
    wmove(pad_, 22, 43);
    wprintw(pad_, gettext("F1 (or [shift] + H)"));
    wprintw(pad_, " - ");
    wprintw(pad_, gettext("help/controls"));
    wmove(pad_, 24, 43);
    wprintw(pad_, gettext("[shift] + L"));
    wprintw(pad_, " - ");
    wprintw(pad_, gettext("browse/load patches"));
    wmove(pad_, 26, 43);
    wprintw(pad_, gettext("[shift] + S"));
    wprintw(pad_, " - ");
    wprintw(pad_, gettext("save patch"));
    wmove(pad_, 28, 43);
    wprintw(pad_, "m - ");
    wprintw(pad_, gettext("arm MIDI learn"));
    wmove(pad_, 30, 43);
    wprintw(pad_, "c - ");
    wprintw(pad_, gettext("erase MIDI learn"));
  }

  void CursynthGui::drawMain() {
    werase(pad_);
    drawLogo();
    drawModulationMatrix();
  }

  void CursynthGui::drawLogo() {
    wattron(pad_, A_BOLD);
    wattron(pad_, COLOR_PAIR(LOGO_COLOR));
    int logo_x = (WIDTH - LOGO_WIDTH) / 2 + 1;

    wmove(pad_, LOGO_Y, logo_x);
    wprintw(pad_, "                                    __  __");
    wmove(pad_, LOGO_Y + 1, logo_x);
    wprintw(pad_, "  _______  ______________  ______  / /_/ /_");
    wmove(pad_, LOGO_Y + 2, logo_x);
    wprintw(pad_, " / ___/ / / / ___/ ___/ / / / __ \\/ __/ __ \\");
    wmove(pad_, LOGO_Y + 3, logo_x);
    wprintw(pad_, "/ /__/ /_/ / /  /__  / /_/ / / / / /_/ / / /");
    wmove(pad_, LOGO_Y + 4, logo_x);
    wprintw(pad_, "\\___/\\____/_/  /____/\\__  /_/ /_/\\__/_/ /_/");
    wmove(pad_, LOGO_Y + 5, logo_x);
    wprintw(pad_, "                    /____/");

    wattroff(pad_, A_BOLD);
    wattroff(pad_, COLOR_PAIR(LOGO_COLOR));

    wmove(pad_, LOGO_Y + 5, logo_x + 33);
    wprintw(pad_, "Matt Tytel");
  }

  void CursynthGui::drawModulationMatrix() {
    wmove(pad_, 34, 26);
    wattron(pad_, A_BOLD);
    wprintw(pad_, "----------------------------------------------------------------------");
    wmove(pad_, 34, 53);
    wprintw(pad_, gettext("Modulation Matrix"));
    wattroff(pad_, A_BOLD);

    wmove(pad_, 35, 26);
    wprintw(pad_, "                     ");
    wmove(pad_, 35, 34);
    wprintw(pad_, gettext("source"));
    wmove(pad_, 35, 50);
    wprintw(pad_, "                     ");
    wmove(pad_, 35, 58);
    wprintw(pad_, gettext("scale"));
    wmove(pad_, 35, 74);
    wprintw(pad_, "                     ");
    wmove(pad_, 35, 79);
    wprintw(pad_, gettext("destination"));
  }

  void CursynthGui::drawMidi(std::string status) {
    wmove(pad_, 2, 2);
    wprintw(pad_, gettext("MIDI Learn: "));
    wattron(pad_, A_BOLD);
    whline(pad_, ' ', MAX_STATUS_SIZE);
    wprintw(pad_, status.substr(0, MAX_STATUS_SIZE).c_str());
    wattroff(pad_, A_BOLD);
    showPad();
  }

  void CursynthGui::drawStatus(std::string status) {
    wmove(pad_, 1, 2);
    wprintw(pad_, gettext("Current Value: "));
    wattron(pad_, A_BOLD);
    whline(pad_, ' ', MAX_STATUS_SIZE);
    wprintw(pad_, gettext(status.substr(0, MAX_STATUS_SIZE).c_str()));
    wattroff(pad_, A_BOLD);
    showPad();
  }

  void CursynthGui::clearPatches() {
    int selection_row = (PATCH_BROWSER_ROWS - 1) / 2;
    wmove(pad_, 1 + selection_row, 83);
    whline(pad_, ' ', PATCH_BROWSER_WIDTH);
    for (int i = 0; i < PATCH_BROWSER_ROWS; ++i) {
      wmove(pad_, 1 + i, 94);
      whline(pad_, ' ', PATCH_BROWSER_WIDTH);
    }
  }

  void CursynthGui::drawPatchSaving(std::string patch_name) {
    int selection_row = (PATCH_BROWSER_ROWS - 1) / 2;
    wmove(pad_, 1 + selection_row, 83);
    wprintw(pad_, "            ");
    whline(pad_, ' ', PATCH_BROWSER_WIDTH);
    wmove(pad_, 1 + selection_row, 83);
    wprintw(pad_, gettext("Save Patch: "));
    wprintw(pad_, patch_name.c_str());
  }

  void CursynthGui::drawPatchLoading(std::vector<std::string> patches,
                                     int selected_index) {
    int selection_row = (PATCH_BROWSER_ROWS - 1) / 2;
    wmove(pad_, 1 + selection_row, 83);
    wprintw(pad_, gettext("Load Patch:"));

    int patch_index = selected_index - selection_row;
    int num_patches = patches.size();
    for (int i = 0; i < PATCH_BROWSER_ROWS; ++i) {
      if (i % 2)
        wattroff(pad_, COLOR_PAIR(PATCH_LOAD_COLOR));
      else
        wattron(pad_, COLOR_PAIR(PATCH_LOAD_COLOR));

      wmove(pad_, 1 + i, 94);
      whline(pad_, ' ', PATCH_BROWSER_WIDTH);
      if (patch_index == selected_index)
        wattron(pad_, A_BOLD);

      if (patch_index >= 0 && patch_index < num_patches)
        wprintw(pad_, patches[patch_index].c_str());
      wattroff(pad_, A_BOLD);
      patch_index++;
    }
    wattroff(pad_, COLOR_PAIR(PATCH_LOAD_COLOR));
    showPad();
  }

  void CursynthGui::drawSlider(const DisplayDetails* slider,
                               float percentage, bool active) {
    int y = slider->y;
    if (slider->label.size())
      y += 1;

    // Clear slider.
    wmove(pad_, y, slider->x - 1);
    wattron(pad_, COLOR_PAIR(BG_COLOR));
    whline(pad_, ' ', slider->width + 2);

    char slider_char = active ? '=' : ' ';
    wmove(pad_, y, slider->x);
    wattron(pad_, COLOR_PAIR(SLIDER_BG_COLOR));
    whline(pad_, slider_char, slider->width);

    // If active draw a bit different.
    if (active) {
      wmove(pad_, y, slider->x - 1);
      wattron(pad_, COLOR_PAIR(LOGO_COLOR));
      whline(pad_, '|', 1);
      wmove(pad_, y, slider->x + slider->width);
      whline(pad_, '|', 1);
    }

    // Find slider position.
    int position = round(slider->width * percentage);
    int slider_midpoint = (slider->width + 1) / 2;

    wattron(pad_, COLOR_PAIR(SLIDER_FG_COLOR));
    if (slider->bipolar) {
      if (position < slider_midpoint) {
        wmove(pad_, y, slider->x + position);
        whline(pad_, ' ', slider_midpoint - position);
      }
      else {
        wmove(pad_, y, slider->x + slider_midpoint);
        whline(pad_, ' ', position - slider_midpoint);
      }
    }
    else {
      wmove(pad_, y, slider->x);
      whline(pad_, ' ', position);
    }

    wattroff(pad_, COLOR_PAIR(SLIDER_FG_COLOR));
    showPad();
  }

  void CursynthGui::drawText(const DisplayDetails* details,
                             std::string text, bool active) {
    int y = details->y;
    if (details->label.size())
      y += 1;

    // Clear area.
    wmove(pad_, y, details->x - 1);
    wattron(pad_, COLOR_PAIR(BG_COLOR));
    whline(pad_, ' ', details->width + 2);
    wmove(pad_, y, details->x);
    wattron(pad_, COLOR_PAIR(CONTROL_TEXT_COLOR));
    whline(pad_, ' ', details->width);

    // If active draw a bit different.
    if (active) {
      wmove(pad_, y, details->x - 1);
      wattron(pad_, COLOR_PAIR(LOGO_COLOR));
      whline(pad_, '|', 1);
      wmove(pad_, y, details->x + details->width);
      whline(pad_, '|', 1);
      wattron(pad_, A_BOLD);
    }

    // Draw text.
    wattron(pad_, COLOR_PAIR(CONTROL_TEXT_COLOR));
    wmove(pad_, y, details->x);
    wprintw(pad_, gettext(text.c_str()));
    wattroff(pad_, A_BOLD);
    wattroff(pad_, COLOR_PAIR(CONTROL_TEXT_COLOR));
  }

  void CursynthGui::drawControl(const Control* control, bool active) {
    DisplayDetails* details = details_lookup_[control];
    if (!details)
      return;

    /* The one being changed is brought into view, along with the row above it
     * that carries its name and the row below that carries its value. */
    if (active)
      bringIntoView(details->y - 1, details->x, 4, details->width);

    // Draw label.
    if (details->label.size()) {
      if (active)
        wattron(pad_, A_BOLD);
      wmove(pad_, details->y, details->x);
      wprintw(pad_, gettext(details->label.c_str()));
      wattroff(pad_, A_BOLD);
    }

    // Draw status.
    if (control->display_strings().size()) {
      int display_index = static_cast<int>(control->current_value());
      drawText(details, control->display_strings()[display_index], active);
    }
    else
      drawSlider(details, control->getPercentage(), active);
  }

  void CursynthGui::drawControlStatus(const Control* control,
                                      bool midi_armed) {
    std::ostringstream midi_learn;
    if (midi_armed)
      midi_learn << "ARMED";
    else if (control->midi_learn())
      midi_learn << control->midi_learn();
    else
      midi_learn << "-";
    drawMidi(midi_learn.str());

    std::ostringstream status;
    if (control->display_strings().size()) {
      int display_index = static_cast<int>(control->current_value());
      status << control->display_strings()[display_index];
    }
    else
      status << control->current_value();
    drawStatus(status.str());
  }

  int CursynthGui::neededWidth() { return WIDTH; }

  int CursynthGui::neededHeight() { return HEIGHT; }

  /* The display is drawn at its full size whatever the terminal is, and the
   * terminal shows as much of it as it has room for. All that is needed is
   * enough space to see something and to read the label of what you are
   * changing. */
  bool CursynthGui::fits() { return COLS >= MIN_WIDTH && LINES >= MIN_HEIGHT; }

  void CursynthGui::showPad() const {
    if (pad_ == 0)
      return;
    const int view_h = std::min(LINES, HEIGHT);
    const int view_w = std::min(COLS, WIDTH);
    pad_top_ = CLAMP(pad_top_, 0, HEIGHT - view_h);
    pad_left_ = CLAMP(pad_left_, 0, WIDTH - view_w);
    prefresh(pad_, pad_top_, pad_left_, 0, 0, view_h - 1, view_w - 1);
  }

  /* Scrolls only as far as it has to, so the display stays where it was put
   * whenever what you are reaching for is already on screen. */
  void CursynthGui::bringIntoView(int y, int x, int height, int width) const {
    const int view_h = std::min(LINES, HEIGHT);
    const int view_w = std::min(COLS, WIDTH);

    if (y < pad_top_)
      pad_top_ = y;
    else if (y + height > pad_top_ + view_h)
      pad_top_ = y + height - view_h;

    if (x < pad_left_)
      pad_left_ = x;
    else if (x + width > pad_left_ + view_w)
      pad_left_ = x + width - view_w;
  }

  /* A terminal too small for the display used to get the display anyway.
   * curses drops whatever falls outside the screen without complaining, so
   * what came up was a scattering of half drawn controls and no hint that the
   * size was the problem. Saying so is not the same as fitting in less room,
   * but it is the difference between a program that looks broken and one that
   * tells you what it needs. */
  void CursynthGui::drawTooSmall() const {
    erase();
    const char* line1 = "cursynth needs a bigger terminal";
    char line2[80], line3[80];
    snprintf(line2, sizeof line2, "it needs %d by %d, and this one is %d by %d",
             WIDTH, HEIGHT, COLS, LINES);
    snprintf(line3, sizeof line3, "%s",
             "make the window bigger, or make the font smaller");
    const int mid = LINES / 2;
    if (mid - 1 >= 0 && COLS > 0) {
      mvprintw(mid - 1, std::max(0, (COLS - (int)strlen(line1)) / 2), "%.*s",
               COLS, line1);
      mvprintw(mid, std::max(0, (COLS - (int)strlen(line2)) / 2), "%.*s",
               COLS, line2);
      mvprintw(mid + 1, std::max(0, (COLS - (int)strlen(line3)) / 2), "%.*s",
               COLS, line3);
    }
    refresh();
  }

  bool CursynthGui::start() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    // Make sure we have color.
    if(has_colors() == FALSE) {
      endwin();
      printf("Your terminal does not support color\n");
      return false;
    }

    /* Said before anything is drawn, and with the terminal put back the way it
     * was found, so the message survives on the screen instead of being wiped
     * by curses on the way out. */
    if (!fits()) {
      const int cols = COLS, lines = LINES;
      endwin();
      printf("cursynth needs a terminal of at least %d by %d.\n"
             "This one is %d by %d.\n"
             "Make the window bigger, or make the font smaller.\n",
             MIN_WIDTH, MIN_HEIGHT, cols, lines);
      return false;
    }

    /* The whole display, whatever the terminal is. */
    pad_ = newpad(HEIGHT, WIDTH);
    if (pad_ == 0) {
      endwin();
      printf("cursynth could not make room to draw in.\n");
      return false;
    }
    keypad(pad_, TRUE);

    // Setup gettext for internationalization.
    setlocale(LC_ALL, "");
    bindtextdomain("cursynth", "/usr/share/locale");
    textdomain("cursynth");

    // Prepare all the color schemes.
    start_color();
    init_pair(BG_COLOR, COLOR_BLACK, COLOR_BLACK);
    init_pair(SLIDER_FG_COLOR, COLOR_WHITE, COLOR_YELLOW);
    init_pair(SLIDER_BG_COLOR, COLOR_YELLOW, COLOR_WHITE);
    init_pair(LOGO_COLOR, COLOR_RED, COLOR_BLACK);
    init_pair(PATCH_LOAD_COLOR, COLOR_BLACK, COLOR_CYAN);
    init_pair(CONTROL_TEXT_COLOR, COLOR_BLACK, COLOR_WHITE);

    // Start initial drawing.
    redrawBase();

    return true;
  }

  void CursynthGui::stop() {
    endwin();
  }

  void CursynthGui::redrawBase() {
    werase(pad_);
    showPad();
    drawLogo();
    drawModulationMatrix();
    curs_set(0);
  }

  DisplayDetails* CursynthGui::initControl(std::string name,
                                           const Control* control) {
    std::map<const Control*, DisplayDetails*>::iterator found =
        details_lookup_.find(control);

    // If we've already created this control, just return that one.
    if (found != details_lookup_.end())
      return found->second;

    control_order_.push_back(name);
    return new DisplayDetails();
  }

  void CursynthGui::placeMinimalControl(std::string name,
                                        const Control* control,
                                        int x, int y, int width) {
    DisplayDetails* details = initControl(name, control);
    details->x = x;
    details->y = y;
    details->width = width;
    details->label = "";
    details->bipolar = control->isBipolar();

    details_lookup_[control] = details;
    drawControl(control, false);
  }

  void CursynthGui::placeControl(std::string name, const Control* control,
                                 int x, int y, int width) {
    DisplayDetails* details = initControl(name, control);
    details->x = x;
    details->y = y;
    details->width = width;
    details->label = name;
    details->bipolar = control->isBipolar();

    details_lookup_[control] = details;
    drawControl(control, false);
  }

  void CursynthGui::addControls(const control_map& controls) {
    // Oscillators.
    placeControl(gettext_noop("osc 1 waveform"),
                 controls.at("osc 1 waveform"),
                 2, 7, 18);
    placeControl(gettext_noop("osc 2 waveform"),
                 controls.at("osc 2 waveform"),
                 22, 7, 18);
    placeControl(gettext_noop("cross modulation"),
                 controls.at("cross modulation"),
                 2, 10, 18);
    placeControl(gettext_noop("osc mix"),
                 controls.at("osc mix"),
                 22, 10, 18);
    placeControl(gettext_noop("osc 2 transpose"),
                 controls.at("osc 2 transpose"),
                 2, 13, 18);
    placeControl(gettext_noop("osc 2 tune"),
                 controls.at("osc 2 tune"),
                 22, 13, 18);

    // LFOs.
    placeControl(gettext_noop("lfo 1 waveform"),
                 controls.at("lfo 1 waveform"),
                 2, 19, 18);
    placeControl(gettext_noop("lfo 1 frequency"),
                 controls.at("lfo 1 frequency"),
                 22, 19, 18);
    placeControl(gettext_noop("lfo 2 waveform"),
                 controls.at("lfo 2 waveform"),
                 2, 22, 18);
    placeControl(gettext_noop("lfo 2 frequency"),
                 controls.at("lfo 2 frequency"),
                 22, 22, 18);

    // Volume / Delay.
    placeControl(gettext_noop("volume"),
                 controls.at("volume"),
                 2, 25, 38);
    placeControl(gettext_noop("delay time"),
                 controls.at("delay time"),
                 2, 28, 38);
    placeControl(gettext_noop("delay feedback"),
                 controls.at("delay feedback"),
                 2, 31, 18);
    placeControl(gettext_noop("delay dry/wet"),
                 controls.at("delay dry/wet"),
                 22, 31, 18);

    // Filter.
    placeControl(gettext_noop("filter type"),
                 controls.at("filter type"),
                 42, 7, 38);
    placeControl(gettext_noop("cutoff"),
                 controls.at("cutoff"),
                 42, 10, 38);
    placeControl(gettext_noop("resonance"),
                 controls.at("resonance"),
                 42, 13, 38);
    placeControl(gettext_noop("keytrack"),
                 controls.at("keytrack"),
                 42, 16, 38);
    placeControl(gettext_noop("fil env depth"),
                 controls.at("fil env depth"),
                 42, 19, 38);
    placeControl(gettext_noop("fil attack"),
                 controls.at("fil attack"),
                 42, 22, 38);
    placeControl(gettext_noop("fil decay"),
                 controls.at("fil decay"),
                 42, 25, 38);
    placeControl(gettext_noop("fil sustain"),
                 controls.at("fil sustain"),
                 42, 28, 38);
    placeControl(gettext_noop("fil release"),
                 controls.at("fil release"),
                 42, 31, 38);

    // Performance.
    placeControl(gettext_noop("polyphony"),
                 controls.at("polyphony"),
                 82, 7, 30);
    placeControl(gettext_noop("legato"),
                 controls.at("legato"),
                 114, 7, 6);
    placeControl(gettext_noop("portamento"),
                 controls.at("portamento"),
                 82, 10, 21);
    placeControl(gettext_noop("portamento type"),
                 controls.at("portamento type"),
                 105, 10, 15);
    placeControl(gettext_noop("pitch bend range"),
                 controls.at("pitch bend range"),
                 82, 13, 38);

    // Amplitude Envelope.
    placeControl(gettext_noop("amp attack"),
                 controls.at("amp attack"),
                 82, 19, 38);
    placeControl(gettext_noop("amp decay"),
                 controls.at("amp decay"),
                 82, 22, 38);
    placeControl(gettext_noop("amp sustain"),
                 controls.at("amp sustain"),
                 82, 25, 38);
    placeControl(gettext_noop("amp release"),
                 controls.at("amp release"),
                 82, 28, 38);
    placeControl(gettext_noop("velocity track"),
                 controls.at("velocity track"),
                 82, 31, 38);

    // Modulation Matrix.
    placeMinimalControl(gettext_noop("mod source 1"),
                        controls.at("mod source 1"),
                        26, 36, 22);
    placeMinimalControl(gettext_noop("mod scale 1"),
                        controls.at("mod scale 1"),
                        50, 36, 22);
    placeMinimalControl(gettext_noop("mod destination 1"),
                        controls.at("mod destination 1"),
                        74, 36, 22);
    placeMinimalControl(gettext_noop("mod source 2"),
                        controls.at("mod source 2"),
                        26, 37, 22);
    placeMinimalControl(gettext_noop("mod scale 2"),
                        controls.at("mod scale 2"),
                        50, 37, 22);
    placeMinimalControl(gettext_noop("mod destination 2"),
                        controls.at("mod destination 2"),
                        74, 37, 22);
    placeMinimalControl(gettext_noop("mod source 3"),
                        controls.at("mod source 3"),
                        26, 38, 22);
    placeMinimalControl(gettext_noop("mod scale 3"),
                        controls.at("mod scale 3"),
                        50, 38, 22);
    placeMinimalControl(gettext_noop("mod destination 3"),
                        controls.at("mod destination 3"),
                        74, 38, 22);
    placeMinimalControl(gettext_noop("mod source 4"),
                        controls.at("mod source 4"),
                        26, 39, 22);
    placeMinimalControl(gettext_noop("mod scale 4"),
                        controls.at("mod scale 4"),
                        50, 39, 22);
    placeMinimalControl(gettext_noop("mod destination 4"),
                        controls.at("mod destination 4"),
                        74, 39, 22);
    placeMinimalControl(gettext_noop("mod source 5"),
                        controls.at("mod source 5"),
                        26, 40, 22);
    placeMinimalControl(gettext_noop("mod scale 5"),
                        controls.at("mod scale 5"),
                        50, 40, 22);
    placeMinimalControl(gettext_noop("mod destination 5"),
                        controls.at("mod destination 5"),
                        74, 40, 22);
  }

  std::string CursynthGui::getCurrentControl() {
    return control_order_[control_index_];
  }

  std::string CursynthGui::getNextControl() {
    control_index_ = (control_index_ + 1) % control_order_.size();
    return getCurrentControl();
  }

  std::string CursynthGui::getPrevControl() {
    control_index_ = (control_index_ + control_order_.size() - 1) %
      control_order_.size();
    return getCurrentControl();
  }
} // namespace mopo
