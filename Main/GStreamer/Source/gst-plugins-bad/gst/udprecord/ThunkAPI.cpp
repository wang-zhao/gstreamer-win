#include "Thunk.h"
#include "ThunkAPI.h"
#include <atlconv.h>

#if defined(__cplusplus)
extern "C"{
#endif

	int IsValidThunk(CThunk* thunk)
  {
    CScopedLock lc(&CThunk::m_lockInstances);

    if (CThunk::m_instances.find(thunk) >= 0)
      return 1;

    return 0;
	}

	HTHUNK THUNKAPI CreateThunk()
	{
		CThunk* thunk = new CThunk();
		if( thunk )
		{
			CScopedLock lc(&CThunk::m_lockInstances);
			CThunk::m_instances.push_back(thunk);
		}
		return (HTHUNK)(thunk);
	}

	void* THUNKAPI AddThunkEntryA(HTHUNK handle, const charA* name, void* host, void* address, ThunkType type)
	{
		USES_CONVERSION;
		CThunk* thunk = (CThunk*)(handle);

		if( IsValidThunk(thunk))
		{
			return thunk->AddThunk(A2T((LPTSTR)(name)), host, address, type);
		}
		else
		{
			return 0;
		}
	}
	void* THUNKAPI AddThunkEntryW(HTHUNK handle, const charW* name, void* host, void* address, ThunkType type)
	{
		USES_CONVERSION;
    LPCWSTR w_name =(LPCWSTR) name;
		return AddThunkEntryA(handle, W2A(w_name), host, address, type);
	}

	int THUNKAPI RemoveThunkEntryA(HTHUNK handle, const charA* name)
	{
		USES_CONVERSION;
		CThunk* thunk = (CThunk*)(handle);

		if( IsValidThunk(thunk))
		{
			return thunk->RemoveThunk(A2T((LPTSTR)(name)));
		}
		else
		{
			return 0;
		}
	}
	int THUNKAPI RemoveThunkEntryW(HTHUNK handle, const charW* name)
	{
		USES_CONVERSION;
    LPCWSTR w_name =(LPCWSTR) name;

		return RemoveThunkEntryA(handle, W2A(w_name));
	}

	int THUNKAPI ClearThunkEntries(HTHUNK handle)
	{
		CThunk* thunk = (CThunk*)(handle);

		if( IsValidThunk(thunk))
		{
			return thunk->Clear();
		}
		else
		{
			return 0;
		}
	}

	void* THUNKAPI GetThunkEntryA(HTHUNK handle, const charA* name)
	{
		USES_CONVERSION;
		CThunk* thunk = (CThunk*)(handle);

		if( IsValidThunk(thunk))
		{
			return thunk->GetThunkProc(A2T((LPTSTR)(name))).pfn;
		}
		else
		{
			return 0;
		}
	}
	void* THUNKAPI GetThunkEntryW(HTHUNK handle, const charW* name)
	{
		USES_CONVERSION;
    LPCWSTR w_name =(LPCWSTR) name;

		return GetThunkEntryA(handle, W2A(w_name));
	}

  void THUNKAPI CloseThunk(HTHUNK handle)
  {
    CThunk* thunk = (CThunk*)(handle);
    CScopedLock lc(&CThunk::m_lockInstances);

    int nth = -1;
    if((nth = CThunk::m_instances.find(thunk)) >= 0)
    {
      CThunk::m_instances.erase(nth);
      delete thunk;
    }
  }


#if defined(__cplusplus)
};
#endif
