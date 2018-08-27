#include "SDLAudio.h"

#if TARGET_OS_IOS
#include "SDL_main.h"
#endif

#pragma comment(lib, "SDL2.lib")
#pragma comment(lib, "SDL2_mixer.lib")
#pragma comment(lib, "SDL2main.lib")


#define SDL_LOG(msg) printf("SDL:%s\n", #msg);
#define SDL_ERR_LOG() printf("SDL_ERROR:%s\n", Mix_GetError());

namespace SDL {
	SDLAudio::SDLAudio():
		_inited(false),
		_Music(nullptr), 
		_thread_pool(nullptr),
		_channelNums(MIX_DEFAULT_CHANNELS)
	{
		init();
	}


	SDLAudio::~SDLAudio()
	{
		uninit();
	}

	void SDLAudio::init()
	{
		if (_inited)
		{
			return;
		}
#if TARGET_OS_IOS
		SDL_SetMainReady();
#endif
		const int    TMP_FREQ = MIX_DEFAULT_FREQUENCY;
		const Uint16 TMP_FORMAT = MIX_DEFAULT_FORMAT;
		const int    TMP_CHAN = MIX_DEFAULT_CHANNELS;
		const int    TMP_CHUNK_SIZE = 1024;

		int ret = Mix_OpenAudio(TMP_FREQ, TMP_FORMAT, TMP_CHAN, TMP_CHUNK_SIZE);
		if (ret != 0)
		{
			SDL_ERR_LOG();
			return;
		}

		_inited = true;
	}

	void SDLAudio::uninit()
	{
		releaseThreadPool();
		unloadAllSound();
		unloadMusic();
		Mix_CloseAudio();
	}

	void SDLAudio::releaseThreadPool()
	{
		if (_thread_pool != nullptr)
		{
			delete _thread_pool;
			_thread_pool = nullptr;
		}
	}

	void SDLAudio::initthread()
	{
		if (_thread_pool == nullptr)
		{
			_thread_pool = new SDLThreadPool();
		}
	}

	void SDLAudio::allocateChannels(int nums)
	{
		_channelNums = nums;
		Mix_AllocateChannels(nums);
	}

	bool SDLAudio::loadSound(const std::string &filePath)
	{
		Mix_Chunk*  l_sound = Mix_LoadWAV(filePath.c_str());
		if (!l_sound)
		{
			SDL_ERR_LOG();
			return false;
		}
		
		_SoundMap.insert(std::pair<std::string, Mix_Chunk*>(filePath, l_sound));

		return true;
	}

	bool SDLAudio::loadSound(const std::string &filename, void *data, unsigned int size)
	{
		Mix_Chunk*  l_sound;
		if (size>0)
		{
			SDL_RWops *rw = SDL_RWFromConstMem(data, size);
			l_sound = Mix_LoadWAV_RW(rw, 1);
		}
		else
		{
			l_sound = Mix_QuickLoad_WAV((Uint8 *)data);
		}
		
		if (!l_sound)
		{
			SDL_ERR_LOG();
			return false;
		}

		_SoundMap.insert(std::pair<std::string, Mix_Chunk*>(filename, l_sound));

		return true;
	}

	void SDLAudio::loadSoundAsynchronous(const std::string& filePath, std::function<void(bool)> callback)
	{
		initthread();
		std::string *fn = new std::string(filePath);
		_thread_pool->addTask([this, fn, callback]() {
			bool ret = loadSound(*fn);
			delete fn;
			callback(ret);
		});
	}

	void SDLAudio::loadSoundAsynchronous(const std::string &filename, void *data, unsigned int size, std::function<void(bool)> callback)
	{
		initthread();
		std::string *fn = new std::string(filename);
		_thread_pool->addTask([this, fn, data, size, callback]() {
			bool ret = loadSound(*fn, data, size);
			delete fn;
			callback(ret);
		});
	}

	void SDLAudio::playSound(const std::string &filename, void *data /* = nullptr */, int channel /* = -1 */, bool loop /* = false */)
	{
		Mix_Chunk* l_sound;
		std::unordered_map<std::string, Mix_Chunk*>::iterator got = _SoundMap.find(filename);
		if (got == _SoundMap.end())
		{
			if (data != nullptr)
			{
				loadSound(filename, data);
			}
			else
			{
				loadSound(filename);
			}
			got = _SoundMap.find(filename);

		}
		
		l_sound = got->second;

		int chan = Mix_PlayChannel(channel, l_sound, loop ? -1 : 0);
		if (chan == -1)
		{
			SDL_ERR_LOG();
		}
	}

	AUDIOSTATE SDLAudio::getSoundState(int channel)
	{
		if (Mix_Paused(channel)!=0)
		{
			return PAUSED;
		}
		if (Mix_Playing(channel)!=0)
		{
			return PLAYING;
		}
		return STOPPED;
	}

	void SDLAudio::pauseSound(int channel)
	{
		Mix_Pause(channel);
	}

	void SDLAudio::resumeSound(int channel)
	{
		Mix_Resume(channel);
	}

	void SDLAudio::stopSound(int channel/* =-1 */)
	{
		Mix_HaltChannel(channel);
	}

	void SDLAudio::unloadSound(const std::string &filename)
	{
		std::unordered_map<std::string, Mix_Chunk*>::iterator got = _SoundMap.find(filename);
		if (got != _SoundMap.end())
		{
			Mix_FreeChunk(got->second);
			_SoundMap.erase(filename);
		}
	}

	void SDLAudio::unloadAllSound()
	{
		std::unordered_map<std::string, Mix_Chunk*>::iterator iter;
		for (iter = _SoundMap.begin(); iter !=_SoundMap.end(); iter++)
		{
			Mix_FreeChunk(iter->second);
		}
		_SoundMap.clear();
	}

	void SDLAudio::setSoundVolume(int volume, int channel)
	{
		Mix_Volume(channel, volume);
	}

	void SDLAudio::setSoundVolume(int volume, const std::string &filename)
	{
		std::unordered_map<std::string, Mix_Chunk*>::iterator got = _SoundMap.find(filename);
		if (got != _SoundMap.end())
		{
			Mix_VolumeChunk(got->second, volume);
		}
	}

	void SDLAudio::channelFinishedCallback(channleCallback cb)
	{
		Mix_ChannelFinished(cb);
	}

	bool SDLAudio::loadMusic(const std::string &filePath)
	{
		if (_Music != nullptr)
		{
			unloadMusic();
		}
		_Music = Mix_LoadMUS(filePath.c_str());
		if (_Music == 0)
		{
			SDL_ERR_LOG();
			return false;
		}
		return true;
	}

	bool SDLAudio::loadMusic(const void *data, unsigned int size)
	{
		SDL_RWops *rwops = SDL_RWFromConstMem(data, size);
		_Music = Mix_LoadMUS_RW(rwops, 1);
		if (_Music == 0)
		{
			SDL_ERR_LOG();
			return false;
		}
		return true;
	}

	void SDLAudio::stopMusic()
	{
		if (_Music != nullptr)
		{
			Mix_HaltMusic();
		}
	}

	void SDLAudio::unloadMusic()
	{
		if (_Music != nullptr)
		{
			stopMusic();
			Mix_FreeMusic(_Music);
			_Music = nullptr;
		}
	}

	void SDLAudio::resumeMusic()
	{
		Mix_ResumeMusic();
	}

	void SDLAudio::pauseMusic()
	{
		Mix_PauseMusic();
	}

	void SDLAudio::rewineMusic()
	{
		Mix_RewindMusic();
	}

	AUDIOSTATE SDLAudio::getMusicState()
	{
		if (Mix_PausedMusic() != 0)
		{
			return PAUSED;
		}
		if (Mix_PlayingMusic() != 0)
		{
			return PLAYING;
		}
		return STOPPED;
	}

	void SDLAudio::setMusicVolume(int volume)
	{
		Mix_VolumeMusic(volume);
	}

	void SDLAudio::playMusic(bool loop)
	{
		playMusic(loop ? -1 : 0);
	}

	void SDLAudio::playMusic(int times)
	{
		Mix_PlayMusic(_Music, times);
	}

	void SDLAudio::loadMusicAsynchronous(const std::string& filePath, std::function<void(bool)> callback)
	{
		initthread();
		std::string *fn = new std::string(filePath);
		_thread_pool->addTask([this, fn, callback]() {
			bool ret = loadMusic(*fn);
			delete fn;
			callback(ret);
		});
	}

	void SDLAudio::loadMusicAsynchronous(const void *data, unsigned int size, std::function<void(bool)> callback)
	{
		initthread();
		_thread_pool->addTask([this, data, size, callback]() {
			callback(loadMusic(data, size));
		});
	}
}