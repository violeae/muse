#include <fluidsynth.h>
#include <bits/stdc++.h>
#include <chrono>
#include <Windows.h>
#include <thread>
using namespace std;


// --- Definition of constants
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif



// --- Initialize FluidSynth globally (once)
fluid_settings_t* g_settings = nullptr;
fluid_synth_t* g_synth = nullptr;

// Call this once when program starts
void initFluidSynth(const char* soundfontPath) {
    g_settings = new_fluid_settings();
    g_synth = new_fluid_synth(g_settings);
    fluid_settings_setstr(g_settings, "audio.driver", "dsound");
    new_fluid_audio_driver(g_settings, g_synth);
    int id = fluid_synth_sfload(g_synth, soundfontPath, 1);
    if (id == FLUID_FAILED) {
        printf("Failed to load SoundFont: %s\n", soundfontPath);
    }
}

// --- Convert frequency (Hz) to nearest MIDI note (to be compatible with the original beeping)
int freqToMidi(double freq) {
    return static_cast<int>(std::round(69 + 12 * log2(freq / 440.0)));
}

// --- Play a note by frequency (ms) with instrument timbre
void playTone(double freq, int durationMs, int velocity = 100) {
    if (!g_synth) return;

    int midiNote = freqToMidi(freq);

    // Start note
    fluid_synth_noteon(g_synth, 0, midiNote, velocity);

    // Wait for specified duration
    Sleep(durationMs);

    // Stop note
    fluid_synth_noteoff(g_synth, 0, midiNote);
}

// --- Cleanup FluidSynth
void cleanupFluidSynth() {
    if (g_synth) delete_fluid_synth(g_synth);
    if (g_settings) delete_fluid_settings(g_settings);
}


// ---------------------------------------------
// Global state
// ---------------------------------------------
ifstream fin;
ofstream fout;

map<string,int> nums={
    {"O", 4}, {"T", 0}, {"B", 140},
    {"N", 4}
};

set<char> keys = {'c','d','e','f','g','a','b'};

map<char,double> oct4 = {
    {'c',261.63}, {'d',293.66}, {'e',329.63},
    {'f',349.23}, {'g',392.00}, {'a',440.00}, {'b',493.88},
};

map<string,string> var_mapping;

// ---------------------------------------------
// Helpers
// ---------------------------------------------
void ofreq(const char &m){
    double note_value = 60000.0 / nums["B"] * 4.0 / nums["N"];
    double freq = oct4[m] * pow(2, nums["O"] - 4) * pow(2, nums["T"] / 12.0);
    //cout << "Playing " << m << nums["O"] << " (" << freq << " Hz)\n";
    playTone(freq, note_value);
}

// ---------------------------------------------
// Process a command line
// ---------------------------------------------
void process_line(const string &line) {
    istringstream iss(line);
    string opr;

    while (iss >> opr) {
        // Keys
        if (keys.count(opr[0])) {
            ofreq(opr[0]);
        }
        else if (opr == "r"){
            Sleep(static_cast<DWORD>(60000.0 / nums["B"] * 4.0 / nums["N"]));
        }
        // Declaration
        else if (opr == "VAR") {
            string var_name;
            string var_content;
            iss >> var_name;
            char c;
            iss >> c; // skip '('
            int brc=1;
            while (iss >> noskipws >> c) {
                if(c=='('){brc++;}
                else if(c==')'){brc--;}
                if(brc==0){break;}
                var_content += c;
            }
            iss >> std::skipws;
            var_mapping[var_name] = var_content;
            cout << "Defined VAR " << var_name << " = " << var_content << endl;
        }
        else if (opr == "NUM"){
            string num_name;
            int num_content;
            iss>>num_name>>num_content;
            nums[num_name]=num_content;
            cout<< "Define NUM "<<num_name<<" = "<< num_content<<endl;
        }
        else if (opr[0] == '/') {
            string var_to_exec = opr.substr(1); // fixed: take name after '/'
            auto it = var_mapping.find(var_to_exec);
            if (it == var_mapping.end()) {
                cerr << "Variable " << var_to_exec << " not found.\n";
            } else {
                // recursively execute the content
                process_line(it->second);
            }
        }

        
        else if (opr == ">") nums["O"]++;
        else if (opr == "<") nums["O"]--;


        // Setting nums
        else if (nums.find(opr)!=nums.end()){
            string num_temp;
            if(!(iss>>num_temp)){
                cerr<<"Error: "<<opr<<" value missing\n";
                continue;
            }

            if(num_temp== "++"){
                nums[opr]++;
            }
            else if (num_temp == "--"){
                nums[opr]--;
            }
            else if (num_temp == ">>"){
                nums[opr]/=2;
            }
            else if (num_temp == "<<"){
                nums[opr]*=2;
            }
            else {
                try {
                    nums[opr] = stoi(num_temp);
                } catch(...) {
                    cerr << "Invalid "<<opr<<" value: "<<num_temp<<endl;
                }
            }
            cout<<"Num "<<opr<<" set to "<<nums[opr]<<endl;
        }



        else if (opr=="if"){
            string if_cond; //a num
            string if_sentence;
            iss>>if_cond; //true if nums[if_cond]!=0
            char c;
            iss>>c; //skip '('
            int brc=1;
            while (iss>>noskipws>>c){
                if(c=='('){brc++;}
                else if(c==')'){brc--;}
                if(brc==0){break;}
                if_sentence+=c;
            }
            iss >> std::skipws;
            //Checking if conditions
            auto it = nums.find(if_cond);
            if(it == nums.end()){
                cerr<<"Num "<<if_cond<<" not found.\n";
            }
            else{
                if(nums[if_cond]!=0){
                    process_line(if_sentence);
                }
            }
        }
        // Printing and functions
        else if (opr == "info"){
            cout<<"Current Settings: "<<"BPM: "<<nums["B"]<<" Octave: "<<nums["O"]<<endl
                <<"Note Value: "<<nums["N"]<<" Transpose: "<<nums["T"]<<endl;
            for(auto vm_pair: var_mapping){
                cout<<"VAR "<<vm_pair.first<<" = "<<vm_pair.second<<endl;
            }
        }
        else if (opr == "quit") {
            cout << "Exiting program..." << endl;
            exit(0);
        }
    }
}

// ---------------------------------------------
// Thread 1: watch data.txt
// ---------------------------------------------
void file_watcher() {
    streampos lastPos = 0;
    while (true) {
        fin.clear();
        fin.seekg(lastPos);

        string line;
        while (getline(fin, line)) {
            if (!line.empty()) process_line(line);
        }

        lastPos = fin.tellg();
        fin.clear();
        this_thread::sleep_for(chrono::milliseconds(200));
    }
}

// ---------------------------------------------
// Thread 2: interactive whole-line input
// ---------------------------------------------
void interactive_input() {
    cout << "Interactive mode: type commands or notes.\n"
        /* << "Examples:\n"
         << "   c d e f g\n"
         << "   VAR tune (c d e)\n"
         << "   /tune\n"
         << "   quit\n" */
         << "-------------------------------------------\n";

    string line;
    while (true) {
        cout << "> ";
        if (!getline(cin, line)) break;
        if (!line.empty()) process_line(line);
    }
}

// ---------------------------------------------
// Main
// ---------------------------------------------
int main() {
    initFluidSynth("FluidR3_GM.sf2");
    fluid_synth_program_change(g_synth, 0, 40);
    fin.open("data.txt");
    fout.open("data.txt", ios::app);
    if (!fin.is_open() || !fout.is_open()) {
        cerr << "Error opening data.txt\n";
        return 1;
    }

    cout << "Muse is running. Edit data.txt or type directly below.\n";

    thread t1(file_watcher);
    thread t2(interactive_input);

    t1.join();
    t2.join();
    return 0;
}
