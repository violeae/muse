#include <fluidsynth.h>
#include <bits/stdc++.h>
#include <chrono>
#include <Windows.h>
#include <thread>
#include<atomic>
using namespace std;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// -- Thread-local channel allocation
atomic<int> nextChannel{0};
int getThreadChannel(){
    int ch = nextChannel++;
    ch%=16;
    if(ch==9){ ch = nextChannel++ % 16;}
    return ch;
}

// ---------------------------------------------
// Scope Structure
// ---------------------------------------------

struct ScopeNode{
    map<string, int> numVars;
    map<string, string> varVars;
    int channel =-1;
    ScopeNode* parent = nullptr;
    vector<ScopeNode*> children;
};



set<char> keys = {'c','d','e','f','g','a','b'};

map<char,double> oct4 = {
    {'c',261.63}, {'d',293.66}, {'e',329.63},
    {'f',349.23}, {'g',392.00}, {'a',440.00}, {'b',493.88},
};




int getNum(ScopeNode* scope, const string& name){
    for(auto* cur = scope; cur; cur = cur->parent){
        auto it = cur->numVars.find(name);
        if (it != cur->numVars.end()){
            int value = it->second;
            if(scope!=cur){
                scope->numVars[name] = value;
            }
            return value;
        }
    }
    throw runtime_error("NUM: "+name+" not found");
}

string getVar(ScopeNode* scope, const string& name){
    for(auto* cur =scope; cur; cur = cur->parent){
        if(cur->varVars.count(name)){
            return cur->varVars[name];
        }
    }
    throw runtime_error("VAR: "+name+" not found");
}

bool hasNum(ScopeNode* scope, const std::string& name) {
    for (auto* cur = scope; cur; cur = cur->parent) {
        if (cur->numVars.find(name) != cur->numVars.end())
            return true;
    }
    return false;
}

bool hasVar(ScopeNode* scope, const std::string& name) {
    for (auto* cur = scope; cur; cur = cur->parent) {
        if (cur->varVars.find(name) != cur->varVars.end())
            return true;
    }
    return false;
}



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
void playTone(double freq, int durationMs, ScopeNode* scope) {
    int midiNote = freqToMidi(freq);
    int channel = scope->channel;
    int velocity = getNum(scope,"V");
    int instrument = getNum(scope,"I");
    fluid_synth_program_change(g_synth, channel, instrument);
    // Start note
    fluid_synth_noteon(g_synth, channel, midiNote, velocity);
    // Wait for specified duration
    this_thread::sleep_for(chrono::milliseconds(durationMs));
    // Stop note
    fluid_synth_noteoff(g_synth, channel, midiNote);
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

ScopeNode* root = new ScopeNode();





// ---------------------------------------------
// Helpers
// ---------------------------------------------
void ofreq(const char &m, ScopeNode* scope){
    double note_value = 60000.0 / getNum(scope,"B") * 4.0 / (getNum(scope,"N"));
    double freq = oct4[m] * pow(2, getNum(scope,"O") - 4) * pow(2, getNum(scope,"T") / 12.0);
    //cout << "Playing " << m << nums["O"] << " (" << freq << " Hz)\n";
    playTone(freq, note_value, scope);
}

bool isComplete(const string& s){
    int braces = 0;
    int parens =0;
    for (char c : s){
        if(c=='{') braces++;
        else if (c=='}') braces --;
        else if (c=='(') parens++;
        else if (c==')') parens--;
    }
    return parens==0 && braces == 0;
}

// ---------------------------------------------
// Process a command line
// ---------------------------------------------
void process_line(const string &line, ScopeNode* scope) {
    istringstream iss(line);
    string opr;

    while (iss >> opr) {
        // Keys
        if (keys.count(opr[0])) {
            ofreq(opr[0], scope);
        }
        else if (opr == "r"){
            Sleep(static_cast<DWORD>(60000.0 / (getNum(scope,"B")) * 4.0 / (getNum(scope,"N")) ));
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
            scope->varVars[var_name] = var_content;
            cout << "Defined VAR " << var_name << " = " << var_content << endl;
        }
        else if (opr == "NUM"){
            string num_name;
            int num_content;
            iss>>num_name>>num_content;
            scope->numVars[num_name]=num_content;
            cout<< "Define NUM "<<num_name<<" = "<< num_content<<endl;
        }
        else if (opr[0] == '/') {
            string var_to_exec = opr.substr(1); // fixed: take name after '/'
            
            if (hasVar==0) {
                cerr << "VAR" << var_to_exec << " not found.\n";
            } else {
                auto it = getVar(scope,var_to_exec);
                // recursively execute the content
                process_line(it,scope);
            }
        }

        
        else if (opr == ">") {
            getNum(scope,"O");
            scope->numVars["O"]++;
        }
        else if (opr == "<") {
            getNum(scope,"O");
            scope->numVars["O"]--;
        }


        // Setting nums
        else if (
            (scope->numVars.find(opr)!=scope->numVars.end())
            ||
            (
                scope->numVars.find(opr)==scope->numVars.end()
                &&
                hasNum(scope,opr)
            )
        ){
            int val = getNum(scope, opr);
            scope->numVars[opr] = val;
            string num_temp;
            if(!(iss>>num_temp)){
                cerr<<"Error: "<<opr<<" value missing\n";
                continue;
            }

            if(num_temp== "++"){
                scope->numVars[opr]++;
            }
            else if (num_temp == "--"){
                scope->numVars[opr]--;
            }
            else if (num_temp == ">>"){
                scope->numVars[opr]/=2;
            }
            else if (num_temp == "<<"){
                scope->numVars[opr]*=2;
            }
            else {
                try {
                    scope->numVars[opr] = stoi(num_temp);
                } catch(...) {
                    cerr << "Invalid "<<opr<<" value: "<<num_temp<<endl;
                }
            }
            cout<<"Num "<<opr<<" set to "<<scope->numVars[opr]<<endl;
        }


        else if (opr=="if"){
            string if_cond; //a num
            string if_sentence;
            iss>>if_cond; //true if nums[if_cond]!=0
            char c;
            iss>>c; //skip '('
            int brc=1;
            while (iss>>noskipws>>c){ //checking depths
                if(c=='('){brc++;}
                else if(c==')'){brc--;}
                if(brc==0){break;}
                if_sentence+=c;
            }
            iss >> std::skipws;
            //Checking if conditions
            auto it = scope->numVars.find(if_cond);
            if(hasNum(scope,if_cond)==0){
                cerr<<"Num "<<if_cond<<" not found.\n";
            }
            else{
                if(getNum(scope,if_cond)!=0){
                    process_line(if_sentence,scope);
                }
            }
        }
        //Simultaneous Sounds
        else if (opr == "{") {
            vector<string> thread_contents;
            string content;
            char c;
            int brace_depth=1;
            while (iss >> noskipws >> c) {
                if(c =='{') {
                    content+=c;
                    brace_depth++;
                }
                else if (c == '}') {
                    brace_depth--;
                    if(brace_depth==0){break;}
                    else{content+=c;}
                }
                
                else if (c == '('&&brace_depth==1) {
                    // Only deal with top-level paren 
                    if(!content.empty()){
                        thread_contents.push_back(content);
                        content.clear();
                    }
                    string par_content;
                    int paren_depth = 1;
                    while (iss >> c) {
                        par_content += c;
                        if (c == '(') paren_depth++;
                        else if (c == ')') paren_depth--;
                        if (paren_depth == 0) break;
                        
                    }
                    
                    thread_contents.push_back(par_content);
                } 
                else if (isspace(c)&&brace_depth==1){
                    if(!content.empty()){
                        thread_contents.push_back(content);
                        content.clear();
                    }
                }
                
                else {
                    content+=c;
                }
                
            }
            if(!content.empty()){thread_contents.push_back(content);}

            
            // Launch subscopes
            vector<thread> ths;
            for(const auto& component: thread_contents){
                mutex mtx;
                ths.emplace_back([component,scope, &mtx](){
                    ScopeNode* node =new ScopeNode();
                    node->parent = scope;
                    node->channel = getThreadChannel();
                    {
                        lock_guard<mutex> lock(mtx);
                        scope->children.push_back(node);
                    }
                    process_line(component,node);
                });
            }
            for (auto& th : ths) th.join();  // wait for all to finish
            string rest;
            getline(iss,rest);
            if(!rest.empty()){
                process_line(rest, scope);
            }
        }



        // Printing and functions
        else if (opr == "info"){
            cout<<"Current Settings: "<<"BPM: "<<getNum(scope,"B")<<" Octave: "<<getNum(scope,"O")<<endl
                <<"Note Value: "<<getNum(scope,"N")<<" Transpose: "<<getNum(scope,"T")<<endl
                <<"Instrument: "<<getNum(scope, "I")<<"Volume: "<<getNum(scope,"V")<<endl;
            //Print All VARs
            auto cur = scope;
            while(cur!=nullptr){
                for (const auto& [name, value]: cur->varVars){
                    cout<<"VAR "<<name<<" = "<<value<<endl;
                }
                cur = cur->parent;
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
            if (!line.empty()) process_line(line,root);
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

    string line,buffer;
    cout << "> ";
    while (true) {
        if (!getline(cin, line)) break;
        auto trim = [](string s){
            s.erase(0, s.find_first_not_of(" \t\r\n"));
            s.erase(s.find_last_not_of(" \t\r\n") +1);
            return s;
        };
        string t = trim(line);
        buffer += line +"\n";
        if (isComplete(buffer)){
            process_line(buffer,root);
            buffer.clear();
            cout<<">";
        }
        else{
            cout<<"...";
        }
    }
}

// ---------------------------------------------
// Main
// ---------------------------------------------
int main() {
    
    root->channel = getThreadChannel();
    root->numVars = {
        {"O", 4}, //Octave
        {"T", 0}, //Transpose
        {"B", 140}, //BPM
        {"N", 4}, //Note Value
        {"V", 100}, // Velocity
        {"I", 0} //Instrument (Default: Piano)
    };

    initFluidSynth("FluidR3_GM.sf2");
    fluid_synth_program_change(g_synth, 0,25);
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
