# Muse - A Beeping Music Generator in C++

### Syntax
**a b c d e f g**: Playing notes separated by a space.

**O**: Setting Octave by adding the octave number after a space. E.g. **O 5** to set to the fifth octave

**T**: Setting Transpose. E.g. **T 1** for a transposition by 1 semitone.

**N**: Setting Note Value. E.g. **N 16** for a 16th note.

**VAR**: Creating a variable for a series of notes (and operations). E.g. **VAR tune (a b c)**.

**/**: Playing a variable. E.g. **/tune** to play **a b c** as defined above.

**++** and **--** (Not finished): To add 1 or subtract 1 from a number (O, T, N, etc.) E.g. **T ++** for a transposition to be higher by 1 semitone.

**quit**: Quiting the program.

### Usage
In the interactive mode, type notes and operations to play. Or in the data.txt, write melodies and run the program directly to hear them.
