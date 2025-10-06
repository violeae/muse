/* COMPILE BY DOING THIS:
   g++ Muse.cpp -ldsound -ldxguid -lwinmm -lole32 -luuid -static -o playtone.exe
*/
#include <bits/stdc++.h>
#include <Windows.h>
#include <dsound.h>
#include <thread>
using namespace std;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#pragma comment(lib, "dsound.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "winmm.lib")

// ---------------------------------------------
// DirectSound tone generator
// ---------------------------------------------
void playTone(double frequency, double durationMs) {
    double durationSeconds = durationMs / 1000.0;

    LPDIRECTSOUND8 directSound;
    if (FAILED(DirectSoundCreate8(NULL, &directSound, NULL))) return;
    directSound->SetCooperativeLevel(GetDesktopWindow(), DSSCL_PRIORITY);

    DSBUFFERDESC primaryDesc = {};
    primaryDesc.dwSize = sizeof(DSBUFFERDESC);
    primaryDesc.dwFlags = DSBCAPS_PRIMARYBUFFER;

    LPDIRECTSOUNDBUFFER primaryBuffer;
    if (FAILED(directSound->CreateSoundBuffer(&primaryDesc, &primaryBuffer, NULL))) {
        directSound->Release();
        return;
    }

    WAVEFORMATEX waveFormat = {};
    waveFormat.wFormatTag = WAVE_FORMAT_PCM;
    waveFormat.nChannels = 1;
    waveFormat.nSamplesPerSec = 44100;
    waveFormat.wBitsPerSample = 16;
    waveFormat.nBlockAlign = waveFormat.nChannels * waveFormat.wBitsPerSample / 8;
    waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
    primaryBuffer->SetFormat(&waveFormat);

    int sampleRate = waveFormat.nSamplesPerSec;
    int numSamples = static_cast<int>(sampleRate * durationSeconds);
    int bufferSize = numSamples * waveFormat.nBlockAlign;

    DSBUFFERDESC secondaryDesc = {};
    secondaryDesc.dwSize = sizeof(DSBUFFERDESC);
    secondaryDesc.dwFlags = DSBCAPS_GLOBALFOCUS;
    secondaryDesc.dwBufferBytes = bufferSize;
    secondaryDesc.lpwfxFormat = &waveFormat;

    LPDIRECTSOUNDBUFFER secondaryBuffer;
    if (FAILED(directSound->CreateSoundBuffer(&secondaryDesc, &secondaryBuffer, NULL))) {
        primaryBuffer->Release();
        directSound->Release();
        return;
    }

    vector<short> samples(numSamples);
    const double amplitude = 30000.0;
    int fadeSamples = sampleRate * 0.01;

    for (int i = 0; i < numSamples; ++i) {
        double t = (double)i / sampleRate;
        double value = sin(2.0 * M_PI * frequency * t);
        double gain = 1.0;
        if (i < fadeSamples) gain = (double)i / fadeSamples;
        else if (i > numSamples - fadeSamples) gain = (double)(numSamples - i) / fadeSamples;
        samples[i] = (short)(value * amplitude * gain);
    }

    void* ptr1; DWORD bytes1; void* ptr2; DWORD bytes2;
    if (SUCCEEDED(secondaryBuffer->Lock(0, bufferSize, &ptr1, &bytes1, &ptr2, &bytes2, 0))) {
        memcpy(ptr1, samples.data(), bytes1);
        if (ptr2 && bytes2 > 0)
            memcpy(ptr2, (char*)samples.data() + bytes1, bytes2);
        secondaryBuffer->Unlock(ptr1, bytes1, ptr2, bytes2);
    }

    secondaryBuffer->SetCurrentPosition(0);
    secondaryBuffer->Play(0, 0, 0);
    Sleep((DWORD)(durationSeconds * 1000));

    secondaryBuffer->Release();
    primaryBuffer->Release();
    directSound->Release();
}

// ---------------------------------------------
// Global state
// ---------------------------------------------
ifstream fin;
ofstream fout;

map<string,int> nums={
    {"octave", 4}, {"transpose", 0}, {"bpm", 140},
    {"note", 4}
};



set<char> keys = {'c','d','e','f','g','a','b'};

map<char,double> oct4 = {
    {'c',261.63}, {'d',293.66}, {'e',329.63},
    {'f',349.23}, {'g',392.00}, {'a',440.00}, {'b',493.88}
};

map<string,string> var_mapping;

// ---------------------------------------------
// Helpers
// ---------------------------------------------
void ofreq(const char &m){
    double note_value = 60000.0 / nums["bpm"] * 4.0 / nums["note"];
    double freq = oct4[m] * pow(2, nums["octave"] - 4) * pow(2, nums["transpose"] / 12.0);
    //cout << "Playing " << m << nums["octave"] << " (" << freq << " Hz)\n";
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

        // Declaration
        else if (opr == "VAR") {
            string var_name;
            string var_content;
            iss >> var_name;
            char c;
            iss >> c; // skip '('
            while (iss >> noskipws >> c && c != ')') {
                var_content += c;
            }
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

        // Setting nums
        else if (opr == ">") nums["octave"]++;
        else if (opr == "<") nums["octave"]--;
        else if (opr == "T") {
            string trans_temp;
            if (!(iss >> trans_temp)) {
                cerr << "Error: transpose value missing\n";
                continue;
            }

            if (trans_temp == "++") {
                nums["transpose"]++;
            } else if (trans_temp == "--") {
                nums["transpose"]--;
            } else {
                try {
                    nums["transpose"] = stoi(trans_temp);
                } catch (...) {
                    cerr << "Invalid transpose value: " << trans_temp << endl;
                    // keep previous transpose
                }
        }

        cout << "Transpose set to " << nums["transpose"] << endl;
    }
        else if (opr == "N") iss >> nums["note"];
        else if (opr == "O") {
            char c; iss >> c;
            if (isdigit(c)) nums["octave"] = c - '0';
        }
        else if (opr=="B"){
            iss>>nums["bpm"];
        }
        else if (opr=="if"){
            string if_cond; //a num
            string if_sentence;
            iss>>if_cond; //true if nums[if_cond]!=0
            char c;
            iss>>c; //skip '('
            while (iss>>noskipws>>c&&c!=')'){
                if_sentence+=c;
            }
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
            cout<<"Current Settings: "<<"BPM: "<<nums["bpm"]<<" Octave: "<<nums["octave"]<<endl
                <<"Note Value: "<<nums["note"]<<" Transpose: "<<nums["transpose"]<<endl;
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
