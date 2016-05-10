
typedef struct CommonVideoInfo_s
{
	int width;
	int height;
	int avg_bitrate;
	int avg_time_per_frame;
	int pictAspRatioX;
	int pictAspRatioY;
}CommonVideoInfo;

#ifdef __cplusplus
extern "C"{
#endif

int GetAvcInfo(unsigned char *data, int size, CommonVideoInfo* info);

#ifdef __cplusplus
};
#endif
