#include <iostream>
#include <conio.h>
#include <alc.h>
#include <al.h>
#pragma comment(lib,"OpenAL32.lib")

static ALuint sid;
[[nodiscard]] bool audioInit()
{
	ALCdevice* device = alcOpenDevice(NULL);
	if (device == NULL) { return false; }

	ALCcontext* context =  
		alcCreateContext(
		device,	 // ALCdevice *device, 
		NULL	 //const ALCint* attrlist
	);
	if (context == NULL) { return false; }

	alcMakeContextCurrent(context);
	ALuint bid;

	alGenBuffers(
		1,		//ALsizei n, 
		&bid	//ALuint* buffers
	);

	//矩形波
	unsigned char data[] = {0xff, 0x00, 0x00 ,0x00 ,0x00 ,0x00 ,0x00 ,0x00 };
	//矩形波をセット
	alBufferData(
		bid,				//ALuint bid, 
		AL_FORMAT_MONO8,	//ALenum format,
		data,				//const ALvoid* data,
		sizeof(data),		//ALsizei size, 
		sizeof(data) * 441	//ALsizei freq
	);

	//Sourceボイスの作成
	
	alGenSources(
		1,		//ALsizei n,
		&sid	//ALuint* sources
	);
	//Sourceにバッファーをセット
	alSourcei(
		sid,		//ALuint sid, 
		AL_BUFFER,	//ALenum param, 
		bid			//ALint value
	);

	//ループ再生	
	alSourcei(
		sid,			//ALuint sid, 
		AL_LOOPING,		//ALenum param, 
		AL_TRUE			//ALint value
	);
	//ボリューム
	alSourcef(
		sid,
		AL_GAIN,
		0.1f
	);
	return true;
}

void audioPlay()
{
	alSourcePlay(sid);
}

void audioStop()
{
	alSourceStop(sid);
}
int main()
{
	if (!audioInit())
	{
		return 0;
	}
	while (1)
	{
		switch (_getch())
		{
		case 'z':
			audioPlay();
			break;
		case 'x':
			audioStop();
			break;
		}
	
	}
}