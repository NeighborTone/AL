//---------------------------------------------------------------------------
#ifndef     __WAVE_H
#define     __WAVE_H

//---------------------------------------------------------------------------
#include <vector>

//---------------------------------------------------------------------------
struct WAVE_FORMAT
{
	unsigned short format_id;           //フォーマットID
	unsigned short num_of_channels;     //チャンネル数 monaural=1 , stereo=2
	unsigned long  samples_per_sec;     //１秒間のサンプル数，サンプリングレート(Hz)
	unsigned long  bytes_per_sec;       //１秒間のデータサイズ
	unsigned short block_size;          //１ブロックのサイズ．8bit:nomaural=1byte , 16bit:stereo=4byte
	unsigned short bits_per_sample;     //１サンプルのビット数 8bit or 16bit
};
//---------------------------------------------------------------------------
class WAVE
{
private:
	//一応コピー禁止
	WAVE(const WAVE&) = delete;
	WAVE& operator=(const WAVE&) = delete;

	WAVE_FORMAT fmt;                    //フォーマット情報
	std::vector<unsigned char> data;    //音データ

	unsigned long sampling_size;        //サンプリング信号サイズ(bytes)
	unsigned long data_size;            //実効データサイズ

public:
	WAVE();
	WAVE(char *file_name);
	~WAVE();

	bool load_from_file(const char *file_name);
	bool save_to_file(const char *file_name);

	void get_channel(std::vector<short> &buf, unsigned short ch);

	void set_channel(std::vector<short> &_ch0, WAVE_FORMAT _fmt);
	void set_channel(std::vector<short> &_ch0, std::vector<short> &_ch1, WAVE_FORMAT _fmt);

	WAVE_FORMAT    get_format() { return fmt; }
	unsigned long  size() { return data_size; }
	unsigned short channels() { return fmt.num_of_channels; }
	unsigned long  sampling_rate() { return fmt.samples_per_sec; }

	double sec() { return data_size / (double)fmt.samples_per_sec; }
};
//---------------------------------------------------------------------------
#endif