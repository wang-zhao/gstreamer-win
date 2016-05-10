#ifndef __thunkapi_h__
#define __thunkapi_h__


#if defined(__cplusplus)
extern "C"{
#endif

  typedef enum _ThunkType
  {
    Cdeclcall,
    Stdcall
  }ThunkType;

#ifndef __charAdefined__
#define __charAdefined__
  typedef char charA;
#endif

#ifndef __charWdefined__
#define __charWdefined__
  typedef unsigned short charW;
#endif

#ifndef __HTHUNK_defined__
#define __HTHUNK_defined__
  typedef struct { int __unused; }* HTHUNK;
#endif

#define THUNKAPI __cdecl


  HTHUNK THUNKAPI CreateThunk();	
  void* THUNKAPI AddThunkEntryA(HTHUNK handle, const charA* name, void* host, void* address, ThunkType type);	
  void* THUNKAPI AddThunkEntryW(HTHUNK handle, const charW* name, void* host, void* address, ThunkType type);
  int THUNKAPI RemoveThunkEntryA(HTHUNK handle, const charA* name);
  int THUNKAPI RemoveThunkEntryW(HTHUNK handle, const charW* name);	
  int THUNKAPI ClearThunkEntries(HTHUNK handle);
  void* THUNKAPI GetThunkEntryA(HTHUNK handle, const charA* name);	
  void* THUNKAPI GetThunkEntryW(HTHUNK handle, const charW* name);
  void THUNKAPI CloseThunk(HTHUNK handle);


#if defined(__cplusplus)
};
#endif


#endif // __thunkapi_h__