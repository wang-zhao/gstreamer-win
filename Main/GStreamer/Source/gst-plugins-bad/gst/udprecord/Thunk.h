#ifndef __THUNK_H__
#define __THUNK_H__

#include "ThunkAPI.h"
#include <windows.h>
#include <glib.h>

#if (defined _UNICODE) || (defined UNICODE)
#define _tstrcmpnocase       _wcsicmp
#else
#define _tstrcmpnocase       _stricmp
#endif

#define DECLARE_FUNCTION_PROTOTYPES(rettype, shortname, calltype )\
	rettype (calltype *pfn_##shortname##_0)( );\
	rettype (calltype *pfn_##shortname##_1)( void* );\
	rettype (calltype *pfn_##shortname##_2)( void*, void* );\
	rettype (calltype *pfn_##shortname##_3)( void*, void*, void* );\
	rettype (calltype *pfn_##shortname##_4)( void*, void*, void*, void*);\
	rettype (calltype *pfn_##shortname##_5)( void*, void*, void*, void*, void*);\
	rettype (calltype *pfn_##shortname##_6)( void*, void*, void*, void*, void*, void*);\
	rettype (calltype *pfn_##shortname##_7)( void*, void*, void*, void*, void*, void*, void*);\
	rettype (calltype *pfn_##shortname##_8)( void*, void*, void*, void*, void*, void*, void*, void*);\
	rettype (calltype *pfn_##shortname##_9)( void*, void*, void*, void*, void*, void*, void*, void*, void*);

#define DECLARE_FUNCTION_PROTOTYPE_UNION(name, datatype, calltype)\
union name\
{\
	datatype pfn;\
	DECLARE_FUNCTION_PROTOTYPES(unsigned long, u, calltype)\
	DECLARE_FUNCTION_PROTOTYPES(long, i, calltype)\
	DECLARE_FUNCTION_PROTOTYPES(void, v, calltype)\
}

#pragma pack(push)
#pragma pack(1)

typedef struct tagStdCallThunk
{
	BYTE dw_esp[3];
	DWORD dw_mov;
	void* vp_this;
	BYTE by_jmp;
	void* vp_relproc;
} CStdCallThunkBase;

typedef struct tagCdeclCallThunk
{
	BYTE by_mov_ecx;
	void* vp_this;

	BYTE by_pop_edx;
	WORD wd_sub_edx_ecx;

	WORD wd_mov_mem;
	BYTE by_offset;

	BYTE by_push_this;
	void* vp_host;

	BYTE by_call;
	void* vp_relproc;

	WORD wd_add_esp;
	BYTE by_4;

	BYTE by_jmp;
	DWORD dw_retaddr;

	WORD wd_padding;
	BYTE by_padding;
} CCdeclCallThunkBase;

typedef 
DECLARE_FUNCTION_PROTOTYPE_UNION(tagStdcallFunctionProtoTypes, CStdCallThunkBase*, __stdcall)
StdcallFunctionProtoTypes;

typedef
DECLARE_FUNCTION_PROTOTYPE_UNION(tagCdeclFunctionProtoTypes, CCdeclCallThunkBase*, __cdecl)
CdeclFunctionProtoTypes;

typedef union tagFunctionProtoTypes
{
	void* pfn;
	CdeclFunctionProtoTypes Cdecl;
	StdcallFunctionProtoTypes Std;
}FunctionProtoTypes;

#pragma pack(pop)


class CCritSec
{
public:
	CRITICAL_SECTION m_cs;
public:
	CCritSec()
	{
		::InitializeCriticalSection(&m_cs);
	}
public:
	~CCritSec()
	{
		::DeleteCriticalSection(&m_cs);
	}

public:
	void Lock() {::EnterCriticalSection(&m_cs);}
	void Unlock(){::LeaveCriticalSection(&m_cs);}
};

class CScopedLock
{
public:
	CScopedLock(CCritSec* pLock)
		: m_pLock(pLock)
	{
		m_pLock->Lock();
	}
	~CScopedLock()
	{
		m_pLock->Unlock();
	}

private:
	CCritSec* m_pLock;
};

class CThunkMemAllocator
{
private:
	typedef struct _ThunkNode
	{
		unsigned char*		addr;
		size_t				size;
		struct _ThunkNode*	prev;
		struct _ThunkNode*	next;
	}ThunkNode;

	class NodeListOp
	{
	public:
		static bool			empty	(ThunkNode* list_head);
		static ThunkNode*	next	(ThunkNode* node);
		static ThunkNode*	prev	(ThunkNode* node);
		static ThunkNode*	find	(ThunkNode* list_head, void* addr);

		static void			remove	(ThunkNode** list_head, ThunkNode* node);
		static void			add		(ThunkNode** list_head, ThunkNode* node);

		static ThunkNode*   remove_head( ThunkNode** list_head);
	};

private:
	CCritSec	mutex;
	ThunkNode*	empty_list;
	ThunkNode*	alloc_list;
	ThunkNode*  page_list;

	size_t		page_size;

private:
	enum{ growth = 32*1024 };

public:
	CThunkMemAllocator();
	virtual ~CThunkMemAllocator();
public:
	void* alloc_thunk(size_t size);
	void  free_thunk(void* p);
private:
	void DoMerge(ThunkNode** list_head);
};

void* gAllocThunkMemory(size_t size);
void gFreeThunkMemory(void* p);


class IThunk
{
public:
	virtual void* GetThunkProc() = 0;
public:
	virtual ~IThunk()
	{

	}
};

class CStdCallThunkImpl : public IThunk
{
public:
	typedef CStdCallThunkImpl		_MyT;
private:
	CStdCallThunkBase* m_Thunk;
public:
	CStdCallThunkImpl(void* pthis, void* method)
	{
		m_Thunk = (CStdCallThunkBase*)gAllocThunkMemory(sizeof(CStdCallThunkBase));


		m_Thunk->dw_esp[0] = 0xFF;		//
		m_Thunk->dw_esp[1] = 0x34;		//
		m_Thunk->dw_esp[2] = 0x24;		// push dword ptr [esp]

		m_Thunk->dw_mov = 0x042444C7;	// mov dword ptr[esp+4], this
		m_Thunk->vp_this = pthis;
		m_Thunk->by_jmp = 0xE9;			// jmp realproc
		m_Thunk->vp_relproc = (void*)(*(int*)&method - (int)(m_Thunk + 1));
	}

	virtual ~CStdCallThunkImpl()
	{
		gFreeThunkMemory(m_Thunk);
	}

	void* GetThunkProc()
	{
		return m_Thunk;
	}
};


class CCdeclCallThunkImpl : public IThunk
{
public:
	typedef CCdeclCallThunkImpl		_MyT;
private:
	CCdeclCallThunkBase* m_Thunk;
public:
	CCdeclCallThunkImpl(void* pthis, void* method)
	{
		m_Thunk = (CCdeclCallThunkBase*)gAllocThunkMemory(sizeof(CCdeclCallThunkBase));

		m_Thunk->by_mov_ecx = 0xB9;					// mov ecx, this+29
		m_Thunk->vp_this = (void*)((int)(m_Thunk)+29);

		m_Thunk->by_pop_edx = 0x5A;					// pop edx;

		m_Thunk->wd_sub_edx_ecx = 0xD12B;			// sub edx, ecx;

		m_Thunk->wd_mov_mem = 0x5189;
		m_Thunk->by_offset = 0xFC;					// mov dword ptr [ecx-04],edx;

		m_Thunk->by_push_this = 0x68;
		m_Thunk->vp_host = pthis;					// push vpthis;

		m_Thunk->by_call = 0xE8;						// call realproc;
		m_Thunk->vp_relproc = (void*)(*(int*)(&method) - ((int)(m_Thunk) + 21));

		m_Thunk->wd_add_esp = 0xC483;				// add esp,4
		m_Thunk->by_4 = 0x4;			

		m_Thunk->by_jmp = 0xE9;						// ret;
		m_Thunk->dw_retaddr = 0;
		m_Thunk->wd_padding = 0x9090;
		m_Thunk->by_padding = 0x90;
	}

	virtual ~CCdeclCallThunkImpl()
	{
		gFreeThunkMemory(m_Thunk);
	}

	void* GetThunkProc()
	{
		return m_Thunk;
	}
};

class CThunk;

class ThunkArray
{
public:
  ThunkArray()
  {
   m_list = 0;
  }

  ~ThunkArray()
  {
    g_list_free_full(m_list, &ThunkArray::DestroyNode);
  }

public:
  void push_back(CThunk* thunk)
  {
    m_list = g_list_append(m_list, thunk);
  }
  int find(CThunk* thunk)
  {
    return g_list_index(m_list, thunk);
  }
  void erase(int nth)
  {
    m_list = g_list_remove(m_list, g_list_nth_data(m_list, nth));
  }
private:
  static void DestroyNode(gpointer data);
private:
  GList* m_list;
};

class CThunk
{
public:
	static ThunkArray m_instances;
	static CCritSec m_lockInstances;

private:
	CCritSec m_lockThunk;

  GHashTable* m_hash;

public:

  CThunk()
  {
    m_hash = g_hash_table_new(g_str_hash,  g_str_equal);
  }
public:
	virtual ~CThunk()
	{
		Clear();
    g_hash_table_destroy(m_hash);
    g_hash_table_unref(m_hash);
	}

	void* AddThunk(gchar* lpName,void* phost, void* method, ThunkType type)
	{
		CScopedLock lc(&m_lockThunk);

    IThunk* p = 0;

    gpointer data = g_hash_table_lookup(m_hash,lpName);

    if( data != NULL)
    {
      return 0;
    }

		if( type == Cdeclcall )
		{
			p = new CCdeclCallThunkImpl(phost, method);
		}
		else if( type == Stdcall )
		{
			p = new CStdCallThunkImpl(phost, method);
		}
		else
		{
			return 0;
		}

		if(!p)
		{
			return 0;
		}

		g_hash_table_insert(m_hash, lpName,p);

		return p->GetThunkProc();
	}

	int RemoveThunk(gchar* lpName)
	{
		CScopedLock lc(&m_lockThunk);

    gpointer data = g_hash_table_lookup(m_hash, lpName);

		if( data == NULL)
		{
			return 0;
		}

    IThunk* thunk = reinterpret_cast<IThunk*>(data);

		delete thunk;
    g_hash_table_remove(m_hash, lpName);

		return 1;
	}

	FunctionProtoTypes GetThunkProc(gchar* lpName)
	{
		CScopedLock lc(&m_lockThunk);

    gpointer data = g_hash_table_lookup(m_hash, lpName);
	
		if( data != NULL )
		{
      IThunk* thunk =reinterpret_cast<IThunk*>(data);

			FunctionProtoTypes fp;
			fp.pfn = thunk->GetThunkProc();
			return fp;
		}
		else
		{
			// 如果没有， 返回空指针
			FunctionProtoTypes fp;
			fp.Std.pfn_i_0= 0;
			return fp;
		}
	}
  static void deleteThunk (gpointer       key,
    gpointer       value,
    gpointer       user_data)
  {
    IThunk* thunk = reinterpret_cast<IThunk*>(value);
    delete thunk;
  }

	int Clear()
	{
		int ret = g_hash_table_size(m_hash);

		CScopedLock lc(&m_lockThunk);
		g_hash_table_foreach(m_hash,deleteThunk,0);

    g_hash_table_remove_all(m_hash);
		return ret;
	}
};


#endif//__THUNK_H__