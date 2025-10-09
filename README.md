# Muse - Compose Music in Notes
### A Music Generator written completely in C++, with FluidSynth libraries.

## Syntax

### Play Notes

**a b c d e f g**: Playing notes separated by a space.

**r**: a rest (with the same note value as a regular note).

### NUMs

**NUM**: Creating a NUM to hold a number. E.g. **NUM A 5**.

**O**: Setting Octave by adding the octave number after a space. E.g. **O 5** to set to the fifth octave.

**T**: Setting Transpose. E.g. **T 1** for a transposition by 1 semitone.

**N**: Setting Note Value. E.g. **N 16** for a 16th note.

**I**: Setting Instrument. Note the index on midi instrument list should be subtracted by 1 since it starts from 0 in the program. So piano is 0 instead of 1.

**V**: Setting Velocity. You can think of it as volume.

### VARs

**VAR**: Creating a VAR for a series of notes (and operations). E.g. **VAR tune (a b c)**.

### Operations and Built-in Functions

**/**: Playing a VAR. E.g. **/tune** to play **a b c** as defined above.

**>** and **<**: To increase or decrease octave by 1.

**++** and **--**: To add 1 or subtract 1 from a NUM (O, T, N, etc.) E.g. **T ++** for a transposition to be higher by 1 semitone.

**>>** and **<<**: To divide by 2 (result floored) or to multiply by 2. E.g. **N 4 N <<** to make a quarter note into an eighth note.

**if**: For setting a condition for a sentence to be executed. E.g. **if A (/tune)** only executes **/tune** when **A != 0**.

**{ () }**: For simultaneous playback. E.g. **{ c e g ( > c e g ) }** will play c/e/g/(>c) together first and then play e then g.

NUMs, VARs declared in a pair of braces are only effective in its own scope. The parsing is organised through a tree of nodes (ScopeNodes) 
where any reference will be looked up from the current node to the root (global settings) where channels are assigned only on initialisation.
However channel 9 (0-index) will produce drums/percussions so it is skipped. So if more than 15 nodes are playing at the same time, there may be some distortions.

**quit**: Quiting the program.

## Usage

In the interactive mode, type notes and operations to play. Or in the data.txt, write melodies and run the program directly to hear them.

Actually, by setting a NUM and --, you can achieve a loop for a definite amount of times. Also, by placing **/B** inside B, you can also create an infinite loop.

Have Fun!
