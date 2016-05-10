/*
 */

#import "SoundflowerController.h"

#include <CoreServices/CoreServices.h>
#include <CoreAudio/CoreAudio.h>

static void ControllerSampleCallback(const void* data, const int frames,  const Float64 sampleTime, void* usr_data) {
    SoundflowerController* ctrl = (SoundflowerController*)usr_data;
    [ctrl onSample :data :frames :sampleTime];
}

@implementation SoundflowerController

AudioThruEngine	*gThruEngine2 = NULL;


void	CheckErr(OSStatus err)
{
    if (err) {
        printf("error %-4.4s %i\n", (char *)&err, (int)err);
        throw 1;
    }
}

OSStatus	HardwareListenerProc (	AudioHardwarePropertyID	inPropertyID,
                                  void*					inClientData)
{
    SoundflowerController *ctrl = (SoundflowerController *)inClientData;
    printf("HardwareListenerProc\n");
    switch(inPropertyID)
    {
        case kAudioHardwarePropertyDevices:
            //			printf("kAudioHardwarePropertyDevices\n");
            
            // An audio device has been added or removed to the system, so lets just start over
            //[NSThread detachNewThreadSelector:@selector(refreshDevices) toTarget:app withObject:nil];
            [ctrl refreshDevices];
            break;
            
        case kAudioHardwarePropertyIsInitingOrExiting:
            printf("kAudioHardwarePropertyIsInitingOrExiting\n");
            // A UInt32 whose value will be non-zero if the HAL is either in the midst of
            //initializing or in the midst of exiting the process.
            break;
            
        case kAudioHardwarePropertySleepingIsAllowed:
            printf("kAudioHardwarePropertySleepingIsAllowed\n");
            //    A UInt32 where 1 means that the process will allow the CPU to idle sleep
            //    even if there is audio IO in progress. A 0 means that the CPU will not be
            //    allowed to idle sleep. Note that this property won't affect when the CPU is
            //    forced to sleep.
            break;
            
        case kAudioHardwarePropertyUnloadingIsAllowed:
            printf("kAudioHardwarePropertyUnloadingIsAllowed\n");
            //   A UInt32 where 1 means that this process wants the HAL to unload itself
            //   after a period of inactivity where there are no IOProcs and no listeners
            //   registered with any AudioObject.
            break;
        case kAudioHardwarePropertyDefaultOutputDevice:
            AudioDeviceID did = kAudioDeviceUnknown;
            UInt32 propertySize = sizeof(AudioDeviceID);
            AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice, &propertySize, &did);
            [ctrl defalutOutputChanged: did];
            break;
    }
    
    return (noErr);
}

OSStatus	DeviceListenerProc (	AudioDeviceID           inDevice,
                                UInt32                  inChannel,
                                Boolean                 isInput,
                                AudioDevicePropertyID   inPropertyID,
                                void*                   inClientData)
{
    SoundflowerController *ctrl = (SoundflowerController *)inClientData;
    
    switch(inPropertyID)
    {
        case kAudioDevicePropertyNominalSampleRate:
            if( gThruEngine2) {  //printf("kAudioDevicePropertyNominalSampleRate\n");
                if (isInput) {
                    
                    //printf("soundflower device potential sample rate change\n");
                    if (gThruEngine2->IsRunning() && gThruEngine2->GetInputDevice() == inDevice){
                        //[NSThread detachNewThreadSelector:@selector(srChanged2ch) toTarget:app withObject:nil];
                        [ctrl srChanged2ch];
                    }
                    
                }
                else {
                    if (inChannel == 0) {
                        //printf("non-soundflower device potential sample rate change\n");
                        if (gThruEngine2->IsRunning() && gThruEngine2->GetOutputDevice() == inDevice){
                            //[NSThread detachNewThreadSelector:@selector(srChanged2chOutput) toTarget:app withObject:nil];
                            [ctrl srChanged2chOutput];
                        }
                    }
                }
            }
            break;
            
        case kAudioDevicePropertyDeviceIsAlive:
            //			printf("kAudioDevicePropertyDeviceIsAlive\n");
            break;
            
        case kAudioDevicePropertyDeviceHasChanged:
            //			printf("kAudioDevicePropertyDeviceHasChanged\n");
            break;
            
        case kAudioDevicePropertyDataSource:
            if( gThruEngine2) {
                // printf("DeviceListenerProc : HEADPHONES! \n");
                if (gThruEngine2->IsRunning() && gThruEngine2->GetOutputDevice() == inDevice){
                    //[NSThread detachNewThreadSelector:@selector(srChanged2chOutput) toTarget:app withObject:nil];
                    [ctrl srChanged2chOutput];
                }
            }
            break;
            
        case kAudioDevicePropertyVolumeScalar:
            break;
            
        case kAudioDevicePropertyDeviceIsRunning:
            //			printf("kAudioDevicePropertyDeviceIsRunning\n");
            break;
            
        case kAudioDeviceProcessorOverload:
            //			printf("kAudioDeviceProcessorOverload\n");
            break;
            
        case kAudioDevicePropertyAvailableNominalSampleRates:
            //printf("kAudioDevicePropertyAvailableNominalSampleRates\n");
            break;
            
        case kAudioStreamPropertyPhysicalFormat:
            //printf("kAudioStreamPropertyPhysicalFormat\n");
            break;
        case kAudioDevicePropertyStreamFormat:
            //printf("kAudioDevicePropertyStreamFormat\n");
            break;
            
        case kAudioDevicePropertyStreams:
            //printf("kAudioDevicePropertyStreams\n");
        case kAudioDevicePropertyStreamConfiguration:
            //printf("kAudioDevicePropertyStreamConfiguration\n");
            if (!isInput) {
                if (inChannel == 0) {
                    if( gThruEngine2) {
                        if (gThruEngine2->GetOutputDevice() == inDevice) {
                            //printf("non-soundflower device potential # of chnls change\n");
                            //[NSThread detachNewThreadSelector:@selector(checkNchnls) toTarget:app withObject:nil];
                            [ctrl checkNchnls];
                        }
                        else{ // this could be an aggregate device in the middle of constructing, going from/to 0 chans & we need to add/remove to menu
                            //[NSThread detachNewThreadSelector:@selector(refreshDevices) toTarget:app withObject:nil];
                            [ctrl refreshDevices];
                        }
                    }
                }
            }
            break;
            
        default:
            //printf("unsupported notification:%s\n", (char*)inPropertyID);
            break;
    }
    
    return noErr;
}

#include <mach/mach_port.h>
#include <mach/mach_interface.h>
#include <mach/mach_init.h>

#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/IOMessage.h>

io_connect_t  root_port = MACH_PORT_NULL;

void
MySleepCallBack(void * x, io_service_t y, natural_t messageType, void * messageArgument)
{
    SoundflowerController *ctrl = (SoundflowerController *)x;
    
    switch ( messageType ) {
        case kIOMessageSystemWillSleep:
            //printf("kIOMessageSystemWillSleep\n");
            
            [ctrl suspend];
            IOAllowPowerChange(root_port, (long)messageArgument);
            break;
            
        case kIOMessageSystemWillNotSleep:
            //printf("kIOMessageSystemWillNotSleep\n");
            break;
            
        case kIOMessageCanSystemSleep:
            //printf("kIOMessageCanSystemSleep\n");
            /* Idle sleep is about to kick in, but applications have a chance to prevent sleep
             by calling IOCancelPowerChange.  Most applications should not do this. */
            
            //IOCancelPowerChange(root_port, (long)messageArgument);
            
            /*  Power Manager waits for your reply via one of these functions for up
             to 30 seconds. If you don't acknowledge the power change by calling
             IOAllowPowerChange(), you'll delay sleep by 30 seconds. */
            
            IOAllowPowerChange(root_port, (long)messageArgument);
            break;
            
        case kIOMessageSystemHasPoweredOn:
            //printf("kIOMessageSystemHasPoweredOn\n");
            [NSTimer scheduledTimerWithTimeInterval:1.0 target:ctrl selector:@selector(resume) userInfo:nil repeats:NO];
            
            break;
            
        default:
            //printf("iomessage: %08lx\n", messageType);//"kIOMessageSystemWillPowerOn\n");
            break;
    }
}

- (void)suspend
{
    if(gThruEngine2){
        //printf("begin suspend\n");
        mSuspended2chDeviceID = gThruEngine2->GetOutputDevice();
        //gThruEngine2->SetOutputDevice(kAudioDeviceUnknown);
        [self outputDeviceSelected: kAudioDeviceUnknown];
    }
}

- (void)resume
{
    //printf("resume\n");
    
    if (mSuspended2chDeviceID == kAudioDeviceUnknown ){
        return;
    }
    
    [self refreshDevices];
    AudioDeviceList::DeviceList& thelist = mOutputDeviceList->GetList();
    AudioDeviceList::DeviceList::iterator i;
    
    if (mSuspended2chDeviceID != kAudioDeviceUnknown) {
        for (i = thelist.begin(); i != thelist.end(); ++i){
            if ((*i).mID == mSuspended2chDeviceID) {
                break;
            }
        }
        if (i == thelist.end()){
            mSuspended2chDeviceID = kAudioDeviceUnknown;
        }
    }
    if (mSuspended2chDeviceID != kAudioDeviceUnknown){
        [self outputDeviceSelected: mSuspended2chDeviceID];
        mSuspended2chDeviceID = kAudioDeviceUnknown;
    }
}


- (void)srChanged2ch
{
    AudioDeviceID backDevId;
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    gThruEngine2->Mute();
    OSStatus err = gThruEngine2->MatchSampleRate(false);
    backDevId = mCur2chOutputDevice;
    [self outputDeviceSelected: kAudioDeviceUnknown];
    mCur2chOutputDevice = backDevId;
    if (err == kAudioHardwareNoError) {
        //usleep(1000);
        [self outputDeviceSelected:mCur2chOutputDevice];
    }
    
    gThruEngine2->Mute(false);
    
    [pool release];
}

- (void)srChanged2chOutput
{
    AudioDeviceID backDevId;
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    gThruEngine2->Mute();
    OSStatus err = gThruEngine2->MatchSampleRate(false);
    backDevId = mCur2chOutputDevice;
    [self outputDeviceSelected: kAudioDeviceUnknown];
    mCur2chOutputDevice = backDevId;
    if (err == kAudioHardwareNoError) {
        //usleep(1000);
        [self outputDeviceSelected:mCur2chOutputDevice];
    }
    gThruEngine2->Mute(false);
    
    [pool release];
}

- (void)checkNchnls
{
    AudioDeviceID backDevId;
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    if (mNchnls2 != gThruEngine2->GetOutputNchnls())
    {
        backDevId = mCur2chOutputDevice;
        [self outputDeviceSelected: kAudioDeviceUnknown];
        mCur2chOutputDevice = backDevId;
        //usleep(1000);
        [self outputDeviceSelected:mCur2chOutputDevice];
    }
    
    [pool release];
}


- (void)refreshDevices
{
    AudioDeviceID backDevId;
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    [self buildDeviceList];
    AudioDeviceList::DeviceList &thelist = mOutputDeviceList->GetList();
    AudioDeviceList::DeviceList::iterator i;
    
    if (!gThruEngine2  && mSoundflower2Device != kAudioDeviceUnknown) {
        
        gThruEngine2 = new AudioThruEngine;
        gThruEngine2->SetCallback(ControllerSampleCallback, self);
        gThruEngine2->SetInputDevice(mSoundflower2Device);
        gThruEngine2->Start();
        
        AudioDeviceID did = kAudioDeviceUnknown;
        UInt32 propertySize;
        AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice,  &propertySize,   &did);
        if( did != kAudioDeviceUnknown && did != mSoundflower2Device) {
            mCur2chOutputDevice = did;
            AudioHardwareSetProperty(kAudioHardwarePropertyDefaultOutputDevice,  sizeof(UInt32),   &mSoundflower2Device);
        }
        for (i = thelist.begin(); i != thelist.end(); ++i){
            if ((*i).mID == mCur2chOutputDevice) {
                break;
            }
        }
        
        if (i == thelist.end()){ // we didn't find it, turn selection to none
            backDevId = mCur2chOutputDevice;
            [self outputDeviceSelected:  kAudioDeviceUnknown];
            mCur2chOutputDevice = backDevId;
        }else{
            [self outputDeviceSelected: mCur2chOutputDevice];
        }
    } else {
        // make sure that one of our current device's was not removed!
        AudioDeviceID dev = gThruEngine2->GetOutputDevice();
        if (dev == kAudioDeviceUnknown && mCur2chOutputDevice != dev){
            dev = mCur2chOutputDevice;
        }
        
        for (i = thelist.begin(); i != thelist.end(); ++i){
            if ((*i).mID == dev) {
                break;
            }
        }
        
        if (i == thelist.end()){ // we didn't find it, turn selection to none
            backDevId = mCur2chOutputDevice;
            [self outputDeviceSelected:  kAudioDeviceUnknown];
            mCur2chOutputDevice = backDevId;
        } else {
            mCur2chOutputDevice = (*i).mID;
            [self outputDeviceSelected:  mCur2chOutputDevice];
        }
    }
    
    [pool release];
}



- (void)InstallListeners;
{
    // add listeners for all devices, including soundflowers
    AudioDeviceList::DeviceList &thelist = mOutputDeviceList->GetList();
    int index = 0;
    for (AudioDeviceList::DeviceList::iterator i = thelist.begin(); i != thelist.end(); ++i, ++index) {
        
        //verify_noerr (AudioDeviceAddPropertyListener((*i).mID, 0, false, kAudioStreamPropertyPhysicalFormat, DeviceListenerProc, self));
        //verify_noerr (AudioDeviceAddPropertyListener((*i).mID, 0, false, kAudioDevicePropertyStreamFormat, DeviceListenerProc, self));
        verify_noerr (AudioDeviceAddPropertyListener((*i).mID, 0, false, kAudioDevicePropertyNominalSampleRate, DeviceListenerProc, self));
        //verify_noerr (AudioDeviceAddPropertyListener((*i).mID, 0, false, kAudioDevicePropertyLatency, DeviceListenerProc, self));
        //verify_noerr (AudioDeviceAddPropertyListener((*i).mID, 0, false, kAudioDevicePropertySafetyOffset, DeviceListenerProc, self));
        verify_noerr (AudioDeviceAddPropertyListener((*i).mID, 0, false, kAudioDevicePropertyStreamConfiguration, DeviceListenerProc, self));
        verify_noerr (AudioDeviceAddPropertyListener((*i).mID, 0, false, kAudioDevicePropertyStreams, DeviceListenerProc, self));
        //verify_noerr (AudioDeviceAddPropertyListener((*i).mID, 0, false, kAudioDevicePropertyAvailableNominalSampleRates, DeviceListenerProc, self));
        
        // this provides us, for example, with notification when the headphones are plugged/unplugged during playback
        verify_noerr (AudioDeviceAddPropertyListener((*i).mID, 0, false, kAudioDevicePropertyDataSource, DeviceListenerProc, self));
        
    }
    
    if ( mSoundflower2Device != kAudioDeviceUnknown ) {
        //verify_noerr (AudioDeviceAddPropertyListener((*i).mID, 0, true, kAudioStreamPropertyPhysicalFormat, DeviceListenerProc, self));
        //verify_noerr (AudioDeviceAddPropertyListener((*i).mID, 0, true, kAudioDevicePropertyStreamFormat, DeviceListenerProc, self));
        verify_noerr (AudioDeviceAddPropertyListener(mSoundflower2Device, 0, true, kAudioDevicePropertyNominalSampleRate, DeviceListenerProc, self));
        //verify_noerr (AudioDeviceAddPropertyListener((*i).mID, 0, true, kAudioDevicePropertyLatency, DeviceListenerProc, self));
        //verify_noerr (AudioDeviceAddPropertyListener((*i).mID, 0, true, kAudioDevicePropertySafetyOffset, DeviceListenerProc, self));
        verify_noerr (AudioDeviceAddPropertyListener(mSoundflower2Device, 0, true, kAudioDevicePropertyStreamConfiguration, DeviceListenerProc, self));
        //verify_noerr (AudioDeviceAddPropertyListener((*i).mID, 0, true, kAudioDevicePropertyAvailableNominalSampleRates, DeviceListenerProc, self));
        
        verify_noerr (AudioDeviceAddPropertyListener(mSoundflower2Device, 0, true, kAudioDevicePropertyDeviceIsAlive, DeviceListenerProc, self));
        verify_noerr (AudioDeviceAddPropertyListener(mSoundflower2Device, 0, true, kAudioDevicePropertyDeviceHasChanged, DeviceListenerProc, self));
        verify_noerr (AudioDeviceAddPropertyListener(mSoundflower2Device, 0, true, kAudioDevicePropertyDeviceIsRunning, DeviceListenerProc, self));
        verify_noerr (AudioDeviceAddPropertyListener(mSoundflower2Device, 0, true, kAudioDeviceProcessorOverload, DeviceListenerProc, self));
    }
    
    
    // check for added/removed devices
    verify_noerr (AudioHardwareAddPropertyListener(kAudioHardwarePropertyDevices, HardwareListenerProc, self));
    
    verify_noerr (AudioHardwareAddPropertyListener(kAudioHardwarePropertyIsInitingOrExiting, HardwareListenerProc, self));
    verify_noerr (AudioHardwareAddPropertyListener(kAudioHardwarePropertySleepingIsAllowed, HardwareListenerProc, self));
    verify_noerr (AudioHardwareAddPropertyListener(kAudioHardwarePropertyUnloadingIsAllowed, HardwareListenerProc, self));
    
    
    verify_noerr (AudioHardwareAddPropertyListener(kAudioHardwarePropertyDefaultOutputDevice, HardwareListenerProc, self));
    
    /*	UInt32 val, size = sizeof(UInt32);
     AudioHardwareGetProperty(kAudioHardwarePropertySleepingIsAllowed, &size, &val);
     printf("Sleep is %s\n", (val ? "allowed" : "not allowed"));
     AudioHardwareGetProperty(kAudioHardwarePropertyUnloadingIsAllowed, &size, &val);
     printf("Unloading is %s\n", (val ? "allowed" : "not allowed"));
     */
}

- (void)RemoveListeners
{
    AudioDeviceList::DeviceList &thelist = mOutputDeviceList->GetList();
    int index = 0;
    for (AudioDeviceList::DeviceList::iterator i = thelist.begin(); i != thelist.end(); ++i, ++index) {
        
        verify_noerr (AudioDeviceRemovePropertyListener((*i).mID, 0, false, kAudioDevicePropertyNominalSampleRate, DeviceListenerProc));
        verify_noerr (AudioDeviceRemovePropertyListener((*i).mID, 0, false, kAudioDevicePropertyStreamConfiguration, DeviceListenerProc));
        verify_noerr (AudioDeviceRemovePropertyListener((*i).mID, 0, false, kAudioDevicePropertyStreams, DeviceListenerProc));
        verify_noerr (AudioDeviceRemovePropertyListener((*i).mID, 0, false, kAudioDevicePropertyDataSource, DeviceListenerProc));
    }
    if (mSoundflower2Device != kAudioDeviceUnknown) {
        verify_noerr (AudioDeviceRemovePropertyListener(mSoundflower2Device, 0, true, kAudioDevicePropertyNominalSampleRate, DeviceListenerProc));
        verify_noerr (AudioDeviceRemovePropertyListener(mSoundflower2Device, 0, true, kAudioDevicePropertyStreamConfiguration, DeviceListenerProc));
        verify_noerr (AudioDeviceRemovePropertyListener(mSoundflower2Device, 0, true, kAudioDevicePropertyDeviceIsAlive, DeviceListenerProc));
        verify_noerr (AudioDeviceRemovePropertyListener(mSoundflower2Device, 0, true, kAudioDevicePropertyDeviceHasChanged, DeviceListenerProc));
        verify_noerr (AudioDeviceRemovePropertyListener(mSoundflower2Device, 0, true, kAudioDevicePropertyDeviceIsRunning, DeviceListenerProc));
        verify_noerr (AudioDeviceRemovePropertyListener(mSoundflower2Device, 0, true, kAudioDeviceProcessorOverload, DeviceListenerProc));
    }
    verify_noerr (AudioHardwareRemovePropertyListener(kAudioHardwarePropertyDevices, HardwareListenerProc));
    verify_noerr (AudioHardwareRemovePropertyListener(kAudioHardwarePropertyIsInitingOrExiting, HardwareListenerProc));
    verify_noerr (AudioHardwareRemovePropertyListener(kAudioHardwarePropertySleepingIsAllowed, HardwareListenerProc));
    verify_noerr (AudioHardwareRemovePropertyListener(kAudioHardwarePropertyUnloadingIsAllowed, HardwareListenerProc));
    verify_noerr (AudioHardwareRemovePropertyListener(kAudioHardwarePropertyDefaultOutputDevice, HardwareListenerProc));
}

- (id)init
{
    mOutputDeviceList = NULL;
    
    mSoundflower2Device = 0;
    mNchnls2 = 0;
    
    mCur2chBufferSize = 0 ;
    mCur2chOutputDevice = kAudioDeviceUnknown;
    mSuspended2chDeviceID = kAudioDeviceUnknown;
    
    mSoundflower2DeviceName = @"Soundflower (2ch)";
    
    mPowerNotify  = NULL;
    mPowerIterator = 0;
    
    g_mutex_init(&mLock);
    
    [self awake];
    
    return self;
}

- (void)dealloc
{
    if (gThruEngine2){
        gThruEngine2->Stop();
        delete gThruEngine2;
        gThruEngine2 = NULL;
    }
    if (mOutputDeviceList){
        [self RemoveListeners];
        delete mOutputDeviceList;
        mOutputDeviceList = NULL;
    }
    if (mPowerIterator) {
        IODeregisterForSystemPower(&mPowerIterator);
        mPowerIterator = 0;
    }
    if (root_port) {
        IOServiceClose(root_port);
        root_port = MACH_PORT_NULL;
    }
    if (mPowerNotify) {
        IONotificationPortDestroy(mPowerNotify);
        mPowerNotify = NULL;
    }
    if (mCur2chOutputDevice != kAudioDeviceUnknown) {
        AudioHardwareSetProperty(kAudioHardwarePropertyDefaultOutputDevice, sizeof(UInt32), &mCur2chOutputDevice);
        mCur2chOutputDevice = kAudioDeviceUnknown;
    }
    [mSoundflower2DeviceName release];
    
    g_mutex_clear(&mLock);
    [super dealloc];
}

- (void)buildDeviceList
{
    if (mOutputDeviceList) {
        [ self RemoveListeners];
        delete mOutputDeviceList;
        mOutputDeviceList =NULL;
    }
    
    
    //Sometimes selecting "Airplay" causes empty device list for a while and then
    //changes all DeviceID(CoreAudio Restarted??), In that case we need retart
    Boolean restartRequired = false;
    mOutputDeviceList = new AudioDeviceList(false);
    while(mOutputDeviceList->GetList().size() == 0){
        restartRequired = true;
        delete mOutputDeviceList;
        mOutputDeviceList =NULL;
        [NSThread sleepForTimeInterval:0.1];
        mOutputDeviceList = new AudioDeviceList(false);
        NSLog(@"----------waiting for devices");
    }
    
    // find soundflower devices, store and remove them from our output list
    AudioDeviceList::DeviceList &thelist = mOutputDeviceList->GetList();
    int index = 0;
    mSoundflower2Device = kAudioDeviceUnknown;
    for (AudioDeviceList::DeviceList::iterator i = thelist.begin(); i != thelist.end(); ++i, ++index) {
        NSString* name = [NSString stringWithUTF8String: (*i).mName];
        if (NSOrderedSame == [mSoundflower2DeviceName caseInsensitiveCompare: name]) {
            mSoundflower2Device = (*i).mID;
            AudioDeviceList::DeviceList::iterator toerase = i;
            i--;
            thelist.erase(toerase);
        }
        [name autorelease];
    }
    
    if (restartRequired) {
        NSLog(@"restarting Thru Engines");
        
        if (gThruEngine2){
            gThruEngine2->Stop();
            delete gThruEngine2;
            gThruEngine2 = NULL;
        }
    }
    [self InstallListeners];
}

- (void)awake
{
    [self buildDeviceList];
    
    if (mSoundflower2Device) {
        gThruEngine2 = new AudioThruEngine;
        gThruEngine2->SetCallback(ControllerSampleCallback, self);
        gThruEngine2->SetInputDevice(mSoundflower2Device);
        gThruEngine2->Start();
        
        AudioDeviceID did = kAudioDeviceUnknown;
        UInt32 propertySize=sizeof(AudioDeviceID);
        AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice,  &propertySize,   &did);
        if( did != kAudioDeviceUnknown && did != mSoundflower2Device) {
            mCur2chOutputDevice = did;
            AudioHardwareSetProperty(kAudioHardwarePropertyDefaultOutputDevice,  sizeof(UInt32),   &mSoundflower2Device);
        }
        [self outputDeviceSelected: mCur2chOutputDevice];
        
        
        root_port = IORegisterForSystemPower(self, &mPowerNotify, MySleepCallBack, &mPowerIterator);
        if ( !root_port ) {
            printf("IORegisterForSystemPower failed\n");
        }
        else
            CFRunLoopAddSource(CFRunLoopGetCurrent(),
                               IONotificationPortGetRunLoopSource(mPowerNotify),
                               kCFRunLoopCommonModes);
    } else {
        if (mOutputDeviceList) {
            [ self RemoveListeners];
            delete mOutputDeviceList;
            mOutputDeviceList =NULL;
        }
    }
}

- (BOOL)isOk {
    return mSoundflower2Device != kAudioDeviceUnknown;
}

- (void)bufferSizeChanged2ch:(int)idx
{
    UInt32 size = 64 << idx;
    if ( size != mCur2chBufferSize) {
        if( gThruEngine2) {
            gThruEngine2->SetBufferSize(size);
        }
        mCur2chBufferSize = size;
    }
}

- (void)outputDeviceSelected:(AudioDeviceID)deviceId
{
    if (mCur2chOutputDevice != deviceId || (gThruEngine2 && gThruEngine2->GetOutputDevice() != deviceId)) {
        if(gThruEngine2){
            gThruEngine2->SetOutputDevice(deviceId );
        }
        mCur2chOutputDevice = deviceId;
    }
}

- (void)defalutOutputChanged:(AudioDeviceID) devid {
    if (devid != mSoundflower2Device) {
        AudioHardwareSetProperty(kAudioHardwarePropertyDefaultOutputDevice, sizeof(UInt32), &mSoundflower2Device);
        [self  outputDeviceSelected: devid];
    }
}

- (void*)register:(SampleCallback) callback :( void*) usr_data {
    g_mutex_lock(&mLock);
    if (mCallbacks.find(usr_data) != mCallbacks.end()) {
        g_mutex_unlock(&mLock);
        return usr_data;
    }
    mCallbacks.insert(CallbackMap::value_type(usr_data, callback));
    g_mutex_unlock(&mLock);
    return usr_data;
}
- (void*)unregister:(void**)reg_id{
    g_mutex_lock(&mLock);
    mCallbacks.erase(*reg_id);
    *reg_id = NULL;
    g_mutex_unlock(&mLock);
}

- (void)getSampleRate:(int*)rate {
    if( gThruEngine2) {
        *rate = gThruEngine2->getSampleRate();
    }else {
        *rate = 0;
    }
}
- (void)onSample:(const void*)data :(const int) frames :(const Float64) sampleTime {
    g_mutex_lock(&mLock);
    CallbackMap::const_iterator it;
    for( it = mCallbacks.begin(); it != mCallbacks.end(); it ++ ) {
        it->second(data,  frames*8, sampleTime, it->first);
    }
    g_mutex_unlock(&mLock);
}
@end
