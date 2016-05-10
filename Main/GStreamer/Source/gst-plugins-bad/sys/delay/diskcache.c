#include "diskcache.h"
#include <glib/gstdio.h>

typedef struct _DiskChunk DiskChunk;

struct _DiskChunk
{
  gint64 start_pos;
  gint64 size_in_bytes;
  gboolean in_use;
};

struct _DiskCache
{
  int cache_file;
  GList* chunk_list;
  gint64 total_size;
  GMutex lock;
};

static int gst_disk_cache_new_file()
{
  const gchar* dir;
  gchar* tmp_name;
  int fd;
  
  
  dir = g_getenv("GST_DISK_CACHE_DIR");
  if ( !dir )
  {
    dir = g_get_tmp_dir();
  }

  tmp_name = g_strdup_printf ("%s\\gst_disk_cache.XXXXXX", dir);
  fd = g_mkstemp_full(tmp_name, 0x8042  /*O_RDWR | _O_BINARY | _O_TEMPORARY*/, _S_IREAD | _S_IWRITE); 

  g_free(tmp_name);
  return fd;
}

static void gst_disk_cache_free_file(int f)
{
  g_close(f);
}

static gboolean gst_disk_cache_read_file(int file, gpointer data, gint64 start, glong size)
{
  g_lseek64(file, start, SEEK_SET);
  return size == g_read(file, data, size);
}

static gboolean gst_disk_cache_write_file(int file, gint64 pos, gpointer data, glong size)
{
  g_lseek64(file, pos, SEEK_SET);

  return size == g_write(file, data, size);
}

static gint64 gst_disk_cache_append_file(int file, gpointer data, glong size)
{
  gint64 pos = g_lseek64(file, 0, SEEK_END);
  if(size == g_write(file, data, size))
    return pos;
  else
    return -1;
}

static void gst_free_chunk(gpointer data)
{
  g_slice_free(DiskChunk, data);
}

static void gst_disk_cache_free_chunk_list(GList* clist)
{
  g_list_free_full(clist, gst_free_chunk);
}

static void gst_disk_cache_merge_chunk(GList** clist)
{  
  GList* tmp;
  DiskChunk* dc1, *dc2 ;
  tmp = *clist;

  while(tmp)
  {
    if( tmp->next)
    {
      dc1 = (DiskChunk*)(tmp->data);
      dc2 = (DiskChunk*)(tmp->next->data);
      if( dc1->in_use == dc2->in_use &&
        dc1->start_pos + dc1->size_in_bytes == dc2->start_pos)
      {
        dc2->start_pos = dc1->start_pos;
        dc2->size_in_bytes += dc1->size_in_bytes;

        tmp = g_list_next(tmp);
        *clist = g_list_remove_link(*clist, tmp->prev);
        g_slice_free(DiskChunk,dc1);
        continue;
      }
    }

    tmp = g_list_next(tmp);
  }
}

static void gst_disk_cache_add_data_chunk(GList ** clist, DiskChunk* dchk)
{
  GList* tmp;
  DiskChunk* dc ;
  tmp = *clist;

  while(tmp)
  {
    dc = (DiskChunk*)(tmp->data);
    if( dc->start_pos > dchk->start_pos )
      break;

    tmp = g_list_next(tmp);
  } 
  *clist = g_list_insert_before(*clist, tmp, dchk);

  gst_disk_cache_merge_chunk(clist);
}

static void gst_disk_cache_op_chunk(GList** clist, DiskChunk* dchk, gint64 chunk_offset, glong size, gboolean use)
{
  GList* theone;
  DiskChunk* newchk;

  theone = g_list_find(*clist, dchk);

  if (!theone)
    return ;

  g_assert(dchk->in_use == !use);

  if( chunk_offset > 0 )
  {
    newchk = g_slice_alloc0(sizeof(DiskChunk));
    newchk->start_pos = dchk->start_pos;
    newchk->size_in_bytes = chunk_offset;
    newchk->in_use = !use;
    *clist = g_list_insert_before(*clist, theone,newchk);

    dchk->start_pos += chunk_offset;
    dchk->size_in_bytes -= chunk_offset;
  }

  dchk->in_use = use;

  if( dchk->size_in_bytes > size )
  {
    newchk = g_slice_alloc0(sizeof(DiskChunk));
    newchk->start_pos = dchk->start_pos+size;
    newchk->size_in_bytes = dchk->size_in_bytes-size;
    newchk->in_use = !use;
    *clist = g_list_insert_before(*clist, theone->next, newchk);
  }

  dchk->size_in_bytes = size;

  gst_disk_cache_merge_chunk(clist);

}
static void gst_disk_cache_use_chunk(GList** clist, DiskChunk* dchk, gint64 chunk_offset, glong size)
{
  gst_disk_cache_op_chunk(clist, dchk, chunk_offset, size, TRUE);
}

static void gst_disk_cache_unuse_chunk(GList** clist, DiskChunk* dchk, gint64 chunk_offset, glong size)
{
  gst_disk_cache_op_chunk(clist, dchk, chunk_offset, size, FALSE);
}

static DiskChunk* gst_disk_cache_find_free_chunk(GList* clist, glong size)
{
  GList* tmp;
  DiskChunk* dc ;
  tmp = clist;

  while(tmp)
  {
    dc = (DiskChunk*)(tmp->data);
    if( dc->in_use == FALSE && dc->size_in_bytes >= size)
      return dc;

    tmp = g_list_next(tmp);
  }
  return NULL;
}

static DiskChunk* gst_disk_cache_find_data_chunk(GList* clist, gint64 start, glong size)
{
  GList* tmp;
  DiskChunk* dc ;
  tmp = clist;

  while(tmp)
  {
    dc = (DiskChunk*)(tmp->data);
    if( dc->in_use == TRUE &&
      dc->start_pos <= start && 
      dc->start_pos + dc->size_in_bytes >= start + size)
      return dc;

    if( dc->start_pos > start)
      break;

    tmp = g_list_next(tmp);
  }
  return NULL;
}

DiskCache* gst_disk_cache_new()
{
  DiskCache* r = NULL;

  r = g_malloc0(sizeof (DiskCache));

  if( r )
  {
    r->cache_file = gst_disk_cache_new_file();
    r->chunk_list = 0;
    r->total_size = 0;
    g_mutex_init(&r->lock);
  }
  return r;
}

 void
__stdcall
OutputDebugStringA(
                   gchar* lpOutputString
                   );


gint64 gst_disk_cache_add_data(DiskCache* dc, gpointer data, glong size)
{
  DiskChunk* dchk = NULL;

  g_mutex_lock(&dc->lock);

  dchk = gst_disk_cache_find_free_chunk(dc->chunk_list, size);
  if( !dchk )
  {
    gint64 s = gst_disk_cache_append_file(dc->cache_file, data, size);
#ifdef _DEBUG
    OutputDebugStringA("alloc new chunk\r\n");
#endif
    if( s >= 0 )
    {
      dchk = g_slice_alloc0(sizeof (DiskChunk));
      dchk->in_use = TRUE;
      dchk->size_in_bytes = size;
      dchk->start_pos = s;
      dc->total_size = s + size;
      gst_disk_cache_add_data_chunk(&dc->chunk_list, dchk);
      g_mutex_unlock(&dc->lock);
      return s;
    }
    else
    {
      g_mutex_unlock(&dc->lock);
      return -1;
    }
  }
  else
  {
    gint64 pos = dchk->start_pos;
#ifdef _DEBUG
    OutputDebugStringA("reuse free chunk\r\n");
#endif
    if(gst_disk_cache_write_file(dc->cache_file,pos, data, size))
    {
      gst_disk_cache_use_chunk(&dc->chunk_list, dchk, 0, size);
      g_mutex_unlock(&dc->lock);
      return pos;
    }
    else
    {
      g_mutex_unlock(&dc->lock);
      return -1;
    }
  }
}

gboolean gst_disk_cache_remove_data(DiskCache* dc, gpointer data, gint64 start, glong size)
{
  DiskChunk* dchk = NULL;

  g_mutex_lock(&dc->lock);
  dchk = gst_disk_cache_find_data_chunk(dc->chunk_list, start, size);
  if (dchk )
  {
    gboolean ret = FALSE;
    if(gst_disk_cache_read_file(dc->cache_file, data, start, size))
    {
      ret = TRUE;
    }
    gst_disk_cache_unuse_chunk(&dc->chunk_list, dchk, start-dchk->start_pos, size);

    g_mutex_unlock(&dc->lock);
    return ret;
  }
  else
  {
    g_mutex_unlock(&dc->lock);
    return FALSE;
  }
}

void gst_disk_cache_free(DiskCache* dc)
{
  if( dc )
  {
    g_mutex_lock(&dc->lock);

    gst_disk_cache_free_file(dc->cache_file);
    gst_disk_cache_free_chunk_list(dc->chunk_list);

    g_mutex_unlock(&dc->lock);

    g_mutex_clear(&dc->lock);
    g_free(dc);
  }
}