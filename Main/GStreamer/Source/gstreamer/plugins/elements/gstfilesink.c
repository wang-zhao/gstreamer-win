/* GStreamer
 * Copyright (C) 1999,2000 Erik Walthinsen <omega@cse.ogi.edu>
 *                    2000 Wim Taymans <wtay@chello.be>
 *                    2006 Wim Taymans <wim@fluendo.com>
 *
 * gstfilesink.c:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
/**
 * SECTION:element-filesink
 * @see_also: #GstFileSrc
 *
 * Write incoming data to a file in the local file system.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch v4l2src num-buffers=1 ! jpegenc ! filesink location=capture1.jpeg
 * ]| Capture one frame from a v4l2 camera and save as jpeg image.
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "../../gst/gst-i18n-lib.h"

#include <gst/gst.h>
#include <stdio.h>              /* for fseeko() */
#ifdef HAVE_STDIO_EXT_H
#include <stdio_ext.h>          /* for __fbufsize, for debugging */
#endif
#include <errno.h>
#include "gstfilesink.h"
#include <string.h>
#include <sys/types.h>

#ifdef G_OS_WIN32
#include <io.h>                 /* lseek, open, close, read */
#undef lseek
#define lseek _lseeki64
#undef off_t
#define off_t guint64
#ifdef _MSC_VER                 /* Check if we are using MSVC, fileno is deprecated in favour */
#define fileno _fileno          /* of _fileno */
#endif
#endif

#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

static GstStaticPadTemplate sinktemplate = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

#define GST_TYPE_BUFFER_MODE (buffer_mode_get_type ())
static GType
buffer_mode_get_type (void)
{
  static GType buffer_mode_type = 0;
  static const GEnumValue buffer_mode[] = {
    {-1, "Default buffering", "default"},
    {_IOFBF, "Fully buffered", "full"},
    {_IOLBF, "Line buffered", "line"},
    {_IONBF, "Unbuffered", "unbuffered"},
    {0, NULL, NULL},
  };

  if (!buffer_mode_type) {
    buffer_mode_type =
        g_enum_register_static ("GstFileSinkBufferMode", buffer_mode);
  }
  return buffer_mode_type;
}

GST_DEBUG_CATEGORY_STATIC (gst_file_sink_debug);
#define GST_CAT_DEFAULT gst_file_sink_debug

#define DEFAULT_LOCATION 	NULL
#define DEFAULT_BUFFER_MODE 	-1
#define DEFAULT_BUFFER_SIZE 	64 * 1024
#define DEFAULT_APPEND		FALSE

enum
{
  PROP_0,
  PROP_LOCATION,
  PROP_BUFFER_MODE,
  PROP_BUFFER_SIZE,
  PROP_APPEND,
  PROP_LAST
};

/* Copy of glib's g_fopen due to win32 libc/cross-DLL brokenness: we can't
 * use the 'file pointer' opened in glib (and returned from this function)
 * in this library, as they may have unrelated C runtimes. */
static FILE *
gst_fopen (const gchar * filename, const gchar * mode)
{
#ifdef G_OS_WIN32
  wchar_t *wfilename = g_utf8_to_utf16 (filename, -1, NULL, NULL, NULL);
  wchar_t *wmode;
  FILE *retval;
  int save_errno;

  if (wfilename == NULL) {
    errno = EINVAL;
    return NULL;
  }

  wmode = g_utf8_to_utf16 (mode, -1, NULL, NULL, NULL);

  if (wmode == NULL) {
    g_free (wfilename);
    errno = EINVAL;
    return NULL;
  }

  retval = _wfopen (wfilename, wmode);
  save_errno = errno;

  g_free (wfilename);
  g_free (wmode);

  errno = save_errno;
  return retval;
#else
  return fopen (filename, mode);
#endif
}


static GstStateChangeReturn
gst_file_sink_change_state (GstElement * element, GstStateChange transition);

static void gst_file_sink_dispose (GObject * object);

static void gst_file_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_file_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_file_sink_open_file (GstFileSink * sink);
static void gst_file_sink_close_file (GstFileSink * sink);

static gboolean gst_file_sink_start (GstBaseSink * sink);
static gboolean gst_file_sink_stop (GstBaseSink * sink);
static gboolean gst_file_sink_event (GstBaseSink * sink, GstEvent * event);
static GstFlowReturn gst_file_sink_render (GstBaseSink * sink,
    GstBuffer * buffer);

static gboolean gst_file_sink_do_seek (GstFileSink * filesink,
    guint64 new_offset);
static gboolean gst_file_sink_get_current_offset (GstFileSink * filesink,
    guint64 * p_pos);

static gboolean gst_file_sink_query (GstBaseSink * bsink, GstQuery * query);

static void gst_file_sink_uri_handler_init (gpointer g_iface,
    gpointer iface_data);


static void
_do_init (GType filesink_type)
{
  static const GInterfaceInfo urihandler_info = {
    gst_file_sink_uri_handler_init,
    NULL,
    NULL
  };

  g_type_add_interface_static (filesink_type, GST_TYPE_URI_HANDLER,
      &urihandler_info);
  GST_DEBUG_CATEGORY_INIT (gst_file_sink_debug, "filesink", 0,
      "filesink element");
}

GST_BOILERPLATE_FULL (GstFileSink, gst_file_sink, GstBaseSink,
    GST_TYPE_BASE_SINK, _do_init);

static void
gst_file_sink_base_init (gpointer g_class)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (g_class);

  gst_element_class_set_details_simple (gstelement_class,
      "File Sink",
      "Sink/File", "Write stream to a file",
      "Thomas Vander Stichele <thomas at apestaart dot org>");
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sinktemplate));
}

static void
gst_file_sink_class_init (GstFileSinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseSinkClass *gstbasesink_class = GST_BASE_SINK_CLASS (klass);

  gobject_class->dispose = gst_file_sink_dispose;

  gobject_class->set_property = gst_file_sink_set_property;
  gobject_class->get_property = gst_file_sink_get_property;
  
  gstbasesink_class->parent_class.change_state =
	  GST_DEBUG_FUNCPTR (gst_file_sink_change_state);

  g_object_class_install_property (gobject_class, PROP_LOCATION,
      g_param_spec_string ("location", "File Location",
          "Location of the file to write", NULL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_BUFFER_MODE,
      g_param_spec_enum ("buffer-mode", "Buffering mode",
          "The buffering mode to use", GST_TYPE_BUFFER_MODE,
          DEFAULT_BUFFER_MODE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_BUFFER_SIZE,
      g_param_spec_uint ("buffer-size", "Buffering size",
          "Size of buffer in number of bytes for line or full buffer-mode", 0,
          G_MAXUINT, DEFAULT_BUFFER_SIZE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstFileSink:append
   * 
   * Append to an already existing file.
   *
   * Since: 0.10.25
   */
  g_object_class_install_property (gobject_class, PROP_APPEND,
      g_param_spec_boolean ("append", "Append",
          "Append to an already existing file", DEFAULT_APPEND,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gstbasesink_class->start = GST_DEBUG_FUNCPTR (gst_file_sink_start);
  gstbasesink_class->stop = GST_DEBUG_FUNCPTR (gst_file_sink_stop);
  gstbasesink_class->query = GST_DEBUG_FUNCPTR (gst_file_sink_query);
  gstbasesink_class->render = GST_DEBUG_FUNCPTR (gst_file_sink_render);
  gstbasesink_class->event = GST_DEBUG_FUNCPTR (gst_file_sink_event);

  if (sizeof (off_t) < 8) {
    GST_LOG ("No large file support, sizeof (off_t) = %" G_GSIZE_FORMAT "!",
        sizeof (off_t));
  }
}

static void
gst_file_sink_init (GstFileSink * filesink, GstFileSinkClass * g_class)
{
  filesink->filename = NULL;
  filesink->file = NULL;
  filesink->buffer_mode = DEFAULT_BUFFER_MODE;
  filesink->buffer_size = DEFAULT_BUFFER_SIZE;
  filesink->buffer = NULL;
  filesink->append = FALSE;

  filesink->buffer_cache = gst_data_cache_new(2*1024*1024);

  gst_base_sink_set_sync (GST_BASE_SINK (filesink), FALSE);
}

static void
gst_file_sink_dispose (GObject * object)
{
  GstFileSink *sink = GST_FILE_SINK (object);

  G_OBJECT_CLASS (parent_class)->dispose (object);

  g_free (sink->uri);
  sink->uri = NULL;
  g_free (sink->filename);
  sink->filename = NULL;
  g_free (sink->buffer);
  sink->buffer = NULL;
  sink->buffer_size = 0;
  g_object_unref(sink->buffer_cache);
  sink->buffer_cache= 0;

}

static gboolean
gst_file_sink_set_location (GstFileSink * sink, const gchar * location)
{
  if (sink->file)
    goto was_open;

  g_free (sink->filename);
  g_free (sink->uri);
  if (location != NULL) {
    /* we store the filename as we received it from the application. On Windows
     * this should be in UTF8 */
    sink->filename = g_strdup (location);
    sink->uri = gst_filename_to_uri (location, NULL);
    GST_INFO ("filename : %s", sink->filename);
    GST_INFO ("uri      : %s", sink->uri);
  } else {
    sink->filename = NULL;
    sink->uri = NULL;
  }

  return TRUE;

  /* ERRORS */
was_open:
  {
    g_warning ("Changing the `location' property on filesink when a file is "
        "open is not supported.");
    return FALSE;
  }
}

static void
gst_file_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstFileSink *sink = GST_FILE_SINK (object);

  switch (prop_id) {
    case PROP_LOCATION:
      gst_file_sink_set_location (sink, g_value_get_string (value));
      break;
    case PROP_BUFFER_MODE:
      sink->buffer_mode = g_value_get_enum (value);
      break;
    case PROP_BUFFER_SIZE:
      sink->buffer_size = g_value_get_uint (value);
      break;
    case PROP_APPEND:
      sink->append = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_file_sink_get_property (GObject * object, guint prop_id, GValue * value,
    GParamSpec * pspec)
{
  GstFileSink *sink = GST_FILE_SINK (object);

  switch (prop_id) {
    case PROP_LOCATION:
      g_value_set_string (value, sink->filename);
      break;
    case PROP_BUFFER_MODE:
      g_value_set_enum (value, sink->buffer_mode);
      break;
    case PROP_BUFFER_SIZE:
      g_value_set_uint (value, sink->buffer_size);
      break;
    case PROP_APPEND:
      g_value_set_boolean (value, sink->append);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_file_sink_open_file (GstFileSink * sink)
{
  gint mode;

  /* open the file */
  if (sink->filename == NULL || sink->filename[0] == '\0')
    goto no_filename;

  if (sink->append)
    sink->file = gst_fopen (sink->filename, "ab");
  else
    sink->file = gst_fopen (sink->filename, "wb");
  if (sink->file == NULL)
    goto open_failed;

  /* see if we are asked to perform a specific kind of buffering */
  if ((mode = sink->buffer_mode) != -1) {
    guint buffer_size;

    /* free previous buffer if any */
    g_free (sink->buffer);

    if (mode == _IONBF) {
      /* no buffering */
      sink->buffer = NULL;
      buffer_size = 0;
    } else {
      /* allocate buffer */
      sink->buffer = g_malloc (sink->buffer_size);
      buffer_size = sink->buffer_size;
    }
#ifdef HAVE_STDIO_EXT_H
    GST_DEBUG_OBJECT (sink, "change buffer size %u to %u, mode %d",
        (guint) __fbufsize (sink->file), buffer_size, mode);
#else
    GST_DEBUG_OBJECT (sink, "change  buffer size to %u, mode %d",
        sink->buffer_size, mode);
#endif
    if (setvbuf (sink->file, sink->buffer, mode, buffer_size) != 0) {
      GST_WARNING_OBJECT (sink, "warning: setvbuf failed: %s",
          g_strerror (errno));
    }
  }

  sink->current_pos = 0;
  /* try to seek in the file to figure out if it is seekable */
  sink->seekable = gst_file_sink_do_seek (sink, 0);

  GST_DEBUG_OBJECT (sink, "opened file %s, seekable %d",
      sink->filename, sink->seekable);

  return TRUE;

  /* ERRORS */
no_filename:
  {
    GST_ELEMENT_ERROR (sink, RESOURCE, NOT_FOUND,
        (_("No file name specified for writing.")), (NULL));
    return FALSE;
  }
open_failed:
  {
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE,
        (_("Could not open file \"%s\" for writing."), sink->filename),
        GST_ERROR_SYSTEM);
    return FALSE;
  }
}

static void
gst_file_sink_close_file (GstFileSink * sink)
{
  if (sink->file) {
    if (fclose (sink->file) != 0)
      goto close_failed;

    GST_DEBUG_OBJECT (sink, "closed file");
    sink->file = NULL;

    g_free (sink->buffer);
    sink->buffer = NULL;
  }
  return;

  /* ERRORS */
close_failed:
  {
    GST_ELEMENT_ERROR (sink, RESOURCE, CLOSE,
        (_("Error closing file \"%s\"."), sink->filename), GST_ERROR_SYSTEM);
    return;
  }
}

static gboolean
gst_file_sink_query (GstBaseSink * bsink, GstQuery * query)
{
  gboolean res;
  GstFileSink *self;
  GstFormat format;

  self = GST_FILE_SINK (bsink);

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_POSITION:
      gst_query_parse_position (query, &format, NULL);

      switch (format) {
        case GST_FORMAT_DEFAULT:
        case GST_FORMAT_BYTES:
          gst_query_set_position (query, GST_FORMAT_BYTES, self->current_pos);
          res = TRUE;
          break;
        default:
          res = FALSE;
          break;
      }
      break;

    case GST_QUERY_FORMATS:
      gst_query_set_formats (query, 2, GST_FORMAT_DEFAULT, GST_FORMAT_BYTES);
      res = TRUE;
      break;

    case GST_QUERY_URI:
      gst_query_set_uri (query, self->uri);
      res = TRUE;
      break;

    default:
      res = GST_BASE_SINK_CLASS (parent_class)->query (bsink, query);
      break;
  }
  return res;
}

#ifdef HAVE_FSEEKO
# define __GST_STDIO_SEEK_FUNCTION "fseeko"
#elif defined (G_OS_UNIX) 
# define __GST_STDIO_SEEK_FUNCTION "lseek"
#elif defined (G_OS_WIN32)
# define __GST_STDIO_SEEK_FUNCTION "_myfseeki64"

#include <Windows.h>

#define myinuse(s)        ((s)->_flag & (_IOREAD|_IOWRT|_IORW))
typedef struct {
	FILE f;
	CRITICAL_SECTION lock;
}   _MYFILEX;

void __cdecl _lock (
					int locknum
					);
void __cdecl _unlock (
					int locknum
					);

void __cdecl _mylock_file (
        FILE *pf
        )
{
        /*
         * The way the FILE (pointed to by pf) is locked depends on whether
         * it is part of _iob[] or not
         */
        if ( (pf >= _iob) && (pf <= (&_iob[_IOB_ENTRIES-1])) )
        {
            /*
             * FILE lies in _iob[] so the lock lies in _locktable[].
             */
            _lock( 16 + (int)(pf - _iob) );
            /* We set _IOLOCKED to indicate we locked the stream */
            pf->_flag |= 0x8000;
        }
        else
            /*
             * Not part of _iob[]. Therefore, *pf is a _FILEX and the
             * lock field of the struct is an initialized critical
             * section.
             */
            EnterCriticalSection( &(((_MYFILEX *)pf)->lock) );
}

void __cdecl _myunlock_file (
        FILE *pf
        )
{
        /*
         * The way the FILE (pointed to by pf) is unlocked depends on whether
         * it is part of _iob[] or not
         */
        if ( (pf >= _iob) && (pf <= (&_iob[_IOB_ENTRIES-1])) )
        {
            /*
             * FILE lies in _iob[] so the lock lies in _locktable[].
             * We reset _IOLOCKED to indicate we unlock the stream.
             */
             pf->_flag &= ~0x8000;
            _unlock( 16 + (int)(pf - _iob) );
        }
        else
            /*
             * Not part of _iob[]. Therefore, *pf is a _FILEX and the
             * lock field of the struct is an initialized critical
             * section.
             */
            LeaveCriticalSection( &(((_MYFILEX *)pf)->lock) );
}


int __cdecl _myflush (
        FILE *str
        )
{
        register FILE *stream;
        register int rc = 0; /* assume good return */
        register int nchar;

        /* Init pointer to stream */
        stream = str;


        if ((stream->_flag & (_IOREAD | _IOWRT)) == _IOWRT && ((stream)->_flag & (_IOMYBUF|0x0100))
                && (nchar = (int)(stream->_ptr - stream->_base)) > 0)
        {
                if ( _write(_fileno(stream), stream->_base, nchar) == nchar ) {
                        /* if this is a read/write file, clear _IOWRT so that
                         * next operation can be a read
                         */
                        if ( _IORW & stream->_flag )
                                stream->_flag &= ~_IOWRT;
                }
                else {
                        stream->_flag |= _IOERR;
                        rc = EOF;
                }
        }

        stream->_ptr = stream->_base;
        stream->_cnt = 0;

        return(rc);
}

int __cdecl _myfseeki64_nolock (

        FILE *str,
        __int64 offset,
        int whence
        )
{


        register FILE *stream;

        /* Init stream pointer */
        stream = str;

        if ( !myinuse(stream) || ((whence != SEEK_SET) && (whence != SEEK_CUR) &&
            (whence != SEEK_END)) ) {
                errno=EINVAL;
                return(-1);
        }

        /* Clear EOF flag */

        stream->_flag &= ~_IOEOF;

        /* If seeking relative to current location, then convert to
           a seek relative to beginning of file.  This accounts for
           buffering, etc. by letting fseek() tell us where we are. */

		_myflush(stream);

        if (whence == SEEK_CUR) {
                offset += _telli64(fileno(stream));
                whence = SEEK_SET;
        }

        /* If file opened for read/write, clear flags since we don't know
           what the user is going to do next. If the file was opened for
           read access only, decrease _bufsiz so that the next _filbuf
           won't cost quite so much */

        if (stream->_flag & _IORW)
                stream->_flag &= ~(_IOWRT|_IOREAD);
        else if ( (stream->_flag & _IOREAD) && (stream->_flag & _IOMYBUF) &&
                  !(stream->_flag & 0x0400) )
                stream->_bufsiz = 512;

        /* Seek to the desired locale and return. */

        return(_lseeki64(_fileno(stream), offset, whence) == -1i64 ? -1 : 0);
}

int __cdecl _myfseeki64 (
					   FILE *stream,
					   __int64 offset,
					   int whence
					   )
{
	int retval;

	_mylock_file(stream);

	__try {
		retval = _myfseeki64_nolock (stream, offset, whence);
	}
	__finally {
		_myunlock_file(stream);
	}

	return(retval);
}

#else
# define __GST_STDIO_SEEK_FUNCTION "fseek"
#endif
static gboolean 
gst_file_sink_write_through(GstFileSink * filesink, guint* psize)
{
	gboolean ret = TRUE;
	guint size = 0;
	guint8 *data = NULL;

	if( psize )
		*psize = 0;

	if(( data = gst_data_cache_lock(filesink->buffer_cache, &size)) != 0 )
	{
		if(1 != fwrite(data, size, 1, filesink->file) )
		{
			ret = FALSE;
		}
		else
		{
			if( psize )
				*psize = size;
		}
		gst_data_cache_unlock(filesink->buffer_cache, size);

		if (fflush (filesink->file))
			ret = FALSE;
	}  
	gst_data_cache_clear(filesink->buffer_cache);

	return ret;
}
static gboolean
gst_file_sink_do_seek (GstFileSink * filesink, guint64 new_offset)
{
  GST_DEBUG_OBJECT (filesink, "Seeking to offset %" G_GUINT64_FORMAT
      " using " __GST_STDIO_SEEK_FUNCTION, new_offset);

  if(!gst_file_sink_write_through(filesink, 0))
	  goto flush_failed;

#ifdef HAVE_FSEEKO
  if (fseeko (filesink->file, (off_t) new_offset, SEEK_SET) != 0)
	  goto seek_failed;
#elif defined (G_OS_UNIX) 
  if (lseek (fileno (filesink->file), (off_t) new_offset,
	  SEEK_SET) == (off_t) - 1)
	  goto seek_failed;
#elif defined (G_OS_WIN32)
  
  if (_myfseeki64(filesink->file, new_offset,SEEK_SET) != 0)
	  goto seek_failed;
#else
  if (fseek (filesink->file, (long) new_offset, SEEK_SET) != 0)
    goto seek_failed;
#endif

  /* adjust position reporting after seek;
   * presumably this should basically yield new_offset */
  gst_file_sink_get_current_offset (filesink, &filesink->current_pos);

  return TRUE;

  /* ERRORS */
flush_failed:
  {
    GST_DEBUG_OBJECT (filesink, "Flush failed: %s", g_strerror (errno));
    return FALSE;
  }
seek_failed:
  {
    GST_DEBUG_OBJECT (filesink, "Seeking failed: %s", g_strerror (errno));
    return FALSE;
  }
}

/* handle events (search) */
static gboolean
gst_file_sink_event (GstBaseSink * sink, GstEvent * event)
{
  GstEventType type;
  GstFileSink *filesink;

  filesink = GST_FILE_SINK (sink);

  type = GST_EVENT_TYPE (event);

  switch (type) {
    case GST_EVENT_NEWSEGMENT:
    {
      gint64 start, stop, pos;
      GstFormat format;

      gst_event_parse_new_segment (event, NULL, NULL, &format, &start,
          &stop, &pos);

      if (format == GST_FORMAT_BYTES) {
        /* only try to seek and fail when we are going to a different
         * position */
          if (filesink->current_pos + gst_data_cache_available(filesink->buffer_cache) != start) {
            /* FIXME, the seek should be performed on the pos field, start/stop are
            * just boundaries for valid bytes offsets. We should also fill the file
            * with zeroes if the new position extends the current EOF (sparse streams
            * and segment accumulation). */
            if (!gst_file_sink_do_seek (filesink, (guint64) start))
              goto seek_failed;
          } else {
            GST_DEBUG_OBJECT (filesink, "Ignored NEWSEGMENT, no seek needed");
          }
        } else {
          GST_DEBUG_OBJECT (filesink,
            "Ignored NEWSEGMENT event of format %u (%s)", (guint) format,
            gst_format_get_name (format));
      }
      break;
    }
    case GST_EVENT_EOS:
      {
        guint csize = 0;

        if(!gst_file_sink_write_through(filesink, &csize))
          goto flush_failed;

        filesink->current_pos += csize;
      }
      break;
    default:
      break;
  }

  return TRUE;

  /* ERRORS */
seek_failed:
  {
    GST_ELEMENT_ERROR (filesink, RESOURCE, SEEK,
        (_("Error while seeking in file \"%s\"."), filesink->filename),
        GST_ERROR_SYSTEM);
    return FALSE;
  }
flush_failed:
  {
    GST_ELEMENT_ERROR (filesink, RESOURCE, WRITE,
        (_("Error while writing to file \"%s\"."), filesink->filename),
        GST_ERROR_SYSTEM);
    return FALSE;
  }
}

static gboolean
gst_file_sink_get_current_offset (GstFileSink * filesink, guint64 * p_pos)
{
  off_t ret = -1;

#ifdef HAVE_FTELLO
  ret = ftello (filesink->file);
#elif defined (G_OS_UNIX) 
  if (fflush (filesink->file)) {
    GST_DEBUG_OBJECT (filesink, "Flush failed: %s", g_strerror (errno));
    /* ignore and continue */
  }
  ret = lseek(fileno(filesink->file), 0, SEEK_CUR);
#elif defined (G_OS_WIN32) 
  if (fflush (filesink->file)) {
	  GST_DEBUG_OBJECT (filesink, "Flush failed: %s", g_strerror (errno));
	  /* ignore and continue */
  }
  ret = _telli64(_fileno(filesink->file));
#else
  ret = (off_t) ftell (filesink->file);
#endif

  if (ret != (off_t) - 1)
    *p_pos = (guint64) ret;

  return (ret != (off_t) - 1);
}

static GstFlowReturn
gst_file_sink_render (GstBaseSink * sink, GstBuffer * buffer)
{
  GstFileSink *filesink;
  guint size;
  guint bsize;
  guint8* bdata;
  guint8 *data;

#ifdef _DEBUG
  static __time64_t timer = 0;
  static __time64_t timer2 = 0;
  __time64_t start = 0;
  __time64_t stop = 0;
#endif


  filesink = GST_FILE_SINK (sink);

  size = GST_BUFFER_SIZE (buffer);
  data = GST_BUFFER_DATA (buffer);

  GST_DEBUG_OBJECT (filesink, "writing %u bytes at %" G_GUINT64_FORMAT,
      size, filesink->current_pos);

  if (size > 0 && data != NULL) 
  {
    // 如果cache已经满了，去除cache中的数据，写入文件
    if( gst_data_cache_push(filesink->buffer_cache, buffer) <= 0)
    {
      bsize = -1;
      if((bdata = gst_data_cache_lock(filesink->buffer_cache, &bsize )) != 0)
      {
        // 4K 对齐
        if(bsize > 4096)
          bsize -= (filesink->current_pos + bsize) % 4096;
#ifdef _DEBUG
        _time64(&start);
#endif
        if(fwrite(bdata,bsize,1, filesink->file) != 1)
        {
          gst_data_cache_unlock(filesink->buffer_cache, bsize);
          goto handle_error;
        }

#ifdef _DEBUG
        _time64(&stop);
#endif

        gst_data_cache_unlock(filesink->buffer_cache, bsize);

        // 如果cache容量太小 直接写入文件
        if( gst_data_cache_push(filesink->buffer_cache, buffer) <= 0 )
        {
          // 将缓存全部写入
          guint csize = 0;          
          gst_file_sink_write_through(filesink, &csize);
          bsize += csize;

          if (fwrite (data, size, 1, filesink->file) != 1)
            goto handle_error;

#ifdef _DEBUG
          _time64(&stop);
#endif
          size += bsize;
        }
        else
        {
          size = bsize;
        }
      }
      else
      {

#ifdef _DEBUG
        _time64(&start);
#endif
        if (fwrite (data, size, 1, filesink->file) != 1)
          goto handle_error;
#ifdef _DEBUG
        _time64(&stop);
#endif
      }
    }
    else
    {
      size = 0;
    }

    filesink->current_pos += size;
  }

#ifdef _DEBUG
  timer += stop - start;

  if(timer2 != timer )
  {
    printf("\trender timer %" G_GINT64_FORMAT "\r\n", timer);
    timer2 = timer;
  }

#endif


  return GST_FLOW_OK;

handle_error:
  {
    switch (errno) {
      case ENOSPC:{
        GST_ELEMENT_ERROR (filesink, RESOURCE, NO_SPACE_LEFT, (NULL), (NULL));
        break;
      }
      default:{
        GST_ELEMENT_ERROR (filesink, RESOURCE, WRITE,
            (_("Error while writing to file \"%s\"."), filesink->filename),
            ("%s", g_strerror (errno)));
      }
    }
    return GST_FLOW_ERROR;
  }
}

static gboolean
gst_file_sink_start (GstBaseSink * basesink)
{
  GstFileSink* filesink = GST_FILE_SINK(basesink);
  gst_data_cache_clear(filesink->buffer_cache);
  return gst_file_sink_open_file (filesink);
}

static gboolean
gst_file_sink_stop (GstBaseSink * basesink)
{
  guint csize = 0;

  GstFileSink* filesink = GST_FILE_SINK(basesink);

  if( gst_file_sink_write_through(filesink, &csize))
  {
    filesink->current_pos += csize;
  }

  gst_file_sink_close_file (filesink);
  return TRUE;
}

/*** GSTURIHANDLER INTERFACE *************************************************/

static GstURIType
gst_file_sink_uri_get_type (void)
{
  return GST_URI_SINK;
}

static gchar **
gst_file_sink_uri_get_protocols (void)
{
  static gchar *protocols[] = { (char *) "file", NULL };

  return protocols;
}

static const gchar *
gst_file_sink_uri_get_uri (GstURIHandler * handler)
{
  GstFileSink *sink = GST_FILE_SINK (handler);

  return sink->uri;
}

static gboolean
gst_file_sink_uri_set_uri (GstURIHandler * handler, const gchar * uri)
{
  gchar *protocol, *location;
  gboolean ret;
  GstFileSink *sink = GST_FILE_SINK (handler);

  protocol = gst_uri_get_protocol (uri);
  if (strcmp (protocol, "file") != 0) {
    g_free (protocol);
    return FALSE;
  }
  g_free (protocol);

  /* allow file://localhost/foo/bar by stripping localhost but fail
   * for every other hostname */
  if (g_str_has_prefix (uri, "file://localhost/")) {
    char *tmp;

    /* 16 == strlen ("file://localhost") */
    tmp = g_strconcat ("file://", uri + 16, NULL);
    /* we use gst_uri_get_location() although we already have the
     * "location" with uri + 16 because it provides unescaping */
    location = gst_uri_get_location (tmp);
    g_free (tmp);
  } else if (strcmp (uri, "file://") == 0) {
    /* Special case for "file://" as this is used by some applications
     *  to test with gst_element_make_from_uri if there's an element
     *  that supports the URI protocol. */
    gst_file_sink_set_location (sink, NULL);
    return TRUE;
  } else {
    location = gst_uri_get_location (uri);
  }

  if (!location)
    return FALSE;
  if (!g_path_is_absolute (location)) {
    g_free (location);
    return FALSE;
  }

  ret = gst_file_sink_set_location (sink, location);
  g_free (location);

  return ret;
}

static void
gst_file_sink_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

  iface->get_type = gst_file_sink_uri_get_type;
  iface->get_protocols = gst_file_sink_uri_get_protocols;
  iface->get_uri = gst_file_sink_uri_get_uri;
  iface->set_uri = gst_file_sink_uri_set_uri;
}

extern gboolean
gst_element_pads_activate_extern(GstElement * element, gboolean active);

static GstStateChangeReturn 
gst_file_sink_change_state(GstElement * element, GstStateChange transition)
{  
	GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
	GstFileSink *basesink = GST_FILE_SINK (element);
	GstFileSinkClass *bclass = GST_FILE_SINK_GET_CLASS (basesink);

	{
		GstStateChangeReturn bret;

		bret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
		if (G_UNLIKELY (bret == GST_STATE_CHANGE_FAILURE))
			goto activate_failed;
		ret = bret;
	}
	switch(transition)
	{
	case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
		break;
	case GST_STATE_CHANGE_NULL_TO_READY:
	case GST_STATE_CHANGE_PAUSED_TO_READY:
		
		gst_element_pads_activate_extern (element, TRUE);
		basesink->parent.need_preroll = FALSE;

		break;
	}
	return ret;

activate_failed:
	{
		GST_DEBUG_OBJECT (element,
			"element failed to change states -- activation problem?");
		return GST_STATE_CHANGE_FAILURE;
	}

}