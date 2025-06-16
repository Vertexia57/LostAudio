# LostAudio

LostAudio is a standalone static library which runs using [RtAudio](https://github.com/thestk/rtaudio).
It is taken out from the [Lost Engine](https://github.com/Vertexia57/Lost) and turned into a standalone library which can be used anywhere.  
<sup>*Note: it is not compatible with said engine*</sup>

The goal of LostAudio is to make using audio with customisable effects simple and easy to use.
LostAudio handles sound loading, unloading, streaming and playback.

## Supported Media

LostAudio only supports .wav files at 44100\* samples per second currently.
It can handle 8-bit, 16-bit, 24-bit and 32-bit audio formats  
<sup>* Files of other sample rates can be used but do not playback correctly</sup>

## Using LostAudio

The normal setup for LostAudio looks something like this:

```cpp
#include "Lost/Audio/Audio.h"

int main()
{
    lost::initAudio();  // Initliazes the audio engine on the default audio device
    while (/*update loop*/)
    {
        lost::updateAudio(); // Updates the audio engine, handling playback times and sound changes
        /*
            The rest of your update code
        */
    }
    lost::exitAudio(); // Closes the audio engine
    return 0;
}
```

To play audio you will need to load a .wav file first and then play them
The functions for `SoundStream`'s are the mostly the same

```cpp
#include "Lost/Audio/Audio.h"

int main()
{
    /* startup code */
    Sound sound = lost::loadSound("sound.wav");
    lost::playSound(sound);
    /* update and shutdown code */
}
```

## Cheat Sheet

```cpp
void initAudio();
void exitAudio();
void updateAudio();

void setMasterVolume(float volume);
float getMasterVolume();

// [=============== Sound Functions ===============]

// Plays the sound given, loopCount has 2 different modes, 0 >= Loops that many times, -1 / UINT_MAX = Loops forever
// "panning" is a value between -1.0 and 1.0, -1.0 being left, 1.0 being right, 0.0 being center
// The returned PlaybackSound is used to modify the sound being played, you do not need to run delete on it
PlaybackSound* playSound(Sound sound, float volume, float panning, unsigned int loopCount = 0);

void stopSound(Sound sound); // Stops the all sounds playing that are using this sound, use the PlaybackSound return to manage individual ones
void stopSound(const PlaybackSound* sound); // Stops the sound given
void setSoundPaused(const PlaybackSound* sound, bool paused); // Pauses/Unpauses the sound given, (NOT IMPLEMENTED)

void setSoundVolume(PlaybackSound* sound, float volume); // Sets the volume of the sound given
void setSoundPanning(PlaybackSound* sound, float panning); // The panning of the sound -1.0 is left ear, 1.0 is right ear, 0.0 is center

bool isSoundPlaying(PlaybackSound* sound); // Returns if the sound is playing

// [============ Sound Stream Functions ============]

// Plays the sound stream given, loopCount has 2 different modes, 0 >= Loops that many times, -1 / UINT_MAX = Loops forever
// "panning" is a value between -1.0 and 1.0, -1.0 being left, 1.0 being right, 0.0 being center
// The same sound stream cannot play over itself, so there is no return for this function
void playSoundStream(SoundStream soundStream, float volume = 1.0f, float panning = 0.0f, unsigned int loopCount = 0);

void stopSoundStream(SoundStream soundStream); // Stops the sound stream given
void setSoundStreamPaused(SoundStream soundStream, bool paused); // Pauses/Unpauses the sound stream given, (NOT IMPLEMENTED)

void setSoundStreamVolume(SoundStream sound, float volume); // Sets the volume of the sound given
void setSoundStreamPanning(SoundStream sound, float panning); // The panning of the sound stream -1.0f is left ear, 1.0f is right ear, 0.0f is center

bool isSoundStreamPlaying(SoundStream sound); // Returns if the sound stream is playing
```

### Loading and Unloading

There is a file in the docs included for Lost about how it handles data

Lost uses an ID system for data, this system is optional but does work in the background
You can load the same file multiple times but only the first load will do anything

LostAudio will automatically unload the data when `lost::exitAudio()` is ran, so the unload functions are optional

`unload-` functions don't always unload the data, they will only unload the data if the
amount of unload functions ran on that data matches the amount of load functions ran on that data
If you would like to bypass this use the `forceUnload-` functions

```cpp
// "id" is considered the name for that sound
// If it is left blank the name will be the directory the file was loaded from

// Sounds are loaded onto RAM

Sound loadSound(const char* soundLoc, const char* id = nullptr);
Sound getSound(const char* id);
void  unloadSound(const char* id);
void  unloadSound(Sound& sound);
void  forceUnloadSound(const char* id);
void  forceUnloadSound(Sound& sound);

// SoundStreams are loaded from disk, saving lots of ram, but cannot be played over themselves

// Only ONE sound stream can be loaded on a file at a time, the file will be considered "busy" after the first load
SoundStream loadSoundStream(const char* soundLoc, const char* id = nullptr);
SoundStream getSoundStream(const char* id);
void        unloadSoundStream(const char* id);
void        unloadSoundStream(SoundStream& sound);
void        forceUnloadSoundStream(const char* id);
void        forceUnloadSoundStream(SoundStream& sound);
```

## Implementing LostAudio

### Implementing with Precompiled Binaries

If you would like to use the precompiled x64 and x86 binaries follow here, otherwise jump to the next section

In the latest release included in the github you can find a zip folder with the latest precompiled binaries. Included in this zip folder are the:

- x64 Precompiled binaries
- x86 Precompiled binaries
- Include Headers


