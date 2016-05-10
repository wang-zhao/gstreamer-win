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


Snapshot_ErrCode __stdcall GetClipSnapshot(const char* src_file, //Դ�ļ�·��
                                           const char* dst_file, //Ŀ���ͼ�洢·��
                                           long offset_in_ms,           // ֡ƫ����,��λ :����
                                           bool override_if_exist, // ���Ŀ���Ѿ�����,�Ƿ񸲸�
                                           int width,   // ͼ����
                                           int height,  // ͼ��߶�
                                           PictureType type //ͼ���ļ���ʽ����
                                           );

Snapshot_ErrCode __stdcall OutputPicture(const char* dst_file, //Ŀ���ͼ�洢·��
                                         unsigned char* data,
                                         long data_size,
                                         PixelFormat pix,
                                         int src_width,
                                         int src_height,
                                         int width,   // ͼ����
                                         int height,  // ͼ��߶�
                                         PictureType type //ͼ���ļ���ʽ����
                                         );


Snapshot_ErrCode __stdcall OutputPictureEx(const char* dst_file, //Ŀ���ͼ�洢·��
                                           unsigned char* data[4],
                                           int line_size[4],
                                           PixelFormat pix,
                                           int src_width,
                                           int src_height,
                                           int width,   // ͼ����
                                           int height,  // ͼ��߶�
                                           PictureType type); //ͼ��


#ifdef __cplusplus
};
#endif

#endif