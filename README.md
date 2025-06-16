# LostAudio

LostAudio is a standalone static library which runs using [RtAudio](https://github.com/thestk/rtaudio).
It is taken out from the [Lost Engine](https://github.com/Vertexia57/Lost) and turned into a standalone library which can be used anywhere.  
<sup>*Note: it is not compatible with said engine*</sup>

The goal of LostAudio is to make using audio with customisable effects simple and easy to use.
LostAudio handles sound loading, unloading, streaming and playback.

- [[##Implementing LostAudio|How to add LostAudio to your project]]

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

More functions can be found in the docs included in the docs folder

## Implementing LostAudio

### Implementing with Precompiled Binaries

If you would like to use the precompiled x64 and x86 binaries follow here, otherwise jump to the next section

In the latest release included in the github you can find a zip folder with the latest precompiled binaries. Included in this zip folder is:

- x64 Precompiled binaries
- x86 Precompiled binaries
- Include Headers

