#include <stdio.h>
#include <librtmp/rtmp.h>
#include <io.h>
#include <stdlib.h>

int main()
{
  RTMP* rtmp;

  rtmp = RTMP_Alloc();
  RTMP_Init(rtmp);

  FILE* f = fopen("d:\\dump.flv", "rb");
  
  int le = _filelength(_fileno(f));

  char* read_buf = (char*)malloc(le);
  fread(read_buf, 1, le, f);
  fclose(f);

  RTMP_Read(rtmp, read_buf, le);

}