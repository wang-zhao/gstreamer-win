/* GStreamer
 *
 * Copyright (C) 2009 Wim Taymans <wim.taymans@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <windows.h>

#define NUMBER_OF_SIMULTANEOUS_PIPELINES 20
#define NUMBER_OF_SIMULTANEOUS_THREADS 4

typedef struct _App App;
typedef struct _ThreadInfo ThreadInfo;

struct _App
{
  GstElement** pipelines;
  int pipeline_count;

  GThread** threads;
  int thread_count;

  GMainLoop *loop;
};

struct _ThreadInfo 
{
  App* app;

  int thread_index;

  GstElement* pipelines[NUMBER_OF_SIMULTANEOUS_PIPELINES / NUMBER_OF_SIMULTANEOUS_THREADS];
  int pipeline_count;
};

App s_app;

static void
test_thread (ThreadInfo* ti)
{
	int i;
  int index;
  GstElement* pipeline;

	for(i = 0; i < 300; ++i) {
    
    //g_print ("Playing...\n");

    for(index = 0; index < ti->pipeline_count; ++index) {
      pipeline = ti->pipelines[index];
      //g_print ("Setting pipeline %d to playing...", pipeline);
		  gst_element_set_state (pipeline, GST_STATE_PLAYING);
    }

    Sleep(100);
    
    //g_print ("Null...\n");

    for(index = 0; index < ti->pipeline_count; ++index) {
      pipeline = ti->pipelines[index];
		  gst_element_set_state (pipeline, GST_STATE_NULL);
    }
	}

  g_print ("Thread %d completed\n", ti->thread_index);
  for(index = 0; index < ti->pipeline_count; ++index)
	  gst_object_unref (ti->pipelines[index]);

  g_main_loop_quit(ti->app->loop);
}

int
main (int argc, char *argv[])
{
  int index;
  App *app = &s_app;
  GThread* threads[NUMBER_OF_SIMULTANEOUS_THREADS];
  ThreadInfo threadInfos[NUMBER_OF_SIMULTANEOUS_THREADS];
  GstElement* pipelines[NUMBER_OF_SIMULTANEOUS_PIPELINES];

  app->threads = threads;
  app->thread_count = NUMBER_OF_SIMULTANEOUS_THREADS;

  app->pipelines = pipelines;
  app->pipeline_count = NUMBER_OF_SIMULTANEOUS_PIPELINES;

  gst_init (&argc, &argv);

  /* create a mainloop to get messages */
  app->loop = g_main_loop_new (NULL, TRUE);

  for(index = 0; index < app->pipeline_count; ++index)
  {
    GstElement* pipeline;
    GstElement* videotestsrc;
    GstElement* ffmpegcolorspace;
    GstElement* videosink;

    g_print ("creating elements\n");

    pipeline = app->pipelines[index] = gst_pipeline_new("pipeline");
    g_assert (pipeline);
    
    videotestsrc = gst_element_factory_make ("videotestsrc", "videotestsrc");
    g_assert (videotestsrc);

    ffmpegcolorspace = gst_element_factory_make ("ffmpegcolorspace", "ffmpegcolorspace");
    g_assert (ffmpegcolorspace);

    videosink = gst_element_factory_make ("autovideosink", "videosink");
    g_assert (videosink);

    gst_bin_add_many(GST_BIN(pipeline), videotestsrc, ffmpegcolorspace, videosink, NULL);
    gst_element_link_many(videotestsrc, ffmpegcolorspace, videosink, NULL);

    g_print ("elements added and linked\n");
  }

  for(index = 0; index < app->thread_count; ++index)
  {
    g_print ("Clearing memory for thread %d\n", index);
    threadInfos[index].pipeline_count = 0;
  }

  /* partition the pipelines into threads */
  for(index = 0; index < app->pipeline_count; ++index)
  {
    ThreadInfo* ti;
    int threadIndex;
    GstElement* pipeline;
    
    threadIndex = index % app->thread_count;
    ti = &threadInfos[threadIndex];
    pipeline = app->pipelines[index];

    g_print("Assigning pipeline %d (%d) to thread %d as pipeline %d\n", index, pipeline, threadIndex, ti->pipeline_count);
    ti->app = app;
    ti->thread_index = threadIndex;
    ti->pipelines[ti->pipeline_count] = pipeline;
    ti->pipeline_count++;
  }

  for(index = 0; index < app->thread_count; ++index)
  {
    app->threads[index] = g_thread_create ((GThreadFunc)test_thread, &threadInfos[index], TRUE, NULL);
  }

  /* this mainloop is stopped when we receive an error or EOS */
  g_main_loop_run (app->loop);

  for(index = 0; index < app->thread_count; ++index)
  {
    g_thread_join(app->threads[index]);
  }

  g_print ("stopping\n");

  //gst_element_set_state (app->pipeline, GST_STATE_NULL);

  g_main_loop_unref (app->loop);

  g_print ("Test complete\n");

  return 0;
}