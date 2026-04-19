#include "MuseApp.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace {
std::string stripLineComment(const std::string& line) {
    for (std::size_t i = 0; i < line.size(); ++i) {
        if (line[i] != '#') {
            continue;
        }

        const bool commentStart = (i == 0) || std::isspace(static_cast<unsigned char>(line[i - 1]));
        if (commentStart) {
            return line.substr(0, i);
        }
    }
    return line;
}

std::string stripComments(const std::string& input) {
    std::istringstream in(input);
    std::ostringstream out;
    std::string line;
    bool first = true;
    while (std::getline(in, line)) {
        if (!first) {
            out << '\n';
        }
        first = false;
        out << stripLineComment(line);
    }
    return out.str();
}
} // namespace

MuseApp::MuseApp(MuseConfig config)
    : config_(std::move(config)), root_(std::make_unique<ScopeNode>()), currentTickPerChannel_(16, 0) {
    keys_ = {
        "c", "d", "e", "f", "g", "a", "b", "c#", "db", "d#", "eb", "f#", "gb", "g#", "ab", "a#", "bb",
    };

    octave4Hz_ = {
        {"c", 261.63}, {"d", 293.66}, {"e", 329.63}, {"f", 349.23}, {"g", 392.00}, {"a", 440.00}, {"b", 493.88},
        {"c#", 277.18}, {"db", 277.18}, {"d#", 311.13}, {"eb", 311.13}, {"f#", 369.99}, {"gb", 369.99},
        {"g#", 415.30}, {"ab", 415.30}, {"a#", 466.16}, {"bb", 466.16},
    };
}

MuseApp::~MuseApp() {
    stop();
    cleanupMidiOut();
    cleanupFluidSynth();
}

bool MuseApp::initialize() {
    root_->channel = getThreadChannel();
    root_->numVars = {
        {"O", 4},   // Octave
        {"T", 0},   // Transpose
        {"B", 140}, // BPM
        {"N", 4},   // Note value
        {"V", 100}, // Velocity
        {"I", 0},   // Instrument
    };

    if (config_.streamMode) {
        if (!initMidiOut(config_.streamPort)) {
            return false;
        }
    } else {
        if (!initFluidSynth(config_.soundfontPath)) {
            return false;
        }
    }

    initMidiRecording();
    midi_.addTempo(0, 0, getNum(root_.get(), "B"));

    preloadSession();

    dataInput_.open(config_.dataFile);
    dataOutput_.open(config_.dataFile, std::ios::app);
    if (!dataInput_.is_open() || !dataOutput_.is_open()) {
        std::cerr << "Error opening data file: " << config_.dataFile << "\n";
        return false;
    }

    std::cout << "Muse is running. Edit " << config_.dataFile << " or type directly below.\n";
    return true;
}

int MuseApp::run() {
    running_.store(true);
    std::thread watcher(&MuseApp::fileWatcherLoop, this);
    std::thread interactive(&MuseApp::interactiveLoop, this);

    interactive.join();
    running_.store(false);
    watcher.join();

    return 0;
}

void MuseApp::stop() {
    running_.store(false);
}

int MuseApp::getThreadChannel() {
    int channel = nextChannel_.fetch_add(1) % 16;
    if (channel == 9) {
        channel = nextChannel_.fetch_add(1) % 16;
    }
    return channel;
}

int MuseApp::getNum(ScopeNode* scope, const std::string& name) {
    for (ScopeNode* cur = scope; cur != nullptr; cur = cur->parent) {
        auto it = cur->numVars.find(name);
        if (it != cur->numVars.end()) {
            if (cur != scope) {
                scope->numVars[name] = it->second;
            }
            return it->second;
        }
    }
    throw std::runtime_error("NUM not found: " + name);
}

std::string MuseApp::getVar(ScopeNode* scope, const std::string& name) {
    for (ScopeNode* cur = scope; cur != nullptr; cur = cur->parent) {
        auto it = cur->varVars.find(name);
        if (it != cur->varVars.end()) {
            return it->second;
        }
    }
    throw std::runtime_error("VAR not found: " + name);
}

bool MuseApp::hasNum(ScopeNode* scope, const std::string& name) const {
    for (ScopeNode* cur = scope; cur != nullptr; cur = cur->parent) {
        if (cur->numVars.find(name) != cur->numVars.end()) {
            return true;
        }
    }
    return false;
}

bool MuseApp::hasVar(ScopeNode* scope, const std::string& name) const {
    for (ScopeNode* cur = scope; cur != nullptr; cur = cur->parent) {
        if (cur->varVars.find(name) != cur->varVars.end()) {
            return true;
        }
    }
    return false;
}

bool MuseApp::isComplete(const std::string& line) const {
    int braces = 0;
    int parens = 0;
    for (char c : line) {
        if (c == '{') {
            ++braces;
        } else if (c == '}') {
            --braces;
        } else if (c == '(') {
            ++parens;
        } else if (c == ')') {
            --parens;
        }
    }
    return braces == 0 && parens == 0;
}

void MuseApp::initMidiRecording(int tpq) {
    std::lock_guard<std::mutex> lock(midiMutex_);
    tpq_ = tpq;
    midi_.clear();
    midi_.absoluteTicks();
    midi_.addTrack(0);
    midi_.setTicksPerQuarterNote(tpq_);
    std::fill(currentTickPerChannel_.begin(), currentTickPerChannel_.end(), 0);
}

void MuseApp::recordProgramChange(int channel, int tick, int program) {
    std::vector<unsigned char> msg;
    msg.push_back(static_cast<unsigned char>(0xC0 | (channel & 0x0F)));
    msg.push_back(static_cast<unsigned char>(program & 0x7F));
    midi_.addEvent(midiTrack_, tick, msg);
}

int MuseApp::msToTicks(int durationMs, int bpm) const {
    const double quarterMs = 60000.0 / static_cast<double>(bpm);
    const double ticks = (static_cast<double>(durationMs) / quarterMs) * static_cast<double>(tpq_);
    return static_cast<int>(std::lround(ticks));
}

void MuseApp::recordNoteEvent(int channel, int startTick, int durationTicks, int midiNote, int velocity) {
    std::vector<unsigned char> on{
        static_cast<unsigned char>(0x90 | (channel & 0x0F)),
        static_cast<unsigned char>(midiNote & 0x7F),
        static_cast<unsigned char>(velocity & 0x7F),
    };

    std::vector<unsigned char> off{
        static_cast<unsigned char>(0x80 | (channel & 0x0F)),
        static_cast<unsigned char>(midiNote & 0x7F),
        static_cast<unsigned char>(0),
    };

    midi_.addEvent(midiTrack_, startTick, on);
    midi_.addEvent(midiTrack_, startTick + durationTicks, off);
}

bool MuseApp::saveMidiFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(midiMutex_);

    midi_.sortTracks();
    std::vector<unsigned char> eot{0xFF, 0x2F, 0x00};
    int lastTick = 0;
    if (midi_.getTrackCount() > 0 && midi_[0].getEventCount() > 0) {
        lastTick = midi_[0][midi_[0].getEventCount() - 1].tick + 1;
    }
    midi_.addEvent(0, lastTick, eot);

    if (!midi_.write(filename)) {
        std::cerr << "Failed to write MIDI file: " << filename << "\n";
        return false;
    }

    std::cout << "Saved MIDI file: " << filename << "\n";
    return true;
}

bool MuseApp::initFluidSynth(const std::string& soundfontPath) {
    fluidSettings_ = new_fluid_settings();
    if (fluidSettings_ == nullptr) {
        std::cerr << "Failed to create FluidSynth settings.\n";
        return false;
    }

    fluidSynth_ = new_fluid_synth(fluidSettings_);
    if (fluidSynth_ == nullptr) {
        std::cerr << "Failed to create FluidSynth synth.\n";
        return false;
    }

#ifdef _WIN32
    fluid_settings_setstr(fluidSettings_, "audio.driver", "dsound");
#endif
    fluidDriver_ = new_fluid_audio_driver(fluidSettings_, fluidSynth_);
    if (fluidDriver_ == nullptr) {
        std::cerr << "Failed to create FluidSynth audio driver.\n";
        return false;
    }

    const int id = fluid_synth_sfload(fluidSynth_, soundfontPath.c_str(), 1);
    if (id == FLUID_FAILED) {
        std::cerr << "Failed to load SoundFont: " << soundfontPath << "\n";
        return false;
    }

    return true;
}

void MuseApp::cleanupFluidSynth() {
    if (fluidDriver_ != nullptr) {
        delete_fluid_audio_driver(fluidDriver_);
        fluidDriver_ = nullptr;
    }
    if (fluidSynth_ != nullptr) {
        delete_fluid_synth(fluidSynth_);
        fluidSynth_ = nullptr;
    }
    if (fluidSettings_ != nullptr) {
        delete_fluid_settings(fluidSettings_);
        fluidSettings_ = nullptr;
    }
}

bool MuseApp::initMidiOut(unsigned int portIndex) {
    try {
        midiOut_ = std::make_unique<rt::midi::RtMidiOut>();
        const unsigned int portCount = midiOut_->getPortCount();
        if (portCount == 0) {
            std::cerr << "No MIDI output ports found. Create one and retry.\n";
            return false;
        }

        if (portIndex >= portCount) {
            std::cerr << "Invalid MIDI out port index " << portIndex << ". Available ports:\n";
            for (unsigned int i = 0; i < portCount; ++i) {
                std::cerr << "  [" << i << "] " << midiOut_->getPortName(i) << "\n";
            }
            return false;
        }

        midiOut_->openPort(portIndex, "Muse Stream Out");
        std::cout << "Streaming mode enabled. MIDI out: [" << portIndex << "] " << midiOut_->getPortName(portIndex)
                  << "\n";
        return true;
    } catch (const rt::midi::RtMidiError& error) {
        std::cerr << "Failed to initialize MIDI output: " << error.getMessage() << "\n";
        midiOut_.reset();
        return false;
    } catch (const std::exception& error) {
        std::cerr << "Failed to initialize MIDI output: " << error.what() << "\n";
        midiOut_.reset();
        return false;
    }
}

void MuseApp::cleanupMidiOut() {
    if (!midiOut_) {
        return;
    }

    try {
        if (midiOut_->isPortOpen()) {
            midiOut_->closePort();
        }
    } catch (...) {
    }

    midiOut_.reset();
}

int MuseApp::freqToMidi(double freq) const {
    return static_cast<int>(std::lround(69.0 + 12.0 * std::log2(freq / 440.0)));
}

void MuseApp::playTone(double freq, int durationMs, ScopeNode* scope) {
    const int midiNote = freqToMidi(freq);
    const int channel = scope->channel;
    const int velocity = getNum(scope, "V");
    const int instrument = getNum(scope, "I");
    const int bpm = getNum(scope, "B");

    {
        std::lock_guard<std::mutex> lock(midiMutex_);
        const int startTick = currentTickPerChannel_[channel];
        recordProgramChange(channel, startTick, instrument);
        const int durationTicks = msToTicks(durationMs, bpm);
        recordNoteEvent(channel, startTick, durationTicks, midiNote, velocity);
        currentTickPerChannel_[channel] = startTick + durationTicks;
    }

    if (config_.streamMode) {
        std::vector<unsigned char> programChange{
            static_cast<unsigned char>(0xC0 | (channel & 0x0F)),
            static_cast<unsigned char>(instrument & 0x7F),
        };
        std::vector<unsigned char> noteOn{
            static_cast<unsigned char>(0x90 | (channel & 0x0F)),
            static_cast<unsigned char>(midiNote & 0x7F),
            static_cast<unsigned char>(velocity & 0x7F),
        };
        std::vector<unsigned char> noteOff{
            static_cast<unsigned char>(0x80 | (channel & 0x0F)),
            static_cast<unsigned char>(midiNote & 0x7F),
            static_cast<unsigned char>(0),
        };

        {
            std::lock_guard<std::mutex> lock(playbackMutex_);
            if (midiOut_ && midiOut_->isPortOpen()) {
                midiOut_->sendMessage(&programChange);
                midiOut_->sendMessage(&noteOn);
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(durationMs));

        {
            std::lock_guard<std::mutex> lock(playbackMutex_);
            if (midiOut_ && midiOut_->isPortOpen()) {
                midiOut_->sendMessage(&noteOff);
            }
        }
    } else {
        fluid_synth_program_change(fluidSynth_, channel, instrument);
        fluid_synth_noteon(fluidSynth_, channel, midiNote, velocity);
        std::this_thread::sleep_for(std::chrono::milliseconds(durationMs));
        fluid_synth_noteoff(fluidSynth_, channel, midiNote);
    }
}

void MuseApp::playNoteFromName(const std::string& noteName, ScopeNode* scope) {
    const double noteValueMs = 60000.0 / static_cast<double>(getNum(scope, "B")) * 4.0 / static_cast<double>(getNum(scope, "N"));
    const double freq =
        octave4Hz_.at(noteName) * std::pow(2.0, getNum(scope, "O") - 4) * std::pow(2.0, getNum(scope, "T") / 12.0);
    playTone(freq, static_cast<int>(std::lround(noteValueMs)), scope);
}

std::string MuseApp::trim(const std::string& input) {
    const std::string whitespace = " \t\r\n";
    const std::size_t start = input.find_first_not_of(whitespace);
    if (start == std::string::npos) {
        return {};
    }
    const std::size_t end = input.find_last_not_of(whitespace);
    return input.substr(start, end - start + 1);
}

bool MuseApp::tryParseInt(const std::string& input, int* output) {
    if (output == nullptr) {
        return false;
    }

    char* end = nullptr;
    const long parsed = std::strtol(input.c_str(), &end, 10);
    if (end == input.c_str() || *end != '\0') {
        return false;
    }

    *output = static_cast<int>(parsed);
    return true;
}

void MuseApp::processLine(const std::string& rawLine, ScopeNode* scope) {
    std::string code = stripComments(rawLine);
    std::string stripped = trim(code);
    if (stripped.empty()) {
        return;
    }

    std::istringstream iss(code);
    std::string op;

    while (iss >> op) {
        if (keys_.find(op) != keys_.end()) {
            playNoteFromName(op, scope);
            continue;
        }

        if (op == "r") {
            const int restMs = static_cast<int>(std::lround(60000.0 / static_cast<double>(getNum(scope, "B")) *
                                                            4.0 / static_cast<double>(getNum(scope, "N"))));
            std::this_thread::sleep_for(std::chrono::milliseconds(restMs));
            continue;
        }

        if (op == "VAR") {
            std::string varName;
            if (!(iss >> varName)) {
                std::cerr << "Invalid VAR declaration.\n";
                continue;
            }

            char c = '\0';
            if (!(iss >> c) || c != '(') {
                std::cerr << "VAR " << varName << " must be followed by (...) .\n";
                continue;
            }

            std::string value;
            int depth = 1;
            while (iss >> std::noskipws >> c) {
                if (c == '(') {
                    ++depth;
                } else if (c == ')') {
                    --depth;
                }
                if (depth == 0) {
                    break;
                }
                value.push_back(c);
            }
            iss >> std::skipws;
            scope->varVars[varName] = value;
            std::cout << "Defined VAR " << varName << " = " << value << "\n";
            continue;
        }

        if (op == "NUM") {
            std::string name;
            int value = 0;
            if (!(iss >> name >> value)) {
                std::cerr << "Invalid NUM declaration.\n";
                continue;
            }
            scope->numVars[name] = value;
            std::cout << "Defined NUM " << name << " = " << value << "\n";
            continue;
        }

        if (!op.empty() && op[0] == '/') {
            const std::string varToExec = op.substr(1);
            if (!hasVar(scope, varToExec)) {
                std::cerr << "VAR " << varToExec << " not found.\n";
                continue;
            }
            processLine(getVar(scope, varToExec), scope);
            continue;
        }

        if (op == ">") {
            scope->numVars["O"] = getNum(scope, "O") + 1;
            continue;
        }
        if (op == "<") {
            scope->numVars["O"] = getNum(scope, "O") - 1;
            continue;
        }

        if (hasNum(scope, op)) {
            scope->numVars[op] = getNum(scope, op);
            std::string token;
            if (!(iss >> token)) {
                std::cerr << "Missing value for " << op << "\n";
                continue;
            }

            if (token == "++") {
                ++scope->numVars[op];
            } else if (token == "--") {
                --scope->numVars[op];
            } else if (token == ">>") {
                scope->numVars[op] /= 2;
            } else if (token == "<<") {
                scope->numVars[op] *= 2;
            } else {
                int parsed = 0;
                if (!tryParseInt(token, &parsed)) {
                    std::cerr << "Invalid numeric value: " << token << "\n";
                    continue;
                }
                scope->numVars[op] = parsed;
            }
            std::cout << "NUM " << op << " set to " << scope->numVars[op] << "\n";
            continue;
        }

        if (op == "if") {
            std::string condName;
            if (!(iss >> condName)) {
                std::cerr << "if condition variable missing.\n";
                continue;
            }

            char c = '\0';
            if (!(iss >> c) || c != '(') {
                std::cerr << "if " << condName << " must be followed by (...)\n";
                continue;
            }

            std::string sentence;
            int depth = 1;
            while (iss >> std::noskipws >> c) {
                if (c == '(') {
                    ++depth;
                } else if (c == ')') {
                    --depth;
                }
                if (depth == 0) {
                    break;
                }
                sentence.push_back(c);
            }
            iss >> std::skipws;

            if (!hasNum(scope, condName)) {
                std::cerr << "NUM " << condName << " not found.\n";
            } else if (getNum(scope, condName) != 0) {
                processLine(sentence, scope);
            }
            continue;
        }

        if (op == "{") {
            std::vector<std::string> threadContents;
            std::string content;
            char c = '\0';
            int braceDepth = 1;

            while (iss >> std::noskipws >> c) {
                if (c == '{') {
                    ++braceDepth;
                    content.push_back(c);
                } else if (c == '}') {
                    --braceDepth;
                    if (braceDepth == 0) {
                        break;
                    }
                    content.push_back(c);
                } else if (c == '(' && braceDepth == 1) {
                    if (!trim(content).empty()) {
                        threadContents.push_back(trim(content));
                        content.clear();
                    }
                    std::string parenContent;
                    int parenDepth = 1;
                    while (iss >> std::noskipws >> c) {
                        parenContent.push_back(c);
                        if (c == '(') {
                            ++parenDepth;
                        } else if (c == ')') {
                            --parenDepth;
                        }
                        if (parenDepth == 0) {
                            break;
                        }
                    }
                    threadContents.push_back(parenContent);
                } else if (std::isspace(static_cast<unsigned char>(c)) && braceDepth == 1) {
                    if (!trim(content).empty()) {
                        threadContents.push_back(trim(content));
                        content.clear();
                    }
                } else {
                    content.push_back(c);
                }
            }
            iss >> std::skipws;

            if (!trim(content).empty()) {
                threadContents.push_back(trim(content));
            }

            std::vector<std::thread> workers;
            workers.reserve(threadContents.size());

            for (const std::string& component : threadContents) {
                workers.emplace_back([this, component, scope]() {
                    auto child = std::make_unique<ScopeNode>();
                    child->parent = scope;
                    child->channel = getThreadChannel();
                    ScopeNode* childPtr = child.get();

                    {
                        std::lock_guard<std::mutex> lock(scopeMutex_);
                        scope->children.push_back(std::move(child));
                    }

                    processLine(component, childPtr);
                });
            }

            for (std::thread& worker : workers) {
                worker.join();
            }

            std::string rest;
            std::getline(iss, rest);
            if (!trim(rest).empty()) {
                processLine(rest, scope);
            }
            continue;
        }

        if (op == "info") {
            std::cout << "Current Settings:\n"
                      << "BPM: " << getNum(scope, "B") << " Octave: " << getNum(scope, "O") << "\n"
                      << "Note Value: " << getNum(scope, "N") << " Transpose: " << getNum(scope, "T") << "\n"
                      << "Instrument: " << getNum(scope, "I") << " Volume: " << getNum(scope, "V") << "\n";

            for (ScopeNode* cur = scope; cur != nullptr; cur = cur->parent) {
                for (const auto& entry : cur->varVars) {
                    std::cout << "VAR " << entry.first << " = " << entry.second << "\n";
                }
            }
            continue;
        }

        if (op == "save") {
            saveMidiFile(config_.midiOutput);
            continue;
        }

        if (op == "quit") {
            std::cout << "Exiting program...\n";
            running_.store(false);
            return;
        }

        if (op == "clear") {
            std::cout << "\033[2J\033[1;1H";
            continue;
        }
    }
}

void MuseApp::preloadSession() {
    std::ifstream preload(config_.preloadFile);
    if (!preload.is_open()) {
        return;
    }

    std::streambuf* oldOut = std::cout.rdbuf();
    std::streambuf* oldErr = std::cerr.rdbuf();
#ifdef _WIN32
    std::ofstream devnull("NUL");
#else
    std::ofstream devnull("/dev/null");
#endif
    std::cout.rdbuf(devnull.rdbuf());
    std::cerr.rdbuf(devnull.rdbuf());

    std::string line;
    std::string buffer;
    while (std::getline(preload, line)) {
        buffer += line;
        buffer.push_back('\n');
        if (isComplete(buffer)) {
            if (!trim(buffer).empty()) {
                processLine(buffer, root_.get());
            }
            buffer.clear();
        }
    }
    if (!trim(buffer).empty()) {
        processLine(buffer, root_.get());
    }

    std::cout.rdbuf(oldOut);
    std::cerr.rdbuf(oldErr);
    std::cout << "Preload complete.\n";
}

void MuseApp::fileWatcherLoop() {
    watcherPosition_ = 0;
    watcherBuffer_.clear();
    while (running_.load()) {
        dataInput_.clear();
        dataInput_.seekg(watcherPosition_);

        std::string line;
        while (running_.load() && std::getline(dataInput_, line)) {
            watcherBuffer_ += line;
            watcherBuffer_.push_back('\n');
            if (isComplete(watcherBuffer_)) {
                if (!trim(watcherBuffer_).empty()) {
                    processLine(watcherBuffer_, root_.get());
                }
                watcherBuffer_.clear();
            }
        }

        watcherPosition_ = dataInput_.tellg();
        if (watcherPosition_ == std::streampos(-1)) {
            dataInput_.clear();
            dataInput_.seekg(0, std::ios::end);
            watcherPosition_ = dataInput_.tellg();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

void MuseApp::interactiveLoop() {
    std::cout << "Interactive mode: type commands or notes.\n"
              << "-------------------------------------------\n"
              << "> ";

    std::string line;
    std::string buffer;
    while (running_.load() && std::getline(std::cin, line)) {
        buffer += line;
        buffer.push_back('\n');

        if (isComplete(buffer)) {
            processLine(buffer, root_.get());
            buffer.clear();
            if (running_.load()) {
                std::cout << "> ";
            }
        } else {
            std::cout << "...";
        }
    }

    running_.store(false);
}
