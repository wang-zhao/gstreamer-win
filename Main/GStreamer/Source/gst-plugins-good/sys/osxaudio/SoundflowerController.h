/* SoundflowerController */

#import <Cocoa/Cocoa.h>
#include "AudioDeviceList.h"
#include <map>

#include "AudioThruEngine.h"


typedef std::map<void*, SampleCallback> CallbackMap;

@interface SoundflowerController : NSObject
{
    AudioDeviceID               mSuspended2chDeviceID;
	
	AudioDeviceID				mSoundflower2Device;
	
	AudioDeviceList *			mOutputDeviceList;
    
    UInt32                             mCur2chBufferSize;
    
    AudioDeviceID               mCur2chOutputDevice;
	
	UInt32 mNchnls2;
    
    NSString *                       mSoundflower2DeviceName;
    
    
    IONotificationPortRef  mPowerNotify;
    io_object_t            mPowerIterator;
    
    CallbackMap mCallbacks;
    GMutex mLock;
}

- (id)init;

- (void)dealloc;

- (BOOL)isOk;

- (void)setSoundflowerDeviceName:(NSString*) name;

- (void)suspend;
- (void)resume;

- (void)srChanged2ch;
- (void)srChanged2chOutput;
- (void)checkNchnls;
- (void)awake;
- (void)refreshDevices;

- (void)outputDeviceSelected:(AudioDeviceID) devId;
- (void)bufferSizeChanged2ch:(int)idx;

- (void)buildDeviceList;

- (void)InstallListeners;
- (void)RemoveListeners;
- (void)defalutOutputChanged:(AudioDeviceID) devid;
- (void*)register:(SampleCallback) callback :(void*) usr_data;
- (void*)unregister:(void**)reg_id;
- (void)getSampleRate:(int*)rate;
- (void)onSample:(const void*)data :(const int) frames :(const Float64) sampleTime;

@end
