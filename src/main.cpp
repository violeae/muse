#include "MuseApp.hpp"

#include <iostream>
#include <string>

namespace {
void printUsage(const char* exeName) {
    std::cout << "Usage: " << exeName << " [options]\n"
              << "Options:\n"
              << "  --soundfont <path>   SoundFont .sf2 file (default: FluidR3_GM.sf2)\n"
              << "  --data <path>        Command text file to watch (default: data.txt)\n"
              << "  --preload <path>     Preload script file (default: preload.muse)\n"
              << "  --midi <path>        MIDI output path for `save` command (default: outputMidi.midi)\n"
              << "  --stream             Stream note events to MIDI out port instead of FluidSynth audio\n"
              << "  --stream-port <n>    MIDI out port index for --stream (default: 0)\n"
              << "  --help               Show this help\n";
}
} // namespace

int main(int argc, char** argv) {
    MuseConfig config;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto requireValue = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                std::cerr << "Missing value after " << name << "\n";
                return nullptr;
            }
            return argv[++i];
        };

        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--soundfont") {
            if (const char* value = requireValue("--soundfont")) {
                config.soundfontPath = value;
            } else {
                return 1;
            }
        } else if (arg == "--data") {
            if (const char* value = requireValue("--data")) {
                config.dataFile = value;
            } else {
                return 1;
            }
        } else if (arg == "--preload") {
            if (const char* value = requireValue("--preload")) {
                config.preloadFile = value;
            } else {
                return 1;
            }
        } else if (arg == "--midi") {
            if (const char* value = requireValue("--midi")) {
                config.midiOutput = value;
            } else {
                return 1;
            }
        } else if (arg == "--stream") {
            config.streamMode = true;
        } else if (arg == "--stream-port") {
            if (const char* value = requireValue("--stream-port")) {
                try {
                    config.streamPort = static_cast<unsigned int>(std::stoul(value));
                } catch (...) {
                    std::cerr << "Invalid stream port index: " << value << "\n";
                    return 1;
                }
            } else {
                return 1;
            }
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    MuseApp app(config);
    if (!app.initialize()) {
        return 1;
    }

    return app.run();
}
