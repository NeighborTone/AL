//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
#define _CRT_SECURE_NO_WARNINGS
#include "wav.h"
#include <iostream>
#include <fstream>
#include <cassert>
#include <algorithm>
#include <functional>

using namespace std;

//---------------------------------------------------------------------------
//チャンク構造体
//---------------------------------------------------------------------------
struct WAVECHUNK
{
	char ID[4];     //チャンクの種類
	int  size;      //チャンクのサイズ
};
//---------------------------------------------------------------------------
WAVE::WAVE()
{
	sampling_size = 0;
	data_size = 0;
}
//---------------------------------------------------------------------------
WAVE::WAVE(char *file_name)
{
	load_from_file(file_name);
}
//---------------------------------------------------------------------------
WAVE::~WAVE()
{

}
//---------------------------------------------------------------------------
bool WAVE::load_from_file(const char *file_name)
{
	WAVECHUNK chunk;
	char      type[4];

	ifstream ifs(file_name, ios::binary);

	data.clear();
	//sampling_size=0;
	data_size = 0;

	if (!ifs) {
		cerr << "file open error." << endl;
		return false;
	}

	ifs.read((char*)&chunk, 8);
	if (ifs.bad() || strncmp(chunk.ID, "RIFF", 4) != 0) return false;

	ifs.read((char*)type, 4);
	if (ifs.bad() || strncmp(type, "WAVE", 4) != 0) return false;

	unsigned char flg = 0;

	try {
		//例外設定?
		ifs.exceptions(ios::badbit);
		while (ifs.read((char*)&chunk, sizeof(WAVECHUNK))) {
			if (strncmp(chunk.ID, "fmt ", 4) == 0) {
				//リニアPCAにのみ対応 : sizeof(WAVE_FORMAT) = 16
				ifs.read((char*)&fmt, min(16, chunk.size));

				//リニアPCAでなかったら抜ける
				if (fmt.format_id != 1) {
					cerr << "unsupported wave file format." << endl;
					goto WAVE_FILE_ERROR;
				}
				flg++;
			}
			else if (strncmp(chunk.ID, "data", 4) == 0) {
				data.resize(chunk.size);
				ifs.read((char*)&data[0], data.size());

				flg++;
				break;
			}
			else {
				ifs.seekg(chunk.size, ios::cur);
			}
		}
	}
	catch (ios_base::failure& e) {
		cerr << e.what() << endl;
		goto WAVE_FILE_ERROR;
	}

	if (flg != 2) {
		cerr << "file format error." << endl;
		goto WAVE_FILE_ERROR;
	}

	sampling_size = fmt.bits_per_sample / 8 * fmt.num_of_channels;
	data_size = data.size() / sampling_size;

	return true;

	//エラー時の処理
WAVE_FILE_ERROR:
	data.clear();
	sampling_size = 0;
	data_size = 0;

	return false;
}
//---------------------------------------------------------------------------
bool WAVE::save_to_file(const char *file_name)
{
	ofstream ofs(file_name, ios::binary);

	if (!ofs) {
		cerr << "file open error." << endl;
		return false;
	}

	try {
		WAVECHUNK chunk;

		//例外設定?
		ofs.exceptions(ios::badbit);

		strncpy(chunk.ID, "RIFF", 4);
		chunk.size = sizeof(chunk) + 4 + sizeof(WAVE_FORMAT) + data.size();
		ofs.write((char*)&chunk, sizeof(WAVECHUNK));
		ofs.write("WAVE", 4);

		strncpy(chunk.ID, "fmt ", 4);
		chunk.size = sizeof(WAVE_FORMAT);
		ofs.write((char*)&chunk, sizeof(WAVECHUNK));
		ofs.write((char*)&fmt, 16);

		strncpy(chunk.ID, "data", 4);
		chunk.size = data.size();
		ofs.write((char*)&chunk, sizeof(WAVECHUNK));
		ofs.write((char*)&data[0], data.size());
	}
	catch (ios_base::failure& e) {
		cerr << e.what() << endl;
		return false;
	}

	return true;
}
//---------------------------------------------------------------------------
void WAVE::get_channel(vector<short> &channel, unsigned short ch)
{
	channel.resize(data_size);
	if (fmt.num_of_channels == 1) {
		if (fmt.bits_per_sample == 8) {
			//8bitならデータは unsigned char (0～255 無音は 128)なので補正
			for (auto t(0u); t < data_size; t++) channel[t] = (short(data[t]) - 0x80) << 8;
		}
		else if (fmt.bits_per_sample == 16) {
			//16bitならデータは signed short (-32768～+32767 無音は 0)
			short *ptr = (short*)&data[0];
			for (auto t(0u); t < data_size; t++) channel[t] = ptr[t];
		}
	}
	else if (fmt.num_of_channels == 2) {
		if (ch > 1) ch = 1;

		if (fmt.bits_per_sample == 8) {
			//8bitならデータは unsigned char (0～255 無音は 128)なので補正
			for (auto t(0u); t < data_size; t++) channel[t] = (short(data[2 * t + ch]) - 0x80) << 8;
		}
		else if (fmt.bits_per_sample == 16) {
			//16bitならデータは signed short (-32768～+32767 無音は 0)
			short *ptr = (short*)&data[0];
			for (auto t(0u); t < data_size; t++) channel[t] = ptr[2 * t + ch];
		}
	}
}
//---------------------------------------------------------------------------
void WAVE::set_channel(std::vector<short> &ch0, WAVE_FORMAT _fmt)
{
	fmt = _fmt;

	if (fmt.bits_per_sample != 8 && fmt.bits_per_sample != 16) fmt.bits_per_sample = 16;

	//BytesPerSample
	unsigned short BPS = fmt.bits_per_sample / 8;

	fmt.format_id = 1;
	fmt.num_of_channels = 1;
	fmt.block_size = fmt.num_of_channels*BPS;
	fmt.bytes_per_sec = fmt.samples_per_sec*fmt.block_size;

	sampling_size = BPS * fmt.num_of_channels;
	data_size = ch0.size();

	data.resize(sampling_size*ch0.size());

	if (BPS == 1) {
		for (auto t(0u); t < ch0.size(); t++) data[t] = (ch0[t] >> 8) + 0x80;
	}
	else if (BPS == 2) {
		short *ptr = (short*)&data[0];
		for (auto t(0u); t < ch0.size(); t++) ptr[t] = ch0[t];
	}
}
//---------------------------------------------------------------------------
void WAVE::set_channel(std::vector<short> &ch0, std::vector<short> &ch1, WAVE_FORMAT _fmt)
{
	fmt = _fmt;

	if (fmt.bits_per_sample != 8 && fmt.bits_per_sample != 16) fmt.bits_per_sample = 16;

	//BytesPerSample
	unsigned short BPS = fmt.bits_per_sample / 8;

	fmt.format_id = 1;
	fmt.num_of_channels = 2;
	fmt.block_size = fmt.num_of_channels*BPS;
	fmt.bytes_per_sec = fmt.samples_per_sec*fmt.block_size;

	sampling_size = BPS * fmt.num_of_channels;
	data_size = min(ch0.size(), ch1.size());

	data.resize(sampling_size*ch0.size());

	if (BPS == 1) {
		for (auto t(0u); t < data_size; t++) {
			data[2 * t] = (ch0[t] >> 8) + 0x80;
			data[2 * t + 1] = (ch1[t] >> 8) + 0x80;
		}
	}
	else if (BPS == 2) {
		short *ptr = (short*)&data[0];
		for (auto t(0u); t < data_size; t++) {
			ptr[2 * t] = ch0[t];
			ptr[2 * t + 1] = ch1[t];
		}
	}
}
//---------------------------------------------------------------------------