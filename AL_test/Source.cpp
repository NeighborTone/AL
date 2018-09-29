#include <iostream>
#include <conio.h>
#include "wav.h"
#include "Audio.hpp"
#include <vector>

int main()
{
	

	using namespace std;
	WAVE          wav1;
	WAVE_FORMAT   fmt1;
	vector<short> ch1, ch2;

	//ファイルから読み込み
	wav1.load_from_file("lastcastle_16bit.wav");

	//フォーマット情報とデータを取り出す
	fmt1 = wav1.get_format();
	wav1.get_channel(ch1, 0);

	//フォーマット情報表示
	cout << endl;
	cout << "format id       = " << fmt1.format_id << endl;
	cout << "channels        = " << fmt1.num_of_channels << "\t[Ch]" << endl;
	cout << "sampling rate   = " << fmt1.samples_per_sec << "\t[Hz]" << endl;
	cout << "bytes per sec   = " << fmt1.bytes_per_sec << "\t[bytes/sec]" << endl;
	cout << "block size      = " << fmt1.block_size << "\t[bytes]" << endl;
	cout << "bits per sample = " << fmt1.bits_per_sample << "\t[bits/sample]" << endl;

	
	cout << endl;

	SoundClass Soundsystem;
	Soundsystem.CreateSource("test", "lastcastle_16bit.wav", SoundSource::LoadMode::Streaming);
	
	
	while (1)
	{
		switch (_getch())
		{
		case 'z':
			Soundsystem.GetSource("test")->Play(true);
			break;
		case 'x':
			Soundsystem.GetSource("test")->Pause();
			break;
		}

	}

	system("pause");
}