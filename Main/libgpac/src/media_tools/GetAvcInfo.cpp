#include <GetAvcInfo.h>
#include <internal\media_dev.h>

// 0 组合信号
// 1 PAL
// 2 NTSC
// 3 SECAM
// 4 MAC
// 5 未定义的视频制式
// 6 保留
// 7 保留

float video_format_tab[] = {
	400000.0f,
	400000.0f,
	10000000.0f/29.97f,
	400000.0f,
	400000.0f,
	400000.0f,
	400000.0f,
	400000.0f
};

int GetAvcInfo(unsigned char *data, int size, CommonVideoInfo* info)
{
	AVCState avc;
	memset(&avc, 0, sizeof(AVCState));

	s32 idx = 0;
	GF_BitStream *bs = gf_bs_new((char*)data, size, GF_BITSTREAM_READ);

	idx = AVC_ReadSeqInfo(bs, &avc, NULL);

	gf_bs_del(bs);

	info->width = avc.sps[idx].width;
	info->height = avc.sps[idx].height;
	info->avg_bitrate = avc.sps[idx].bit_rate_value_minus1[0];

	if( avc.sps[idx].video_format < 0 )
		avc.sps[idx].video_format = 0;
	if( avc.sps[idx].video_format > 7 )
		avc.sps[idx].video_format = 7;

	if( avc.sps[idx].video_format == 0 )
		avc.sps[idx].video_format = 5;

	info->avg_time_per_frame = (int)(video_format_tab[avc.sps[idx].video_format]);
	info->pictAspRatioX = avc.sps[idx].par_num;
	info->pictAspRatioY = avc.sps[idx].par_den;

	return true;
}
