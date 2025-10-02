#include<bits/stdc++.h>
#include<Windows.h>
using namespace std;

//Initial Status
int octave=4;
int transpose=0;
int bpm=120;
int note=4;

set<char> keys={'c','d','e','f','g','a','b'};

map<char,double> oct4={
    {'c',261.63},
    {'d',293.66},
    {'e',329.63},
    {'f',349.23},
    {'g',392.00},
    {'a',440.00},
    {'b',493.88},
};

void ofreq(const char &m){
    double note_value=60000.0/bpm*4.0/note;
    double freq=oct4[m]*pow(2,octave-4);

    Beep(freq,note_value);
}

void check(const string &opr){
    int oprlen=opr.length();
    if(keys.count(opr[0])==1){ofreq(opr[0]);}
    else if(opr[0]=='O'){octave = int(opr[1]-'0');}
    else if(opr[0]=='N'){ //setting note value 1,2,4,8,16 etc;
        int note_temp=0;
        for(int i=1;i<=oprlen-1;i++){
            note_temp+=int(opr[i]-'0')*int(pow(10,oprlen-i-1));
        }
        note=note_temp;
    }
    else if(opr[0]=='>'){octave++;}
    else if(opr[0]=='<'){octave--;}

}

int main(){
    while(1){
        string opr;
        cin>>opr;
        if(opr=="quit"){return 0;}
        check(opr);
    }



    return 0;
}