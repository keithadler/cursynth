/* cursynth: native language support, or doing without it.
 *
 * Copyright 2013-2015 Matt Tytel
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

/* The most reported problem with cursynth, for eleven years, was that it
 * would not build because <libintl.h> could not be found. On macOS gettext is
 * installed away from the default include path, and the header was included
 * whether or not the build had asked for translation at all.
 *
 * A synthesizer that refuses to compile because it cannot find the machinery
 * for translating its own help screen has its priorities the wrong way round.
 * Without gettext it now simply speaks English. */

#ifndef CURSYNTH_NLS_H
#define CURSYNTH_NLS_H

#include "config.h"

#if defined(ENABLE_NLS) && ENABLE_NLS
#include <libintl.h>
#else
#define gettext(msgid) (msgid)
#define bindtextdomain(domain, directory) ((void)(domain), (void)(directory))
#define textdomain(domain) ((void)(domain))
#endif

#include <locale.h>

#endif  /* CURSYNTH_NLS_H */
