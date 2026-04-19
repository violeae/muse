# Muse

Muse is a command-line melody player that reads a text-based command sheet and plays notes through a SoundFont via FluidSynth. It can also export played notes into a MIDI file.

## Project Layout

- `src/` - application sources
- `include/` - application headers
- `midifile/` - vendored [midifile](https://github.com/craigsapp/midifile) library source
- `example/` - sample melody command sheets
- `Makefile` - standard build entry point

## Build

Requirements:

- C++17 compiler (`g++` or `clang++`)
- GNU `make`
- FluidSynth development package (headers + library)

Build:

```bash
make
```

Binary output:

```text
bin/muse
```

## Run

```bash
./bin/muse --soundfont FluidR3_GM.sf2 --data data.txt --preload preload.muse --midi outputMidi.midi
```

Streaming mode (route events to MIDI out / loopMIDI):

```bash
./bin/muse --stream --stream-port 0 --data data.txt --preload preload.muse
```

Options:

- `--soundfont <path>`: SoundFont file (`.sf2`)
- `--data <path>`: watched command file
- `--preload <path>`: preload script run at startup
- `--midi <path>`: output MIDI path used by the `save` command
- `--stream`: send note events to MIDI out port instead of direct FluidSynth audio
- `--stream-port <n>`: MIDI out port index used in streaming mode
- `--help`: show help

## Command Language (Current)

- Notes: `c d e f g a b` with sharps/flats (`c#`, `db`, ...)
- Rest: `r`
- Scope variables:
  - `NUM <name> <value>`
  - `VAR <name> (<commands>)`
  - `/name` to execute a `VAR`
- Controls:
  - `>` / `<` octave shift
  - `if <numVar> (<commands>)`
  - `{ ... }` parallel components
- Utility:
  - `info` print settings
  - `save` write MIDI
  - `clear` clear terminal
  - `quit` exit
