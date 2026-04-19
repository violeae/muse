#pragma once

#include <atomic>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "MidiFile.h"
#include "Options.h"
#include "RtMidi.h"
#include <fluidsynth.h>

struct MuseConfig {
    std::string soundfontPath = "FluidR3_GM.sf2";
    std::string dataFile = "data.txt";
    std::string preloadFile = "preload.muse";
    std::string midiOutput = "outputMidi.midi";
    bool streamMode = false;
    unsigned int streamPort = 0;
};

class MuseApp {
public:
    explicit MuseApp(MuseConfig config);
    ~MuseApp();

    bool initialize();
    int run();
    void stop();

private:
    struct ScopeNode {
        std::unordered_map<std::string, int> numVars;
        std::unordered_map<std::string, std::string> varVars;
        int channel = -1;
        ScopeNode* parent = nullptr;
        std::vector<std::unique_ptr<ScopeNode>> children;
    };

    int getThreadChannel();
    int getNum(ScopeNode* scope, const std::string& name);
    std::string getVar(ScopeNode* scope, const std::string& name);
    bool hasNum(ScopeNode* scope, const std::string& name) const;
    bool hasVar(ScopeNode* scope, const std::string& name) const;
    bool isComplete(const std::string& line) const;

    void processLine(const std::string& line, ScopeNode* scope);
    void preloadSession();
    void fileWatcherLoop();
    void interactiveLoop();

    void initMidiRecording(int tpq = 480);
    void recordProgramChange(int channel, int tick, int program);
    int msToTicks(int durationMs, int bpm) const;
    void recordNoteEvent(int channel, int startTick, int durationTicks, int midiNote, int velocity);
    bool saveMidiFile(const std::string& filename);

    bool initFluidSynth(const std::string& soundfontPath);
    void cleanupFluidSynth();
    bool initMidiOut(unsigned int portIndex);
    void cleanupMidiOut();
    int freqToMidi(double freq) const;
    void playTone(double freq, int durationMs, ScopeNode* scope);
    void playNoteFromName(const std::string& noteName, ScopeNode* scope);

    static std::string trim(const std::string& input);
    static bool tryParseInt(const std::string& input, int* output);

    MuseConfig config_;
    std::unique_ptr<ScopeNode> root_;

    std::atomic<bool> running_{false};
    std::atomic<int> nextChannel_{0};

    std::unordered_set<std::string> keys_;
    std::unordered_map<std::string, double> octave4Hz_;

    std::ifstream dataInput_;
    std::ofstream dataOutput_;
    std::streampos watcherPosition_{0};
    std::string watcherBuffer_;

    std::mutex midiMutex_;
    smf::MidiFile midi_;
    int tpq_ = 480;
    int midiTrack_ = 0;
    std::vector<int> currentTickPerChannel_;

    std::mutex scopeMutex_;

    fluid_settings_t* fluidSettings_ = nullptr;
    fluid_synth_t* fluidSynth_ = nullptr;
    fluid_audio_driver_t* fluidDriver_ = nullptr;
    std::unique_ptr<rt::midi::RtMidiOut> midiOut_;
    std::mutex playbackMutex_;
};
