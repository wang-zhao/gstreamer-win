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
#include <gst/interfaces/xoverlay.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <windows.h>

#define VIDEO_SINK "d3dvideosink"

#define NUMBER_OF_SIMULTANEOUS_PIPELINES 16
#define NUMBER_OF_SIMULTANEOUS_THREADS 8
#define NUMBER_OF_CHILD_WINDOWS NUMBER_OF_SIMULTANEOUS_PIPELINES

#define SLEEP_TIME 10

typedef struct _App App;
typedef struct _ThreadInfo ThreadInfo;

static void xoverlay_main(App* app);
static void xoverlay_end(App* app);
static GstBusSyncReply xoverlay_bus_sync_handler(GstBus *bus, GstMessage *message, gpointer data);

struct _ThreadInfo 
{
  App* app;

  int thread_index;

  GstElement* pipelines[NUMBER_OF_SIMULTANEOUS_PIPELINES / NUMBER_OF_SIMULTANEOUS_THREADS];
  int pipeline_count;
};

struct _App
{
  GstElement** pipelines;
  int pipeline_count;

  ThreadInfo** thread_infos;
  GThread** threads;
  int thread_count;

  GMainLoop *loop;

  GThread* main_loop;

  HWND hWndMain;
  HWND hWndChild[NUMBER_OF_CHILD_WINDOWS];
};

App s_app;

static void
main_loop_thread (App* app)
{
  /* this mainloop is stopped when we receive an error or EOS */
  g_main_loop_run (app->loop);

  SendMessage(app->hWndMain, WM_CLOSE, 0, 0);
}

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

    Sleep(SLEEP_TIME);
    
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
  GThread* main_loop;
  GThread* threads[NUMBER_OF_SIMULTANEOUS_THREADS];
  ThreadInfo* threadInfos[NUMBER_OF_SIMULTANEOUS_THREADS];
  GstElement* pipelines[NUMBER_OF_SIMULTANEOUS_PIPELINES];

  memset(app, 0, sizeof(App));

  app->threads = threads;
  app->thread_infos = threadInfos;
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
    GstElement* navigationtest;
    GstElement* ffmpegcolorspace;
    GstElement* videosink;

    g_print ("creating elements\n");

    pipeline = app->pipelines[index] = gst_pipeline_new("pipeline");
    g_assert (pipeline);
    
    videotestsrc = gst_element_factory_make ("videotestsrc", "videotestsrc");
    g_assert (videotestsrc);

    navigationtest = gst_element_factory_make ("navigationtest", "navigationtest");
    g_assert (navigationtest);

    ffmpegcolorspace = gst_element_factory_make ("ffmpegcolorspace", "ffmpegcolorspace");
    g_assert (ffmpegcolorspace);

    videosink = gst_element_factory_make (VIDEO_SINK, "videosink");
    g_assert (videosink);

    gst_bin_add_many(GST_BIN(pipeline), videotestsrc, navigationtest, ffmpegcolorspace, videosink, NULL);
    gst_element_link_many(videotestsrc, navigationtest, ffmpegcolorspace, videosink, NULL);

    g_print ("elements added and linked\n");
  }

  for(index = 0; index < app->thread_count; ++index)
  {
    g_print ("Clearing memory for thread %d\n", index);
    app->thread_infos[index] = malloc(sizeof(ThreadInfo));
    app->thread_infos[index]->pipeline_count = 0;
  }

  xoverlay_main(app);

  g_print ("stopping\n");

  //gst_element_set_state (app->pipeline, GST_STATE_NULL);

  g_main_loop_unref (app->loop);

  g_print ("Test complete\n");

  return 0;
}

static void 
xoverlay_init(App* app) 
{
  gint index;

  /* partition the pipelines into threads */
  for(index = 0; index < app->pipeline_count; ++index)
  {
    ThreadInfo* ti;
    int threadIndex;
    GstElement* pipeline;
    GstElement* bus;
    
    threadIndex = index % app->thread_count;
    ti = app->thread_infos[threadIndex];
    pipeline = app->pipelines[index];

    g_print("Assigning pipeline %d (%d) to thread %d as pipeline %d\n", index, pipeline, threadIndex, ti->pipeline_count);
    ti->app = app;
    ti->thread_index = threadIndex;
    ti->pipelines[ti->pipeline_count] = pipeline;
    ti->pipeline_count++;

    //set pipeline bus sync handler to watch for window id
    bus = (GstElement*)gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    gst_bus_set_sync_handler(GST_BUS(bus), (GstBusSyncHandler)xoverlay_bus_sync_handler, app->hWndChild[index]);
    gst_object_unref(bus);
  }

  for(index = 0; index < app->thread_count; ++index)
  {
    app->threads[index] = g_thread_create((GThreadFunc)test_thread, app->thread_infos[index], TRUE, NULL);
  }

  app->main_loop = g_thread_create((GThreadFunc)main_loop_thread, app, TRUE, NULL);
}

static void 
xoverlay_end(App* app) 
{
  gint index;

  g_thread_join(app->main_loop);

  for(index = 0; index < app->thread_count; ++index)
  {
    g_thread_join(app->threads[index]);
  }
}

LRESULT CALLBACK MainWndProc(HWND hWnd, UINT Msg,
                             WPARAM wParam, LPARAM lParam)
{
  App* app;

  if (Msg == WM_CREATE) {
    /* lParam holds a pointer to a CREATESTRUCT instance which in turn holds the parameter used when creating the window. */
    app = (App *)((LPCREATESTRUCT)lParam)->lpCreateParams;
    
    /* In our case, this is a pointer to the app. So we immediately attach it for use in subsequent calls. */
    SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG)app);
  }

  app = (App*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

  switch(Msg)
  {
  case WM_ERASEBKGND:
    return TRUE;
  case WM_PAINT:
    //g_print("PAINTING HWND: %d (Main is %d)\n", hWnd, app->hWndMain);
    //{
    //  gint i;
    //  HDC hDC;
    //  PAINTSTRUCT Ps;
    //  HPEN hPen;
    //  RECT r;

    //  for(i = 0; i < NUMBER_OF_CHILD_WINDOWS; ++i) {
    //    GetClientRect(app->hWndChild[i], &r);

    //    hDC = BeginPaint(app->hWndChild[i], &Ps);

    //    hPen = CreatePen(PS_DASHDOTDOT, 1, RGB(255, 25, 5));
    //    SelectObject(hDC, hPen);
    //    Rectangle(hDC, r.left, r.top, r.right, r.bottom);

    //    EndPaint(app->hWndChild[i], &Ps);
    //  }
    //}
    break;

  case WM_SIZE: 
    {
      int i;
      int left;
      int width;
      int height;
      RECT rMain, rChild;

      GetClientRect(app->hWndMain, &rMain);

      left = 0;
      height = rMain.bottom - rMain.top;
      width = (rMain.right - rMain.left) / NUMBER_OF_CHILD_WINDOWS;
      for(i = 0; i < NUMBER_OF_CHILD_WINDOWS; ++i) {
        SetWindowPos(app->hWndChild[i], HWND_TOP, left, 0, width, height, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOREDRAW);
        left += width;
      }

      //GetWindowRect(hWndToolbox, &rctToolbox);

      //SetWindowPos(hWndToolbox,
      //  HWND_TOP,
      //  rctMainWnd.left,
      //  rctMainWnd.top,
      //  rctToolbox.right - rctToolbox.left,
      //  rctMainWnd.bottom,
      //  SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    }
    break;

  case WM_CLOSE: 
  case WM_DESTROY:
    PostQuitMessage(WM_QUIT);
    break;
  }

  return DefWindowProc(hWnd, Msg, wParam, lParam);
}


static void 
xoverlay_main(App* app) {
  gint i;
  MSG   msg;
  RECT rect;
  WNDCLASS WndClass, WndClassXOverlay;
  HINSTANCE hInstance;

  memset(&WndClass, 0, sizeof(WNDCLASS));
  WndClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
  WndClass.hInstance = hInstance = GetModuleHandle(NULL);
  WndClass.lpszClassName = "GST-Test-XOverlay";
  WndClass.hbrBackground = (HBRUSH) GetStockObject(BLACK_BRUSH);
  WndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
  WndClass.cbClsExtra = 0;
  WndClass.cbWndExtra = 0;
  WndClass.lpfnWndProc = MainWndProc;
  RegisterClass(&WndClass);

  s_app.hWndMain = CreateWindowEx(
    0, 
    WndClass.lpszClassName,
    "XOverlay Window",
    WS_OVERLAPPEDWINDOW, 
    CW_USEDEFAULT, 
    CW_USEDEFAULT, 
    960 + GetSystemMetrics (SM_CXSIZEFRAME) * 2, 
    50 + GetSystemMetrics(SM_CYCAPTION) + (GetSystemMetrics(SM_CYSIZEFRAME) * 2), 
    NULL, 
    NULL,
    hInstance, 
    &s_app
  );

  if(!s_app.hWndMain)
    return;

  // Create a child window based on the available dialog box
  for(i = 0; i < NUMBER_OF_CHILD_WINDOWS; ++i) {
    s_app.hWndChild[i] = CreateWindow(
      WndClass.lpszClassName, 
      "XOverlay Embedded Window",
      WS_CHILD | WS_VISIBLE,
      0, 
      0, 
      1, 
      1, 
      s_app.hWndMain, 
      NULL, 
      hInstance, 
      NULL
    );
  }

  ShowWindow(s_app.hWndMain, SW_SHOW);

  for(i = 0; i < NUMBER_OF_CHILD_WINDOWS; ++i)
    ShowWindow(s_app.hWndChild[i], SW_SHOWNOACTIVATE);

  xoverlay_init(app);

  while (GetMessage(&msg,NULL, 0,0))
  {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  xoverlay_end(app);
}

static GstBusSyncReply 
xoverlay_bus_sync_handler(GstBus *bus, GstMessage *message, gpointer data)
{
  HWND hWnd = data;

  // ignore anything but 'prepare-xwindow-id' element messages
  if (GST_MESSAGE_TYPE (message) != GST_MESSAGE_ELEMENT)
    return GST_BUS_PASS;
  if (!gst_structure_has_name (message->structure, "prepare-xwindow-id"))
    return GST_BUS_PASS;

  if (hWnd != 0) {
    GstXOverlay *xoverlay;

    // GST_MESSAGE_SRC (message) will be the video sink element
    xoverlay = GST_X_OVERLAY (GST_MESSAGE_SRC (message));
    gst_x_overlay_set_xwindow_id (xoverlay, (gulong)hWnd);
  } else {
    g_warning ("Should have obtained hWnd by now!");
  }

  gst_message_unref (message);
  return GST_BUS_DROP;
}