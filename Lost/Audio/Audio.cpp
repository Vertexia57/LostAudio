#include "Audio.h"
#include "../Log.h"
#include "../DeltaTime.h"
#include "Effects.h"

#include "ResourceManagers/AudioResourceManagers.h"

#include <stdio.h>
#include <vector>

#ifndef PI
#define PI 3.141592653589
#endif // !PI

namespace lost
{
	class _Sound;
	class _SoundStream;

	struct SamplerPassInInfo
	{
		_HaltWrite<std::vector<PlaybackSound*>> activeSounds;
		_HaltWrite<std::vector<_SoundStream*>>  activeStreams;
		_HaltWrite<std::vector<Effect*>>        activeEffects;
	};

	int playRaw(void* outputBuffer, void* inputBuffer, unsigned int nBufferFrames,
		double streamTime, RtAudioStreamStatus status, void* data)
	{

		// This will compile all sounds and soundstreams into one buffer
		
		// Since the audio is on a different thread we need to make sure we don't run into any
		// race conditions. We can use mutex to do this
		
		// The sounds may be in different PCM formats, we will need to convert them into the same as this one
		// It should be processed beforehand

		// Eg. Sound format is 32-PCM and out is 16-PCM
		// Sound in >> 16 (Bitshift the input 16 bits to the right)
		// Eg. Sound format is 8-PCM and out is 16-PCM
		// Sound in << 8 (Bitshift the input 8 bits to the left)

		SamplerPassInInfo* inData = (SamplerPassInInfo*)data;
		
		// Will ALWAYS be 2!!!
		const unsigned int channelCount = LOST_AUDIO_CHANNELS;
		const float volumeDecrement = 1.0f / 4.0f; // The second number is prefered to be a power of 2

		_ChannelQuality* outData = (_ChannelQuality*)outputBuffer;
		_ChannelQuality outDataSwapBuffer[LOST_AUDIO_BUFFER_COUNT];
		unsigned int outDataCapacity = _EngineBytesPerSample * nBufferFrames * channelCount;

		memset((char*)outputBuffer, 0, outDataCapacity);

		// [==========================]
		//         Update Sounds
		// [==========================]

		std::vector<PlaybackSound*> sounds = inData->activeSounds.read();
		for (unsigned int i = 0; i < sounds.size(); i++)
		{
			if (sounds.at(i)->_getPaused())
				continue;

			_PlaybackData& playbackData = sounds.at(i)->_getPlaybackData();

			unsigned int inFormatSize = playbackData.bytesPerSample;
			int formatFactor = playbackData.formatFactor;

			int inChannelCount = playbackData.channelCount;

			unsigned int mask = 0;
			bool isSigned = true;
			switch (inFormatSize)
			{
			case 1:
				//isSigned = false;
				//mask = 0x000000FF;
				//break; 
				sounds.at(i)->_setIsPlaying();
				continue; // Skip to the next sound, currently there's no support for this
			case 2:
				mask = 0x0000FFFF;
				break;
			case 3:
				mask = 0x00FFFFFF;
				break;
			case 4:
				mask = 0xFFFFFFFF;
				break;
			default:
				break;
			}

			unsigned int bytesLeft = playbackData.loopCount > 0 ? UINT_MAX : playbackData.dataCount - playbackData.currentByte;
			if (bytesLeft == 0)
				continue;
			
			// Checks if the amount of data left in the raw sound is enough to fill the buffer
			// byteWrite is the amount of samples it will write into the outBuffer
			bool fillsBuffer = nBufferFrames * channelCount < bytesLeft / inFormatSize * channelCount / inChannelCount;
			unsigned int sampleWrite = fillsBuffer ? nBufferFrames * channelCount : bytesLeft / inFormatSize * channelCount / inChannelCount;

			float volume = sounds.at(i)->_getVolume() * volumeDecrement * getMasterVolume();
			float panning = fmaxf(fminf(sounds.at(i)->_getPanning(), 1.0f), -1.0f);

			for (int sample = 0; sample < sampleWrite / channelCount; sample++)
			{

				_ChannelQuality channelOutputs[channelCount];

				// Calculate the per channel data
				for (int channel = 0; channel < channelCount; channel++)
				{
					// Get the amount of bytes to go through the data for this sample
					unsigned int sampleOffset = playbackData.currentByte + (sample * inChannelCount + (inChannelCount == 2 ? channel : 0)) * inFormatSize;

					// Loop sound read offset
					if (playbackData.loopCount > 0 && sampleOffset > playbackData.dataCount)
					{
						playbackData.currentByte -= playbackData.dataCount;
						sampleOffset = sampleOffset % playbackData.dataCount;
						if (playbackData.loopCount != UINT_MAX)
							playbackData.loopCount--;
					}

					// The value of the sample cast to an integer, doesn't scale to fit range
					int outSample = (*(int*)(playbackData.data + sampleOffset) & mask);

					// Scale output and store it for pan processing, apply volume here
					if (formatFactor >= 0)
						channelOutputs[channel] = (_ChannelQuality)(outSample >> (formatFactor * 8)) * volume;
					else
						channelOutputs[channel] = (_ChannelQuality)(outSample << (-formatFactor * 8)) * volume;
				}

				// This is the amount to merge the right channel into the left channel
				float leftPanAmount  = -fminf(panning, 0.0f) * PI / 2.0f;
				float rightPanAmount =  fmaxf(panning, 0.0f) * PI / 2.0f;

				// Apply pan
				outData[sample * channelCount + 0] += channelOutputs[0] * fmaxf(sinf(leftPanAmount),  0.0f);
				outData[sample * channelCount + 1] += channelOutputs[0] * fmaxf(cosf(leftPanAmount),  0.0f);
				outData[sample * channelCount + 1] += channelOutputs[1] * fmaxf(sinf(rightPanAmount), 0.0f);
				outData[sample * channelCount + 0] += channelOutputs[1] * fmaxf(cosf(rightPanAmount), 0.0f);
			}

			// Seek to the next data we need to read, if it's the end of the data and we've finished looping, stop the sound
			if (playbackData.currentByte + sampleWrite * inFormatSize / channelCount * inChannelCount < playbackData.dataCount - 1)
				playbackData.currentByte += sampleWrite * inFormatSize / channelCount * inChannelCount;
			else if (playbackData.loopCount == 0)
			{
				playbackData.currentByte = playbackData.dataCount;
				sounds.at(i)->_setIsPlaying();
			}
		}

		// [==========================]
		//     Update Sound Streams
		// [==========================]

		std::vector<_SoundStream*> streams = inData->activeStreams.read();
		for (unsigned int i = 0; i < streams.size(); i++)
		{
			_SoundStream& stream = *streams.at(i);

			if (stream._getPaused())
				continue;

			const _SoundInfo& streamInfo = stream._getSoundInfo();

			unsigned int inFormatSize = streamInfo.bitsPerSample / 8;
			int formatFactor = stream._getFormatFactor();

			int inChannelCount = stream._getSoundInfo().channelCount;

			unsigned int mask = 0;
			bool isSigned = true;
			switch (inFormatSize)
			{
			case 1:
				//isSigned = false;
				//mask = 0x000000FF;
				//break; 
				stream._setIsPlaying(false);
				continue; // Skip to the next sound, currently there's no support for this
			case 2:
				mask = 0x0000FFFF;
				break;
			case 3:
				mask = 0x00FFFFFF;
				break;
			case 4:
				mask = 0xFFFFFFFF;
				break;
			default:
				break;
			}

			// Figure out how many samples to write (samples * channels)
			unsigned int bytesLeft = stream._getBytesLeftToPlay();
			unsigned int sampleWrite = nBufferFrames * channelCount < bytesLeft / inFormatSize ? nBufferFrames * channelCount : bytesLeft / inFormatSize;

			// Get data in const char* form
			const char* data = stream._getNextDataBlock();

			// Get volume and panning info
			float volume = stream._getVolume() * volumeDecrement * getMasterVolume();
			float panning = fmaxf(fminf(stream._getPanning(), 1.0f), -1.0f);

			for (int sample = 0; sample < sampleWrite / channelCount; sample++)
			{
				_ChannelQuality channelOutputs[channelCount];

				// Get sample data
				for (int channel = 0; channel < channelCount; channel++)
				{
					// Get the amount of bytes to go through the data for this sample
					unsigned int sampleOffset = (sample * inChannelCount + (inChannelCount == 2 ? channel : 0)) * inFormatSize;

					// We don't need to do loop processing here as it is done by _getNextDataBlock()

					// The value of the sample cast to an integer, doesn't scale to fit range
					int outSample = (*(int*)(data + sampleOffset) & mask);

					// Scale output and store it for pan processing, apply volume here
					if (formatFactor >= 0)
						channelOutputs[channel] = (_ChannelQuality)(outSample >> (formatFactor * 8)) * volume;
					else
						channelOutputs[channel] = (_ChannelQuality)(outSample << (-formatFactor * 8)) * volume;
				}

				// This is the amount to merge the right channel into the left channel
				float leftPanAmount = -fminf(panning, 0.0f) * PI / 2.0f;
				float rightPanAmount = fmaxf(panning, 0.0f) * PI / 2.0f;

				// Apply pan
				outData[sample * channelCount + 0] += channelOutputs[0] * fmaxf(sinf(leftPanAmount), 0.0f);
				outData[sample * channelCount + 1] += channelOutputs[0] * fmaxf(cosf(leftPanAmount), 0.0f);
				outData[sample * channelCount + 1] += channelOutputs[1] * fmaxf(sinf(rightPanAmount), 0.0f);
				outData[sample * channelCount + 0] += channelOutputs[1] * fmaxf(cosf(rightPanAmount), 0.0f);
			}

			// If it's the end of the data and we've finished looping, stop the sound
			if (sampleWrite == bytesLeft / inFormatSize)
				stream._setIsPlaying(false);
		}

		// [==========================]
		//     Update Audio Effects
		// [==========================]

		std::vector<Effect*> effects = inData->activeEffects.read();
		bool bufferSwapped = false;
		for (unsigned int i = 0; i < effects.size(); i++)
		{
			if (!bufferSwapped)
				effects.at(i)->processChunk(outData, outDataSwapBuffer);
			else
				effects.at(i)->processChunk(outDataSwapBuffer, outData);
			bufferSwapped = !bufferSwapped;
		}

		if (bufferSwapped)
			memcpy(outData, outDataSwapBuffer, outDataCapacity);

		return 0;
	}

	class AudioHandler
	{
	public:
		AudioHandler()
			: m_SamplerPassInInfo{ { {} }, { {} }, { {} } }
		{

		}

		void init(RtAudioFormat format = _RTAudioFormat)
		{
			std::vector<unsigned int> deviceIds = m_Dac.getDeviceIds();

			debugLogIf(deviceIds.size() < 1, "No audio devices found!", LOST_LOG_ERROR);

			m_OutputParameters.deviceId = m_Dac.getDefaultOutputDevice();
			m_OutputParameters.nChannels = 2;
			m_OutputParameters.firstChannel = 0;

			m_CurrentDeviceName = m_Dac.getDeviceInfo(m_OutputParameters.deviceId).name;

			m_Format = format;

			m_Dac.openStream(&m_OutputParameters, NULL, format, LOST_AUDIO_SAMPLE_RATE, &m_BufferFrames, &playRaw, (void*)&m_SamplerPassInInfo);
			m_Dac.startStream();
			
#ifdef LOST_DEBUG_MODE
			std::string formatSize = "???";
			switch (format)
			{
			case 1:
			case 2:
				formatSize = std::to_string(format << 3);
				break;
			case 8:
				formatSize = "32";
				break;
			}

			debugLog("Successfully initialized audio on device: " + m_CurrentDeviceName + ", at " + formatSize + " bits per sample", LOST_LOG_SUCCESS);
#endif
		}

		void exit()
		{
			if (m_Dac.isStreamOpen()) m_Dac.closeStream();

			const std::vector<PlaybackSound*>& list1 = m_SamplerPassInInfo.activeSounds.read();
			for (PlaybackSound* sound : list1)
				delete sound;
		}

		unsigned int getBufferFrameCount() const { return m_BufferFrames; };
		RtAudio& getDac() { return m_Dac; }
		RtAudio::StreamParameters& getOutputStreamParams() { return m_OutputParameters; }
		RtAudioFormat getAudioFormat() const { return m_Format; };
		
		// Loopcount - when UINT_MAX / -1 - will cause the sound to loop forever, only stopped by stopSound
		PlaybackSound* playSound(_Sound* sound, float volume, float panning, unsigned int loopCount)
		{
			std::mutex& soundMutex = m_SamplerPassInInfo.activeSounds.getMutex();
			PlaybackSound* pbSound = new PlaybackSound(sound, volume, panning, loopCount);

			soundMutex.lock();
			m_SamplerPassInInfo.activeSounds.getWriteRef().push_back(pbSound);
			m_SamplerPassInInfo.activeSounds.forceDirty();
			soundMutex.unlock();

			return pbSound;
		}

		void stopSound(const PlaybackSound* sound)
		{
			std::mutex& soundMutex = m_SamplerPassInInfo.activeSounds.getMutex();

			unsigned int location = -1;
			PlaybackSound* ref = nullptr;
			std::vector<PlaybackSound*>& writeRef = m_SamplerPassInInfo.activeSounds.getWriteRef();
			for (int i = 0; i < writeRef.size(); i++)
			{
				if (writeRef.at(i) == sound)
				{
					location = i;
					ref = writeRef.at(i);
					break;
				}
			}

			if (ref)
			{
				m_GarbageSounds.push_back({ 0.0, ref });
				soundMutex.lock();
				writeRef.erase(writeRef.begin() + location);
				m_SamplerPassInInfo.activeSounds.forceDirty();
				soundMutex.unlock();
			}
			else
			{
				debugLog("Tried to stop a sound that had already finished playing or doesn't exist", LOST_LOG_WARNING_NO_NOTE);
			}
		}

		void stopSounds(const _Sound* sound)
		{
			std::mutex& soundMutex = m_SamplerPassInInfo.activeSounds.getMutex();

			unsigned int location = -1;
			PlaybackSound* ref = nullptr;
			std::vector<PlaybackSound*>& writeRef = m_SamplerPassInInfo.activeSounds.getWriteRef();
			for (int i = writeRef.size() - 1; i >= 0; i--)
			{
				if (writeRef.at(i)->getParentSound() == sound)
				{
					location = i;
					ref = writeRef.at(i);

					m_GarbageSounds.push_back({ 0.0, ref });
					soundMutex.lock();
					writeRef.erase(writeRef.begin() + location);
					m_SamplerPassInInfo.activeSounds.forceDirty();
					soundMutex.unlock();
				}
			}
		}

		void endSounds(float deltaTime)
		{
			std::mutex& soundMutex = m_SamplerPassInInfo.activeSounds.getMutex();
			std::vector<PlaybackSound*>& writeRef = m_SamplerPassInInfo.activeSounds.getWriteRef();
			for (int i = writeRef.size() - 1; i >= 0; i--)
			{
				if (!writeRef.at(i)->isPlaying())
				{
					m_GarbageSounds.push_back({ 0.0, writeRef.at(i) });
					soundMutex.lock();
					writeRef.erase(writeRef.begin() + i);
					m_SamplerPassInInfo.activeSounds.forceDirty();
					soundMutex.unlock();
				}
			}

			for (int i = m_GarbageSounds.size() - 1; i >= 0; i--)
			{
				m_GarbageSounds.at(i).first += deltaTime;
				if (m_GarbageSounds.at(i).first >= m_CullTime)
				{
					delete m_GarbageSounds.at(i).second;
					m_GarbageSounds.erase(m_GarbageSounds.begin() + i);
				}
			}
		}

		/*void destroyEffects(float deltaTime)
		{
			std::mutex& effectMutex = m_SamplerPassInInfo.activeEffects.getMutex();
			for (int i = m_GarbageEffects.size() - 1; i >= 0; i--)
			{
				m_GarbageEffects.at(i).first += deltaTime;
				if (m_GarbageEffects.at(i).first >= m_CullTime)
				{
					delete m_GarbageEffects.at(i).second;
					m_GarbageEffects.erase(m_GarbageEffects.begin() + i);
				}
			}
		}*/

		bool hasSound(const PlaybackSound* sound)
		{
			std::vector<PlaybackSound*>& writeRef = m_SamplerPassInInfo.activeSounds.getWriteRef();
			for (int i = 0; i < writeRef.size(); i++)
			{
				if (writeRef.at(i) == sound)
					return true;
			}
			return false;
		}

		bool hasSoundStream(const SoundStream sound)
		{
			std::vector<_SoundStream*>& writeRef = m_SamplerPassInInfo.activeStreams.getWriteRef();
			for (int i = 0; i < writeRef.size(); i++)
			{
				if (writeRef.at(i) == sound)
					return true;
			}
			return false;
		}

		void playSoundStream(_SoundStream* soundStream, float volume, float panning, unsigned int loopCount)
		{
			std::mutex& streamMutex = m_SamplerPassInInfo.activeStreams.getMutex();

			soundStream->_prepareStartPlay(volume, panning, loopCount);
			soundStream->_setActive(true);
			soundStream->_setIsPlaying(true);

			streamMutex.lock();
			m_SamplerPassInInfo.activeStreams.getWriteRef().push_back(soundStream);
			m_SamplerPassInInfo.activeStreams.forceDirty();
			streamMutex.unlock();
		}

		void stopSoundStream(_SoundStream* soundStream)
		{
			std::mutex& streamMutex = m_SamplerPassInInfo.activeStreams.getMutex();

			unsigned int location = -1;
			_SoundStream* ref = nullptr;
			std::vector<_SoundStream*>& writeRef = m_SamplerPassInInfo.activeStreams.getWriteRef();
			for (int i = 0; i < writeRef.size(); i++)
			{
				if (writeRef.at(i) == soundStream)
				{
					location = i;
					ref = writeRef.at(i);
					break;
				}
			}

			if (ref)
			{
				m_GarbageStreams.push_back({ 0.0, ref });
				streamMutex.lock();
				writeRef.erase(writeRef.begin() + location);
				m_SamplerPassInInfo.activeStreams.forceDirty();
				streamMutex.unlock();
			}
			else
			{
				debugLog("Tried to stop a sound stream that had already finished playing or doesn't exist", LOST_LOG_WARNING_NO_NOTE);
			}
		}

		void endSoundStreams(float deltaTime)
		{
			std::mutex& streamMutex = m_SamplerPassInInfo.activeStreams.getMutex();
			std::vector<_SoundStream*>& writeRef = m_SamplerPassInInfo.activeStreams.getWriteRef();
			for (int i = writeRef.size() - 1; i >= 0; i--)
			{
				if (!writeRef.at(i)->isPlaying())
				{
					m_GarbageStreams.push_back({ 0.0, writeRef.at(i) });
					streamMutex.lock();
					writeRef.erase(writeRef.begin() + i);
					m_SamplerPassInInfo.activeStreams.forceDirty();
					streamMutex.unlock();
				}
			}

			for (int i = m_GarbageStreams.size() - 1; i >= 0; i--)
			{
				m_GarbageStreams.at(i).first += deltaTime;
				if (m_GarbageStreams.at(i).first >= m_CullTime)
				{
					m_GarbageStreams.at(i).second->_setActive(false);
					m_GarbageStreams.erase(m_GarbageStreams.begin() + i);
				}
			}
		}

		void update(float deltaTime)
		{
			endSounds(deltaTime);
			endSoundStreams(deltaTime);
			//destroyEffects(deltaTime);
		}

		void setMasterVolume(float volume)
		{
			a_MasterVolume.write(volume);
		}

		float getMasterVolume()
		{
			return a_MasterVolume.read();
		}

		void addGlobalEffect(Effect* effect)
		{
			std::mutex& effectMutex = m_SamplerPassInInfo.activeEffects.getMutex();
			effectMutex.lock();
			m_SamplerPassInInfo.activeEffects.getWriteRef().push_back(effect);
			m_SamplerPassInInfo.activeEffects.forceDirty();
			effectMutex.unlock();
		}

		void removeGlobalEffect(Effect* effect)
		{
			std::mutex& effectMutex = m_SamplerPassInInfo.activeEffects.getMutex();

			unsigned int location = -1;
			Effect* ref = nullptr;
			std::vector<Effect*>& writeRef = m_SamplerPassInInfo.activeEffects.getWriteRef();
			for (int i = 0; i < writeRef.size(); i++)
			{
				if (writeRef.at(i) == effect)
				{
					location = i;
					ref = writeRef.at(i);
					break;
				}
			}

			if (ref)
			{
				//m_GarbageEffects.push_back({ 0.0, ref });
				effectMutex.lock();
				writeRef.erase(writeRef.begin() + location);
				m_SamplerPassInInfo.activeEffects.forceDirty();
				effectMutex.unlock();
			}
			else
			{
				debugLog("Tried to remove an effect that had already been removed or wasn't added in the first place", LOST_LOG_WARNING_NO_NOTE);
			}
		}

	private:
		RtAudio m_Dac;
		RtAudio::StreamParameters m_OutputParameters;
		std::string m_CurrentDeviceName;

		SamplerPassInInfo m_SamplerPassInInfo;

		// Audio stream settings
		RtAudioFormat m_Format;
		unsigned int m_BufferFrames = LOST_AUDIO_BUFFER_FRAMES;

		_HaltWrite<float> a_MasterVolume = 1.0f;

		// lifetime + playbackSound*
		std::vector<std::pair<double, PlaybackSound*>> m_GarbageSounds;
		std::vector<std::pair<double, _SoundStream*>> m_GarbageStreams;
		//std::vector<std::pair<double, Effect*>> m_GarbageEffects;
		double m_CullTime = 10.0; // Must be larger than 1000.0f / LOST_AUDIO_SAMPLE_RATE
	};

	AudioHandler _audioHandler;

	PlaybackSound::PlaybackSound(_Sound* soundPlaying, float volume, float panning, unsigned int loopCount)
		: a_Playing{ true }
		, a_Paused{ false }
		, a_Volume{ volume }
		, a_Panning{ fmaxf(fminf(panning, 1.0f), -1.0f) }
	{
		// Bytes per channel's samples
		unsigned int soundFormat = soundPlaying->_getSoundInfo().format;
		// The bit selected is the amount of bytes per channel sample
		unsigned int audioFormat = _audioHandler.getAudioFormat();

		m_FormatFactor = soundFormat - (log2(audioFormat) + 1);

		a_PlaybackData.currentByte = 0;
		a_PlaybackData.dataCount = soundPlaying->getDataSize();
		a_PlaybackData.bytesPerSample = soundPlaying->_getSoundInfo().sampleSize / soundPlaying->_getSoundInfo().channelCount;
		a_PlaybackData.data = soundPlaying->getData();
		a_PlaybackData.formatFactor = m_FormatFactor;
		a_PlaybackData.format = soundPlaying->_getSoundInfo().format;
		a_PlaybackData.channelCount = soundPlaying->_getSoundInfo().channelCount;
		a_PlaybackData.loopCount = loopCount;

		m_ParentSound = soundPlaying;
	}

	float audioSampleToFloat(AudioSample sample)
	{
#if defined(LOST_AUDIO_QUALITY_HIGH)
		return static_cast<float>(sample) / 2147483648.0f; // 2^31
#elif defined(LOST_AUDIO_QUALITY_LOW)
		return static_cast<float>(sample) / 128.0f; // 2^7
#else // LOST_AUDIO_QUALITY_MEDIUM
		return static_cast<float>(sample) / 32768.0f; // 2^15
#endif
	}

	AudioSample floatToAudioSample(float sample)
	{
#if defined(LOST_AUDIO_QUALITY_HIGH)
		return static_cast<int32_t>(sample * 2147483648.0f); // 2^31
#elif defined(LOST_AUDIO_QUALITY_LOW)
		return static_cast<int8_t>(sample * 128.0f); // 2^7
#else // LOST_AUDIO_QUALITY_MEDIUM
		return static_cast<int16_t>(sample * 32768.0f); // 2^15
#endif
	}

	static float PCM32ToFloat(int32_t val) { return static_cast<float>(val) / 2147483648.0f; } // 2^31 
	static float PCM16ToFloat(int16_t val) { return static_cast<float>(val) / 32768.0f; } // 2^15
	static float PCM8ToFloat(int8_t val)   { return static_cast<float>(val) / 128.0f; } // 2^7

	static int32_t floatToPCM32(float val) { return static_cast<int32_t>(val * 2147483648.0f); } // 2^31
	static int16_t floatToPCM16(float val) { return static_cast<int16_t>(val * 32768.0f); } // 2^15
	static int8_t  floatToPCM8(float val)  { return static_cast<int8_t>(val * 128.0f); } // 2^7

	void initAudio()
	{
		_audioHandler.init();
		_initAudioRMs();
	}

	void exitAudio()
	{
		_audioHandler.exit();
		_destroyAudioRMs();
	}

	void updateAudio()
	{
		_recalcDeltaTime();
		_audioHandler.update(_getDeltaTime());
	}

	unsigned int _getAudioHandlerFormat()
	{
		return _audioHandler.getAudioFormat();
	}

	unsigned int _getAudioHandlerBufferSize()
	{
		return _audioHandler.getBufferFrameCount();
	}

	void setMasterVolume(float volume)
	{
		_audioHandler.setMasterVolume(fmaxf(volume, 0.0f));
	}

	float getMasterVolume()
	{
		return _audioHandler.getMasterVolume();
	}

	PlaybackSound* playSound(Sound sound, float volume, float panning, unsigned int loopCount)
	{
		if (sound->isFunctional())
			return _audioHandler.playSound(sound, volume, panning, loopCount);
		return nullptr;
	}

	void stopSound(const PlaybackSound* sound)
	{
		return _audioHandler.stopSound(sound);
	}

	void stopSound(Sound sound)
	{
		return _audioHandler.stopSounds(sound);
	}

	void setSoundPaused(PlaybackSound* sound, bool paused)
	{
		sound->_setPaused(paused);
	}

	void setSoundVolume(PlaybackSound* sound, float volume)
	{
		sound->_setVolume(volume);
	}

	void setSoundPanning(PlaybackSound* sound, float panning)
	{
		sound->_setPanning(panning);
	}

	bool isSoundPlaying(PlaybackSound* sound)
	{
		if (!_audioHandler.hasSound(sound))
			return false;
		return sound->isPlaying();
	}

	void playSoundStream(SoundStream soundStream, float volume, float panning, unsigned int loopCount)
	{
		// Check if it's already being played
		if (!soundStream->getActive() && soundStream->isFunctional())
		{
			_audioHandler.playSoundStream(soundStream, volume, panning, loopCount);
		}
	}

	void stopSoundStream(SoundStream soundStream)
	{
		_audioHandler.stopSoundStream(soundStream);
	}

	void setSoundStreamPaused(SoundStream soundStream, bool paused)
	{
		soundStream->_setPaused(paused);
	}

	bool isSoundStreamPlaying(SoundStream sound)
	{
		if (!_audioHandler.hasSoundStream(sound))
			return false;
		return sound->isPlaying();
	}

	void addGlobalEffect(Effect* effect)
	{
		_audioHandler.addGlobalEffect(effect);
	}

	void removeGlobalEffect(Effect* effect)
	{
		_audioHandler.removeGlobalEffect(effect);
	}

	void setSoundStreamVolume(SoundStream sound, float volume)
	{
		sound->_setVolume(volume);
	}

	void setSoundStreamPanning(SoundStream sound, float panning)
	{
		sound->_setPanning(panning);
	}
}