/* Writes the plugin's Turtle files from the engine's own controls, so a
 * control added to the synth cannot go missing from the plugin's description.
 * Hand written port lists drift; generated ones cannot. */

#include "cursynth_engine.h"
#include "cursynth_lv2.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

/* A name a Turtle file will accept as a symbol: letters, digits, underscore. */
std::string symbolFor(const std::string& name) {
  std::string out;
  for (size_t i = 0; i < name.size(); ++i) {
    const char c = name[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9'))
      out += c;
    else if (!out.empty() && out[out.size() - 1] != '_')
      out += '_';
  }
  while (!out.empty() && out[out.size() - 1] == '_')
    out.erase(out.size() - 1);
  if (out.empty() || (out[0] >= '0' && out[0] <= '9'))
    out = "c" + out;
  return out;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string dir = argc > 1 ? argv[1] : ".";

  mopo::CursynthEngine engine;
  mopo::control_map controls = engine.getControls();

  FILE* manifest = fopen((dir + "/manifest.ttl").c_str(), "w");
  if (!manifest) {
    fprintf(stderr, "cannot write %s/manifest.ttl\n", dir.c_str());
    return 1;
  }
  fprintf(manifest,
          "@prefix lv2:  <http://lv2plug.in/ns/lv2core#> .\n"
          "@prefix rdfs: <http://www.w3.org/2000/01/rdf-schema#> .\n"
          "\n"
          "<" CURSYNTH_URI ">\n"
          "    a lv2:Plugin ;\n"
          "    lv2:binary <cursynth@MODULE_EXT@> ;\n"
          "    rdfs:seeAlso <cursynth.ttl> .\n");
  fclose(manifest);

  FILE* f = fopen((dir + "/cursynth.ttl").c_str(), "w");
  if (!f) {
    fprintf(stderr, "cannot write %s/cursynth.ttl\n", dir.c_str());
    return 1;
  }

  fprintf(f,
          "@prefix atom:  <http://lv2plug.in/ns/ext/atom#> .\n"
          "@prefix doap:  <http://usefulinc.com/ns/doap#> .\n"
          "@prefix foaf:  <http://xmlns.com/foaf/0.1/> .\n"
          "@prefix lv2:   <http://lv2plug.in/ns/lv2core#> .\n"
          "@prefix midi:  <http://lv2plug.in/ns/ext/midi#> .\n"
          "@prefix rdf:   <http://www.w3.org/1999/02/22-rdf-syntax-ns#> .\n"
          "@prefix rdfs:  <http://www.w3.org/2000/01/rdf-schema#> .\n"
          "@prefix urid:  <http://lv2plug.in/ns/ext/urid#> .\n"
          "\n"
          /* The person belongs to the project, not to the plugin: a
           * maintainer on an lv2:Plugin is what the validator objects to. */
          "<" CURSYNTH_URI "#project>\n"
          "    a doap:Project ;\n"
          "    doap:name \"cursynth\" ;\n"
          "    doap:license <http://opensource.org/licenses/gpl-3.0> ;\n"
          "    doap:maintainer [\n"
          "        a foaf:Person ;\n"
          "        foaf:name \"Matt Tytel\" ;\n"
          "    ] .\n"
          "\n"
          "<" CURSYNTH_URI ">\n"
          "    a lv2:Plugin, lv2:InstrumentPlugin ;\n"
          "    lv2:project <" CURSYNTH_URI "#project> ;\n"
          "    doap:name \"cursynth\" ;\n"
          "    lv2:requiredFeature urid:map ;\n"
          "    lv2:optionalFeature lv2:hardRTCapable ;\n");

  fprintf(f,
          "    lv2:port [\n"
          "        a lv2:InputPort, atom:AtomPort ;\n"
          "        atom:bufferType atom:Sequence ;\n"
          "        atom:supports midi:MidiEvent ;\n"
          "        lv2:index %d ;\n"
          "        lv2:symbol \"midi_in\" ;\n"
          "        lv2:name \"MIDI In\" ;\n"
          "    ] , [\n"
          "        a lv2:OutputPort, lv2:AudioPort ;\n"
          "        lv2:index %d ;\n"
          "        lv2:symbol \"out_left\" ;\n"
          "        lv2:name \"Out Left\" ;\n"
          "    ] , [\n"
          "        a lv2:OutputPort, lv2:AudioPort ;\n"
          "        lv2:index %d ;\n"
          "        lv2:symbol \"out_right\" ;\n"
          "        lv2:name \"Out Right\" ;\n"
          "    ]",
          CURSYNTH_PORT_MIDI_IN, CURSYNTH_PORT_OUT_LEFT,
          CURSYNTH_PORT_OUT_RIGHT);

  int index = CURSYNTH_PORT_FIRST_CONTROL;
  int written = 0;
  for (mopo::control_map::iterator it = controls.begin();
       it != controls.end(); ++it, ++index) {
    const mopo::Control* c = it->second;
    const std::vector<std::string> strings = c->display_strings();

    fprintf(f,
            " , [\n"
            "        a lv2:InputPort, lv2:ControlPort ;\n"
            "        lv2:index %d ;\n"
            "        lv2:symbol \"%s\" ;\n"
            "        lv2:name \"%s\" ;\n"
            "        lv2:default %g ;\n"
            "        lv2:minimum %g ;\n"
            "        lv2:maximum %g ;\n",
            index, symbolFor(it->first).c_str(), it->first.c_str(),
            static_cast<double>(c->current_value()),
            static_cast<double>(c->min()), static_cast<double>(c->max()));

    if (!strings.empty()) {
      /* A control that names its settings is a list, not a dial, and a host
       * can say so instead of showing a number nobody can read. */
      fprintf(f, "        lv2:portProperty lv2:integer, lv2:enumeration ;\n");
      for (size_t i = 0; i < strings.size(); ++i)
        fprintf(f, "        lv2:scalePoint [ rdfs:label \"%s\" ; rdf:value %d ] ;\n",
                strings[i].c_str(), static_cast<int>(i));
    }
    fprintf(f, "    ]");
    written++;
  }
  fprintf(f, " .\n");
  fclose(f);

  printf("wrote %s/manifest.ttl and %s/cursynth.ttl with %d controls\n",
         dir.c_str(), dir.c_str(), written);
  return 0;
}
