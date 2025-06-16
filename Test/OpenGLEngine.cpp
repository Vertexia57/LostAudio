#include <iostream>

#include <thread>

#include "../Lost/Audio/Audio.h"

// Thread safe boolean for writing
lost::_HaltWrite<bool> m_ProgramClosed = false;
void audioThread()
{
	// This is our general while loop
	// In a game this would be our "while(lost::windowOpen())" loop
	while (!m_ProgramClosed.read())
	{
		lost::updateAudio();
	}
}

int main()
{
	// Initialize audio thread
	lost::initAudio();

	lost::Sound sound = lost::loadSound("../data/sound.wav");
	lost::SoundStream soundStream = lost::loadSoundStream("../data/Cascade.wav");

	// Initialize audio update thread, this is essentially our while loop
	// But since this is a console application we don't have a way to pause until an input is given
	// without pausing the whole thread, so we have to do it on a different one
	std::thread audThread(&audioThread);
	
	// Pause main thread until enter is pressed
	std::cout << "Press Enter to play Sound (Loaded on RAM)";
	std::cin.ignore();
	lost::playSound(sound);
	std::cout << "Press Enter to stop";
	std::cin.ignore();
	lost::stopSound(sound);

	std::cout << "Press Enter to play SoundStream (Loaded from disk)";
	std::cin.ignore();
	lost::playSoundStream(soundStream);
	std::cout << "Press Enter to stop";
	std::cin.ignore();
	lost::stopSoundStream(soundStream);

	// Tell the audio thread the program is closed
	m_ProgramClosed.write(true);
	// Wait for it to finish
	audThread.join();

	// [=============== OPTIONAL ===============]
	lost::unloadSound(sound);
	lost::unloadSoundStream(soundStream);
	// [=============== OPTIONAL ===============]

	// Close audio system
	lost::exitAudio();
}