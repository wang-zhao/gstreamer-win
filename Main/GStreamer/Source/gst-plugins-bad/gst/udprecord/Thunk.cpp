#include "Thunk.h"
#include <atlconv.h>

CThunkMemAllocator::CThunkMemAllocator()
: empty_list(0)
, alloc_list(0)
, page_list(0)
{
	SYSTEM_INFO info = {0};
	::GetSystemInfo(&info);
	page_size = max(512, min(1024, info.dwPageSize));
}

CThunkMemAllocator::~CThunkMemAllocator()
{
	//ASSERT(NodeListOp::empty(alloc_list));

	for (ThunkNode* ptr = page_list; 
		!NodeListOp::empty(ptr); 
		ptr = NodeListOp::next(ptr))
	{
		::VirtualFree(ptr->addr, 0, MEM_RELEASE);
	}
	
	for( ThunkNode* ptr = NodeListOp::remove_head(&page_list);
		!NodeListOp::empty(ptr);
		ptr = NodeListOp::remove_head(&page_list))
	{
		delete ptr;
	}

	for( ThunkNode* ptr = NodeListOp::remove_head(&empty_list);
		!NodeListOp::empty(ptr);
		ptr = NodeListOp::remove_head(&empty_list))
	{
		delete ptr;
	}

	for( ThunkNode* ptr = NodeListOp::remove_head(&alloc_list);
		!NodeListOp::empty(ptr);
		ptr = NodeListOp::remove_head(&alloc_list))
	{
		delete ptr;
	}
}

void* CThunkMemAllocator::alloc_thunk(size_t size)
{
	CScopedLock lc(&mutex);
	if( size > growth )
	{
		return NULL;
	}

	for (int i = 0; i < 2; i ++)
	{
		if( NodeListOp::empty(empty_list) || (i != 0 ))
		{
			int nGrow = ((growth+page_size-1) / page_size) * page_size;

			void* addr = ::VirtualAlloc(NULL, nGrow, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
			if( addr )
			{
				ThunkNode* node = new ThunkNode();
				node->addr = (unsigned char*)(addr);
				node->size = nGrow;
				node->next = NULL;
				node->prev = NULL;

				ThunkNode* node2 = new ThunkNode();
				*node2 = *node;

				NodeListOp::add(&page_list, node);
				NodeListOp::add(&empty_list, node2);
			}
		}

		for (ThunkNode* ptr = empty_list;
			!NodeListOp::empty(ptr);
			ptr = NodeListOp::next(ptr))
		{
			if( ptr->size == size )
			{
				NodeListOp::remove(&empty_list, ptr);
				NodeListOp::add(&alloc_list, ptr);

				memset(ptr->addr, 0x90, ptr->size);

				return ptr->addr;
			}
			else if( ptr->size > size)
			{
				ThunkNode* ret = new ThunkNode();
				ret->addr = ptr->addr;
				ret->size = size;
				ret->next = NULL;
				ret->prev = NULL;

				NodeListOp::add(&alloc_list, ret);

				ptr->addr += size;
				ptr->size -= size;

				memset(ret->addr, 0x90, ret->size);
				return ret->addr;
			}
		}
	}

	return NULL;
}

void  CThunkMemAllocator::free_thunk(void* p)
{
	CScopedLock lc(&mutex);
	ThunkNode* ptr = NodeListOp::find(alloc_list, p);
	if( ptr )
	{
		NodeListOp::remove(&alloc_list, ptr);
		NodeListOp::add(&empty_list, ptr);
		DoMerge(&empty_list);
	}
}

void CThunkMemAllocator::DoMerge(CThunkMemAllocator::ThunkNode** list_head)
{
	for (ThunkNode* ptr = *list_head;
		!NodeListOp::empty(ptr);
		ptr = NodeListOp::next(ptr))
	{
		while( !NodeListOp::empty( ptr->next ))
		{
			if( ptr->addr + ptr->size == ptr->next->addr )
			{
				ThunkNode* rnode = ptr->next;
				NodeListOp::remove(list_head, rnode);

				ptr->size += rnode->size;
				delete rnode;				
			}
			else
			{
				break;
			}
		}
	}
}

bool CThunkMemAllocator::NodeListOp::empty(CThunkMemAllocator::ThunkNode* list_head)
{
	return !list_head;
}

CThunkMemAllocator::ThunkNode*	CThunkMemAllocator::NodeListOp::next(CThunkMemAllocator::ThunkNode* node)
{
	if( empty(node) )
		return NULL;
	else 
		return node->next;
}

CThunkMemAllocator::ThunkNode*	CThunkMemAllocator::NodeListOp::prev(CThunkMemAllocator::ThunkNode* node)
{
	if( empty(node) )
		return NULL;
	else 
		return node->prev;
}

CThunkMemAllocator::ThunkNode*	CThunkMemAllocator::NodeListOp::find(CThunkMemAllocator::ThunkNode* list_head, void* addr)
{
	for (ThunkNode* ptr = list_head; !empty(ptr); ptr = next(ptr))
	{
		if( ptr->addr == addr )
			return ptr;
	}

	return NULL;
}

void CThunkMemAllocator::NodeListOp::remove(CThunkMemAllocator::ThunkNode** list_head, CThunkMemAllocator::ThunkNode* node)
{
	if( empty(*list_head))
		return ;

	if( *list_head == node )
	{
		*list_head = (*list_head)->next;
		if( !empty(*list_head) )
			(*list_head)->prev = NULL;

		node->next = NULL;
		node->prev = NULL;
	}
	else
	{
		if(empty(node))
			return;

		if( node->prev )
			node->prev->next = node->next;

		if( node->next )
			node->next->prev = node->prev;

		node->next = NULL;
		node->prev = NULL;
	}
}

CThunkMemAllocator::ThunkNode* CThunkMemAllocator::NodeListOp::remove_head(CThunkMemAllocator::ThunkNode** list_head)
{
	if( empty(*list_head ))
		return NULL;
	else
	{
		ThunkNode* ptr = *list_head;

		ptr->next = 0;
		ptr->prev = 0;

		*list_head = (*list_head)->next;

		if( *list_head )
			(*list_head)->prev = NULL;
		return ptr;
	}
}

void CThunkMemAllocator::NodeListOp::add(CThunkMemAllocator::ThunkNode** list_head, CThunkMemAllocator::ThunkNode* node)
{
	ThunkNode* last = NULL;
	ThunkNode* ptr = NULL;

	for (ptr = (*list_head); !empty(ptr); last = ptr, ptr = next(ptr) )
	{
		if( ptr->addr > node->addr )
		{
			break;
		}
	}

	if( !ptr && !last )
	{
		node->next = NULL;
		node->prev = NULL;

		*list_head = node;
	}
	else if( !ptr && last )
	{
		last->next = node;
		node->prev = last;
		node->next = NULL;
	}
	else if( ptr && !last )
	{
		node->prev = NULL;
		node->next = ptr;
		ptr->prev = node;

		*list_head = node;
	}
	else 
	{
		last->next = node;
		ptr->prev = node;

		node->next = ptr;
		node->prev = last;
	}
}

static CThunkMemAllocator g_alloc;

void* gAllocThunkMemory(size_t size)
{
	return g_alloc.alloc_thunk(size);
}

void gFreeThunkMemory(void* p)
{
	g_alloc.free_thunk(p);
}

void ThunkArray::DestroyNode(gpointer data)
{ 
  CThunk* th = reinterpret_cast<CThunk*>(data);
  delete th;
}

ThunkArray CThunk::m_instances;
CCritSec CThunk::m_lockInstances;