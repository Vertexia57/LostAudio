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
void initAudio(); // Initializes LostAudio, playing on the primary sound device listen by the operating system
void exitAudio(); // Closes LostAudio, unloading all sounds and sound streams left loaded
void updateAudio(); // Required to be ran, updates the delta time of effects and removes sounds from the heap, saving memory

void setMasterVolume(float volume); // Sets the overall volume, multiplicative with an individual sound's volume
float getMasterVolume(); // Returns the master volume

// [=============== Sound Functions ===============]

// Plays the sound given, loopCount has 2 different modes, 0 >= Loops that many times, -1 / UINT_MAX = Loops forever
// "panning" is a value between -1.0 and 1.0, -1.0 being left, 1.0 being right, 0.0 being center
// The returned PlaybackSound is used to modify the sound being played, you do not need to run delete on it
PlaybackSound* playSound(Sound sound, float volume, float panning, unsigned int loopCount = 0);

void stopSound(Sound sound); // Stops the all sounds playing that are using this sound, use the PlaybackSound return to manage individual ones
void stopSound(const PlaybackSound* sound); // Stops the sound given
void setSoundPaused(PlaybackSound* sound, bool paused); // Pauses/Unpauses the sound given

void setSoundVolume(PlaybackSound* sound, float volume); // Sets the volume of the sound given
void setSoundPanning(PlaybackSound* sound, float panning); // The panning of the sound -1.0 is left ear, 1.0 is right ear, 0.0 is center

bool isSoundPlaying(PlaybackSound* sound); // Returns if the sound is playing

// [============ Sound Stream Functions ============]

// Plays the sound stream given, loopCount has 2 different modes, 0 >= Loops that many times, -1 / UINT_MAX = Loops forever
// "panning" is a value between -1.0 and 1.0, -1.0 being left, 1.0 being right, 0.0 being center
// The same sound stream cannot play over itself, so there is no return for this function
void playSoundStream(SoundStream soundStream, float volume = 1.0f, float panning = 0.0f, unsigned int loopCount = 0);

void stopSoundStream(SoundStream soundStream); // Stops the sound stream given
void setSoundStreamPaused(SoundStream soundStream, bool paused); // Pauses/Unpauses the sound stream given

void setSoundStreamVolume(SoundStream sound, float volume); // Sets the volume of the sound given
void setSoundStreamPanning(SoundStream sound, float panning); // The panning of the sound stream -1.0f is left ear, 1.0f is right ear, 0.0f is center

bool isSoundStreamPlaying(SoundStream sound); // Returns if the sound stream is playing

// [============ Sound Effect Functions ============]

void addGlobalEffect(Effect* effect); // Adds an effect to the master track
void removeGlobalEffect(Effect* effect); // Removes an effect from the master track

/* UNIMPLEMENTED /
 * 
 * void addEffectToSound(Effect* effect, PlaybackSound* sound); // Adds an effect to a singular sound
 * void removeEffectFromSound(Effect* effect, PlaybackSound* sound); // Removes an effect from a singular sound
 * void addEffectToSoundStream(Effect* effect, SoundStream soundStream);// Adds an effect to a singular sound stream
 * void removeEffectFromSoundStream(Effect* effect, SoundStream soundStream);// Removes an effect from a singular sound stream
 *
 */

// Inherit from this if making custom effects
class Effect {
public:
    virtual void processChunk(AudioSample[LOST_AUDIO_BUFFER_COUNT] in, AudioSample[LOST_AUDIO_BUFFER_COUNT] out) = 0;
}

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

## Effects

**Stuff gets a little complicated here, you only need to know this stuff if you want to write your own effects**

There are 3 effects included in the audio engine, a Low Pass Filter, High Pass Filter, and Delay effect.
Effects are one of the few things in LostAudio which need to be deleted manually afterwards.

### Custom Effects

LostAudio allows for the use of custom effects, but there is a bit you should know about how audio is handled under the hood first.

At the end of processing a sound or the master track, LostAudio will apply any effects on that sound or the master track. To do this it runs the function `processChunk()` on all audio effects.

`processChunk()` is defined as:

```cpp
virtual void processChunk(AudioSample inBuffer[LOST_AUDIO_BUFFER_COUNT], AudioSample outBuffer[LOST_AUDIO_BUFFER_COUNT]);
```

`LOST_AUDIO_BUFFER_COUNT` is a macro defined by the `Audio.h` header, it is set to the amount of samples the audio buffer has **in total**, which is the amount of overrall samples multiplied by the amount of channels used in the engine, which is always 2 in this case

There are other macros which are useful to use in effects, here's a few:

- `LOST_AUDIO_BUFFER_COUNT`: Total sample count
- `LOST_AUDIO_BUFFER_FRAMES`: Sample count **per channel**
- `LOST_AUDIO_CHANNELS`: Total channels in the engine, will always be **2**
- `LOST_AUDIO_SAMPLE_RATE`: The sample rate of the audio engine, by default **44100**

Another important thing to explain is what AudioSample is: AudioSample is an **integer** that fits the engine's bit quality, by default it is **16 bits**, if the engine is built in high quality mode it will be 32 bit, if it is built at low quality mode it will be 8 bit, 24 bit isn't supported

LostAudio uses **PCM Signed Integer audio**, a lot of audio effects require the samples of audio to be floats in a range of -1.0 to 1.0, for this there are 2 included functions:

```cpp
float audioSampleToFloat(AudioSample); // Converts an AudioSample to a float between -1.0 and 1.0
AudioSample floatToAudioSample(float); // Converts a float between -1.0 and 1.0 to an AudioSample
```

#### Writing custom effects

It's important to understand how the data in the audio buffers are formatted, when audio data is passed to an effect it is given in an interlaced form: left ear, right ear, left ear, ...
The `outBuffer`'s data should be set in the same way.

In effects where the effect's changes to the samples doesn't depend on the other samples the interlacing doesn't matter, but in effects where the past samples does matter, you will need to account for this.

Here's an example of a custom amplify effect:

```cpp
// Effects use inheritance, you will need to inherit from lost::Effect to apply the effect
class AmplifyEffect : public lost::Effect
{
public:
    // processChunk is the function which is ran by the audio thread
    virtual void processChunk(AudioSample inBuffer[LOST_AUDIO_BUFFER_COUNT], AudioSample outBuffer[LOST_AUDIO_BUFFER_COUNT]) override
    {
        // "inBuffer" is a buffer containing the sound data from the previous effect or the master track
        const float amplifyAmount = 2.0f;
        // Loop over all samples in the inBuffer, amplify them and set them in the out buffer
        for (int i = 0; i < LOST_AUDIO_BUFFER_COUNT; i++)
            outBuffer[i] = inBuffer[i] * amplifyAmount;
    }   
}
```

### Applying Effects

There are 6 functions which you can use with effects:

```cpp
// Master Track Effects
void addGlobalEffect(Effect* effect);
void removeGlobalEffect(Effect* effect);
// Single Sound Effects
void addEffectToSound(Effect* effect, PlaybackSound* sound);
void removeEffectFromSound(Effect* effect, PlaybackSound* sound);
// Single Sound Stream Effects
void addEffectToSoundStream(Effect* effect, SoundStream soundStream);
void removeEffectFromSoundStream(Effect* effect, SoundStream soundStream);
```

Note: Like described before, Effects are one of the only things that need to be manually deleted in the engine, here's an example of applying an effect to the master track and removing it:

```cpp
// Other Stuff

lost::Effect* effect = new lost::LowPassFilter(200); // Create the effect
lost::addGlobalEffect(effect); // Add it to the master track
// Your sounds
lost::removeGlobalEffect(effect); // Remove it from the master track
delete effect; // Delete the effect from the heap

// Other Stuff
```

## Implementing LostAudio

### Implementing with Precompiled Binaries

If you would like to use the precompiled x64 and x86 binaries follow here, otherwise jump to the next section

In the latest release included in the github you can find a zip folder with the latest precompiled binaries. Included in this zip folder are the:

- x64 Precompiled binaries
- x86 Precompiled binaries
- Include Headers

To include LostAudio within a solution in Visual Studio you will need to modify the project settings you're including it in, as follows:

- Add include directory, make sure you have Configuration set to "All Configurations" and Platform set to "All Platforms" when you do this

![image](https://github.com/user-attachments/assets/fe15445c-d525-4fec-b446-a1cf48d78803)

- You should add one that looks something like this but it is up to you where you put it

![image](https://github.com/user-attachments/assets/4d591c29-e7af-4878-89f7-a3fab71903c3)

- Add library directory

![image](https://github.com/user-attachments/assets/846e49d9-7d2d-4b09-9c21-0a4746a33bae)
![image](https://github.com/user-attachments/assets/d82df36a-8225-4415-97cc-8f94654c1e78)

- Setup debug, release and their respective x64 and x86 libraries, Note each one has to be set for each individual configuration and platform
    - LostAudio.lib = Release x64
    - LostAudio_d.lib = Debug x64
    - LostAudio_x86.lib = Release x86
    - LostAudio_x86d.lib = Debug x86
    - LostAudio_mt.lib = Release x64 Multi-Threaded / "/MT" build
    - LostAudio_x86mt.lib = Release x86 Multi-Threaded / "/MT" build

![image](https://github.com/user-attachments/assets/5565947c-f0de-45b0-9fe1-93ff3d0e32de)

Once you've done that, you should be all good to go!

### Implementing from Source

Currently the github does not include CMake and so this feature isn't properly supported
If you are using Visual Studio you can download the entire repository and build from there and then follow the previous steps listed
