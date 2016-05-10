#ifndef __clip_snapshot_h__
#define __clip_snapshot_h__


#ifdef __cplusplus
extern "C"{
#endif

typedef enum _Snapshot_ErrCode
{
  ErrOk = 0,
  ErrUnknowFormat,
  ErrCannotFindStreamInfo,
  ErrNoVideoStream,
  ErrNoVideoDecoder,
  ErrVideoDecoderOpenFailed,
  ErrNoImageEncoder,
  ErrImageEncoderOpenFailed,
  ErrInvalidArgument,
  ErrUnsurpportedPictureType,
  ErrSourceFileNotExist,
  ErrDstFileExist,
  ErrDstPathAccessFail,
  ErrEncodeErr,
  ErrMuxErr,
  ErrOutOfRange,
  ErrNoScale,
  ErrUnknown,
}Snapshot_ErrCode;

typedef enum _PictureType
{
  PictureNone,
  PictureBMP,
  PictureJPG,
  PicturePNG,
  PictureTGA,
  PictureTIF
}PictureType;

#define IsValidType(_t_)  \
  ((_t_)== PictureBMP || \
  (_t_) == PictureJPG || \
  (_t_) == PicturePNG || \
  (_t_) == PictureTGA || \
  (_t_) == PictureTIF)


Snapshot_ErrCode __stdcall GetClipSnapshot(const char* src_file, //源文件路径
                                           const char* dst_file, //目标截图存储路径
                                           long offset_in_ms,           // 帧偏移量,单位 :毫秒
                                           bool override_if_exist, // 如果目标已经存在,是否覆盖
                                           int width,   // 图像宽度
                                           int height,  // 图像高度
                                           PictureType type //图像文件格式类型
                                           );

Snapshot_ErrCode __stdcall OutputPicture(const char* dst_file, //目标截图存储路径
                                         unsigned char* data,
                                         long data_size,
                                         PixelFormat pix,
                                         int src_width,
                                         int src_height,
                                         int width,   // 图像宽度
                                         int height,  // 图像高度
                                         PictureType type //图像文件格式类型
                                         );


Snapshot_ErrCode __stdcall OutputPictureEx(const char* dst_file, //目标截图存储路径
                                           unsigned char* data[4],
                                           int line_size[4],
                                           PixelFormat pix,
                                           int src_width,
                                           int src_height,
                                           int width,   // 图像宽度
                                           int height,  // 图像高度
                                           PictureType type); //图像


#ifdef __cplusplus
};
#endif

#endif