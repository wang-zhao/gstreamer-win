#include "FastDelegate.h"
#include <DemuxAPI/demuxer/demuxer.h>

typedef fastdelegate::FastDelegate0<int> CancelFunType;

typedef fastdelegate::FastDelegate2<uint8_t*, int, int> DataFunType;


URLInterruptCB MakeInterruptCB(void*data, URLInterruptCB callback)
{
  MakeDelegate(&c, &CDerivedClass::SimpleDerivedFunction);
  CancelFunType()
}
URLDataCB MakeDataCB(void* data, URLDataCB callback)
{

}

