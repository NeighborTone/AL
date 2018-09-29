#pragma once
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <string>
#include <conio.h>
#include <alc.h>
#include <al.h>
#pragma comment(lib,"OpenAL32.lib")

enum class SoundFormat
{
	Mono8,
	Mono16,
	Stereo8,
	Stereo16,
};
struct AudioData
{
	int pcmSize;
	int pcmOffset;
	int loopStart;
	int loopLength;
	SoundFormat format;
	int samplingRate;
	int blockSize;
	int dataStartOffset;
};
class IAudioData
{
public:
	IAudioData() = default;
	virtual ~IAudioData() = default;
	virtual bool LoadFile(const char* filePass) = 0;
	virtual int Read(char* buffer, int maxSize) = 0;
	virtual void Seek(int pcmOffset) = 0;
	virtual int GetPcmOffset() const = 0;
	virtual int GetLoopStart() const = 0;
	virtual int GetLoopLength() const = 0;
	virtual int GetPcmSize() const = 0;
	virtual int GetBlockSize() const = 0;
	virtual SoundFormat GetFormat() const = 0;
	virtual int GetSamplingRate() const = 0;
};
//Waveの読み込みとデータ保持を担当する
class WavData : public IAudioData 
{
private:
	AudioData data_;
	std::ifstream waveFile_;
public:
	WavData(const char* filePass)
	{
		if (!LoadFile(filePass))
		{
			throw("not wav" + std::string(filePass));
		}
	}
	~WavData()
	{
		waveFile_.close();
	}

	void Seek(int offset)
	{
		//ファイルの先頭からPCMまでの位置まで移動させたあとに (offset * blockSize) バイト分移動
		PcmSeek(offset);
		data_.pcmOffset = offset;

		//終端を超えたら終端位置にシーク
		if (data_.loopLength < data_.pcmOffset)
		{
			data_.pcmOffset = data_.loopLength;
			PcmSeek(data_.loopLength);
		}
	}
	int Read(char* buffer, int maxSize)
	{
		//読み込みサイズがPCMの一つ分サイズで割り切れないなら失敗
		if (maxSize % data_.blockSize != 0)
		{
			return 0;
		}
		int readSize = maxSize;
		//終端を超えないようにサイズ調整
		if (data_.loopStart + data_.loopLength < data_.pcmOffset + readSize / data_.blockSize)
		{
			readSize = (data_.loopLength - data_.pcmOffset) * data_.blockSize;
		}

		if (readSize == 0) 
		{
			return 0;
		}

		int prevOffset = data_.pcmOffset;
		waveFile_.read(buffer, readSize);
		data_.pcmOffset += readSize / data_.blockSize;

		//実際に読み込んだサイズを求めて返す
		return (data_.pcmOffset - prevOffset) * data_.blockSize;
	}

	int GetPcmOffset() const override
	{
		return data_.pcmOffset;
	}

	int GetLoopStart() const override
	{
		return data_.loopStart;
	}

	int GetLoopLength() const override
	{
		return data_.loopLength;
	}

	int GetPcmSize() const override
	{
		return data_.pcmSize;
	}

	int GetBlockSize() const override
	{
		return data_.blockSize;
	}

	SoundFormat GetFormat() const override
	{
		return data_.format;

	}

	int GetSamplingRate() const override
	{
		return data_.samplingRate;
	}
private:
	bool LoadFile(const char* filePass)
	{
		//各チャンクの先頭にこの情報が必ずある
		struct WaveChunk {
			char id[4];
			int size;
		};
		//fmtチャンク用の構造体
		struct WaveStruct {
			unsigned short formatId;           //フォーマットID
			unsigned short numChannel;			//チャンネル数 monaural=1 , stereo=2
			unsigned long  samplingRate;	 //１秒間のサンプル数，サンプリングレート(Hz)
			unsigned long  bytesPerSec;       //１秒間のデータサイズ
			unsigned short blockSize;          //１ブロックのサイズ．8bit:nomaural=1byte , 16bit:stereo=4byte
			unsigned short bitsPerSample;     //１サンプルのビット数 8bit or 16bit
			WaveStruct():
			formatId(0),
			numChannel(0),
			samplingRate(0),
			bytesPerSec(0),
			blockSize(0),
			bitsPerSample(0)
			{}
		};


		waveFile_.open(filePass, std::ifstream::binary);
		if (!waveFile_) {
			return false;
		}
		//RIFFチャンクの先頭12バイト
		//id = 4bite : size = 4bite;
		WaveChunk chunk;
		WaveStruct fmtChunk;
		char format[4];

		waveFile_.read((char*)&chunk, 8);
		waveFile_.read(format, 4);

		//WAVEフォーマット以外は失敗
		if (strncmp(chunk.id, "RIFF", 4) != 0 || strncmp(format, "WAVE", 4)) {
			return false;
		}

		//各チャンクを読む
		int count = 0;
		int byteOffset = 12;
		int dataSize = 0;
		int fileSize = chunk.size;
		while (byteOffset < fileSize) {
			waveFile_.read((char*)&chunk, sizeof(WaveChunk));
			if (strncmp(chunk.id, "fmt ", 4) == 0) {
				//fmtチャンク
				waveFile_.read((char*)&fmtChunk, chunk.size);
				if (fmtChunk.formatId != 1) {
					return false;
				}
				byteOffset += chunk.size + sizeof(WaveChunk);
				++count;
			}
			else if (strncmp(chunk.id, "data ", 4) == 0) {
				//dataチャンク
				//位置を保存して逐次読み込み用のデータに利用
				data_.dataStartOffset = byteOffset + sizeof(WaveChunk);
				waveFile_.seekg(chunk.size, std::ios_base::cur);
				byteOffset += chunk.size + sizeof(WaveChunk);
				dataSize = chunk.size;
				++count;
			}
			else {
				//それ以外のチャンクはスキップ
				waveFile_.seekg(chunk.size, std::ios_base::cur);
				byteOffset += chunk.size + sizeof(WaveChunk);
			}
		}

		//fmt,dataのチャンク合わせて２つでない時は失敗
		if (count != 2) {
			return false;
		}

		data_.samplingRate = fmtChunk.samplingRate;
		if (fmtChunk.numChannel == 1) {
			//モノラル
			if (fmtChunk.bitsPerSample == 8) {
				data_.format = SoundFormat::Mono8;
			};
			if (fmtChunk.bitsPerSample == 16) {
				data_.format = SoundFormat::Mono16;
			};
		}
		else {
			//ステレオ
			if (fmtChunk.bitsPerSample == 8) {
				data_.format = SoundFormat::Stereo8;
			};
			if (fmtChunk.bitsPerSample == 16) {
				data_.format = SoundFormat::Stereo16;
			};
		}

		data_.blockSize = fmtChunk.blockSize;

		//oggと違ってこっちはループポイントが最初と最後で固定
		data_.loopStart = 0;
		data_.loopLength = data_.loopStart + dataSize / data_.blockSize;

		Seek(0);
		return true;
	}
	void PcmSeek(int pcmOffset)
	{
		waveFile_.seekg(data_.dataStartOffset + pcmOffset * data_.blockSize, std::ios_base::beg);
	}
};

//指定されたファイルパスの拡張子を用いて適切にAudioLoaderを生成するファクトリー
class AudioDataFactory {
public:
	enum AudioType {
		Wave,
		Ogg,
		NonSupport,
	};

public:
	AudioDataFactory() = default;
	~AudioDataFactory() = default;
	IAudioData* Create(const char* filePass)
	{
		IAudioData* data=nullptr;

		switch (CheckType(filePass))
		{
		case AudioType::Wave:
			data = new WavData(filePass);
			break;
		case AudioType::Ogg:
		
			break;

		case AudioType::NonSupport:
		default:
			data = nullptr;
		}

		return data;
	}
private:
	AudioType CheckType(const char* filePass)
	{
		//拡張子は初めの「.」から終端までの文字列とする
		int i = 0;
		char ext[16] = {};
		while (filePass[i] != '.') {
			++i;
		}
		//ドットは飛ばす
		++i;

		//終端までコピー
		int extCount = 0;
		while (filePass[i] != 0 && extCount < 15) {
			ext[extCount] = filePass[i];
			++extCount;
			++i;
		}
		ext[extCount] = 0;

		AudioType type = AudioType::NonSupport;
		if (strcmp(ext, "ogg") == 0) {
			type = AudioType::Ogg;
		}
		if (strcmp(ext, "wav") == 0) {
			type = AudioType::Wave;
		}

		return type;
	}
};
class SoundSource
{
public:
	enum LoadMode {
		Streaming,
		AllRead,
	};
private:
	const std::string name_;
	IAudioData* audio_;
	LoadMode mode_;
	std::vector<char> allReadData_;
	//CopyPlay用の管理リスト
	std::list<ALuint> copySources_;

	std::thread* thread_;
	std::recursive_mutex mutex_;

	ALuint sourceID_;
	ALuint *bufferIDs_;
	int numBuffer_;

	ALuint format_;

	float volume_;
	float posX_, posY_, posZ_;
	float velocityX_, velocityY_, velocityZ_;

	bool isPlayed_;
	bool isLoop_;
	bool isEnd_;
public:
	SoundSource(const char* filePass, LoadMode mode, int numBuffer = 32)
	{
		bufferIDs_ = new ALuint[numBuffer];
		isPlayed_ = false;
		isLoop_ = false;
		isEnd_ = false;
		numBuffer = numBuffer;
		mode = mode;


		alGenSources(1, &sourceID_);
		alDistanceModel(AL_EXPONENT_DISTANCE);

		//読み込み
		AudioDataFactory factory;
		audio_ = factory.Create(filePass);

		if (audio_->GetFormat() == SoundFormat::Stereo16) {
			format_ = AL_FORMAT_STEREO16;
		}
		else {
			format_ = AL_FORMAT_MONO16;
		}
		//バッファ生成＆事前読み込み
		switch (mode) {
		case LoadMode::Streaming:
			//ストリーミングモードはバッファを作ってキューする
			char buffer[4096];
			for (int i = 0; i < numBuffer; ++i) {
				alGenBuffers(1, &bufferIDs_[i]);
				int readSize = ReadBuffer(buffer, 4096);
				alBufferData(bufferIDs_[i], format_, buffer, readSize, audio_->GetSamplingRate());
				alSourceQueueBuffers(sourceID_, 1, &bufferIDs_[i]);
			}
			//スレッド開始 (スレッドにメンバー関数を指定する際は第二引数にthisポインターを指定する)
			thread_ = new std::thread(&SoundSource::StreamingThread, this);
			break;

		case LoadMode::AllRead:
			//オールリードモードはすべて読んでバッファに突っ込む
			int size = audio_->GetLoopLength() * audio_->GetBlockSize();
			allReadData_.resize(size);
			int readSize = ReadBuffer(allReadData_.data(), size);
			alGenBuffers(1, &bufferIDs_[0]);
			alBufferData(bufferIDs_[0], format_, allReadData_.data(), readSize, audio_->GetSamplingRate());
			alSourcei(sourceID_, AL_BUFFER, bufferIDs_[0]);
			numBuffer = 1;
			thread_ = new std::thread(&SoundSource::AllReadThread, this);
			break;
		}
	}
	~SoundSource()
	{
		EndThread();

		if (thread_ != nullptr)
		{
			thread_->join();
			delete thread_;
			thread_ = nullptr;
		}
		delete audio_;

		alDeleteBuffers(numBuffer_, bufferIDs_);
		alDeleteSources(1, &sourceID_);

		delete[] bufferIDs_;
	}
	void Play(bool loop)
	{
		ALint state;
		alGetSourcei(sourceID_, AL_SOURCE_STATE, &state);
		if (state == AL_PLAYING)
		{
			return;
		}
		
		
		alSourcePlay(sourceID_);
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		isPlayed_ = true;
		isLoop_ = loop;
	}
	void PlayCopy()
	{
		//ストリーミングモードでは利用できない
		if (mode_ == LoadMode::Streaming) 
		{
			return;
		}
		ALuint source;
		{
			std::lock_guard<std::recursive_mutex> lock(mutex_);
			isPlayed_ = true;
			isLoop_ = false;
			alGenSources(1, &source);
			alSourcei(source, AL_BUFFER, bufferIDs_[0]);
			copySources_.push_back(source);
		}
		alSourcePlay(source);
	}
	void Pause()
	{
		alSourceStop(sourceID_);
		{
			std::lock_guard<std::recursive_mutex> lock(mutex_);
			isPlayed_ = false;
		}
	}

	void Stop()
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		isPlayed_ = false;
		//バッファを初期化
		int num;
		ALuint soundBuffer;
		while (alGetSourcei(sourceID_, AL_BUFFERS_PROCESSED, &num), num > 0) {
			alSourceUnqueueBuffers(sourceID_, 1, &soundBuffer);
		}

		alSourceStop(sourceID_);

		audio_->Seek(0);
		char buffer[4096];
		for (int i = 0; i < numBuffer_; ++i) 
		{
			int readSize = ReadBuffer(buffer, 4096);
			alBufferData(bufferIDs_[i], format_, buffer, readSize, audio_->GetSamplingRate());
			alSourceQueueBuffers(sourceID_, 1, &bufferIDs_[i]);
		}
	}
	void SetVolume(float volume)
	{
		alSourcef(sourceID_, AL_MAX_GAIN, volume);
		{
			std::lock_guard<std::recursive_mutex> lock(mutex_);
			volume_ = volume;
		}
	}
	void SetPosition(float x, float y, float z)
	{
		alSource3f(sourceID_, AL_POSITION, x, y, z);
		{
			std::lock_guard<std::recursive_mutex> lock(mutex_);
			posX_ = x;
			posY_ = y;
			posZ_ = z;
		}
	}
	void SetVelocity(float x, float y, float z)
	{
		alSource3f(sourceID_, AL_VELOCITY, x, y, z);
		{
			std::lock_guard<std::recursive_mutex> lock(mutex_);
			velocityX_ = x;
			velocityY_ = y;
			velocityZ_ = z;
		}
	}
	bool IsPlay()
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		return isPlayed_;
	}

private:
	void StreamingThread()
	{
		while (!isEnd_) {
			if (!isPlayed_) {
				//スリープはだいたい1フレーム分のウェイト
				std::this_thread::sleep_for(std::chrono::milliseconds(16));
				continue;
			}

			ALint state;
			alGetSourcei(sourceID_, AL_SOURCE_STATE, &state);

			if (state != AL_PLAYING) {
				alSourcePlay(sourceID_);
			}
			else {
				FillBuffer();
			}
		}
	}
	void AllReadThread()
	{
		//再生終了したら削除
		while (!isEnd_) {
			//大体1秒に一回でも行けそうではあるが、とりあえず60FPS相当のウェイト
			std::this_thread::sleep_for(std::chrono::milliseconds(16));

			std::lock_guard<std::recursive_mutex> lock(mutex_);
			for (auto it = copySources_.begin(); it != copySources_.end();)
			{
				ALint state;
				alGetSourcei((*it), AL_SOURCE_STATE, &state);

				if (state != AL_PLAYING)
				{
					alDeleteSources(1, &(*it));
					it = copySources_.erase(it);
					continue;
				}
				++it;
			}
		}

		for (auto i : copySources_) {
			alDeleteSources(1, &i);
		}
	}
	void EndThread()
	{
		alSourceStop(sourceID_);
		alSourcei(sourceID_, AL_BUFFER, AL_NONE);

		std::lock_guard<std::recursive_mutex> lock(mutex_);
		isEnd_ = true;
		isPlayed_ = false;
	}
	void FillBuffer()
	{
		int size = 0;

		char buffer[4096];
		int fillSize = 4096;
		int offset = audio_->GetPcmOffset();
		int loopEnd = audio_->GetLoopStart() + audio_->GetLoopLength();

		//4096バイトよりループ終端までの距離が短いときはそっちを読み込み量とする
		if (fillSize > (loopEnd - offset) * audio_->GetBlockSize()) {
			fillSize = (loopEnd - offset) * audio_->GetBlockSize();
		}

		//処理済みキューがない場合はそのまま帰る予定
		int numProcessed = 0;
		while (isPlayed_) {
			alGetSourcei(sourceID_, AL_BUFFERS_PROCESSED, &numProcessed);
			if (numProcessed == 0) 
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(600));
				continue;
			}

			ALuint soundBuffer;
			alSourceUnqueueBuffers(sourceID_, 1, &soundBuffer);

			int readSize = ReadBuffer(buffer, fillSize - size);
			alBufferData(soundBuffer, format_, buffer, readSize, audio_->GetSamplingRate());
			alSourceQueueBuffers(sourceID_, 1, &soundBuffer);

			offset = audio_->GetPcmOffset();
			if (readSize == 0 || offset == loopEnd) {
				//ループしない場合は終端に達したらストリーミングを停止
				if (isLoop_) 
				{
					audio_->Seek(audio_->GetLoopStart());
					offset = audio_->GetLoopStart();
					//std::cout << "Loop!" << std::endl;
				}
				else
				{
					Stop();
					break;
				}
			}

			size += readSize;
			if (size == fillSize) {
				break;
			}
			alGetSourcei(sourceID_, AL_BUFFERS_PROCESSED, &numProcessed);
		}
	}
	int  ReadBuffer(char* buffer, int maxReadSize)
	{
		//readBufferが埋まるまで読みこむ
		int readSize = 0;
		while (readSize != maxReadSize) {
			char tmp[4096];
			int size = maxReadSize - readSize;
			if (size > 4096) {
				size = 4096;
			}
			int singleReadSize = audio_->Read(tmp, size);
			if (singleReadSize <= 0) {
				break;
			}
			for (int i = 0; i < singleReadSize; ++i) {
				buffer[i + readSize] = tmp[i];
			}
			readSize += singleReadSize;
		}
		if (readSize == 0) {
			return 0;
		}

		return readSize;
	}
};
class SoundClass final
{
private:
	ALCdevice* device;
	ALCcontext* context;
	std::unordered_map<std::string, SoundSource*> source;
public:
	SoundClass()
	{
		device = nullptr;
		context = nullptr;

		device = alcOpenDevice(NULL);
		if (device == NULL)
		{
			throw("OpenAL Initialize Failed : device");
		}
		context = alcCreateContext(device, NULL);
		if (context == NULL)
		{
			alcCloseDevice(device);
			throw("OpenAL Initialize Failed : context");
		}
		if (alcMakeContextCurrent(context) == ALC_FALSE) 
		{
			alcDestroyContext(context);
			alcCloseDevice(device);
			throw("OpenAL Initialize Failed : alcMakeContextCullent() Failed");
		}
	}
	~SoundClass()
	{
		alcMakeContextCurrent(NULL);
		alcDestroyContext(context);
		alcCloseDevice(device);
	}
	bool CreateSource(const char* sourceName, const char* filePass, SoundSource::LoadMode mode)
	{
		//ソース名の重複は許さない
		if (source.find(sourceName) != source.end()) {
			return false;
		}
		SoundSource* audioSource = new SoundSource(filePass, mode);
		if (!audioSource) {
			return false;
		}

		source[sourceName] = audioSource;
		return true;
	}
	void DeleteSource(const char* sourceName)
	{
		if (source.find(sourceName) == source.end()) {
			return;
		}
		delete source[sourceName];
		source.erase(sourceName);
	}
	SoundSource* GetSource(const char* sourceName)
	{
		//ソース名の存在チェック
		if (source.find(sourceName) == source.end()) {
			return nullptr;
		}
		return source[sourceName];
	}

};