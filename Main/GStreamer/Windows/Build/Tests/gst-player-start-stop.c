/* GStreamer
 *
 * Copyright (C) 2010 David Hoyt <dhoyt@hoytsoft.org>
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

#define NUMBER_OF_SIMULTANEOUS_PIPELINES 1
#define NUMBER_OF_SIMULTANEOUS_THREADS 1
#define NUMBER_OF_CHILD_WINDOWS NUMBER_OF_SIMULTANEOUS_PIPELINES

#define SLEEP_TIME 2000

#ifdef G_OS_WIN32
#define KILL_PROCESS(pid) SafeTerminateProcess((HANDLE)pid, 0)
#else
#define KILL_PROCESS(pid) kill(pid, SIGKILL)
#endif

typedef struct _App App;
typedef struct _ThreadInfo ThreadInfo;

static void gst_player_start_stop_main(App* app);
static void gst_player_start_stop_end(App* app);
static GstBusSyncReply xoverlay_bus_sync_handler(GstBus *bus, GstMessage *message, gpointer data);

struct _ThreadInfo 
{
  App* app;

  int thread_index;

  GPid* processes[NUMBER_OF_SIMULTANEOUS_PIPELINES];
  int process_count;
};

struct _App
{
  gboolean exiting;

  gchar* uri;

  GPid* processes;
  int process_count;

  ThreadInfo** thread_infos;
  GThread** threads;
  int thread_count;

  HWND hWndMain;
  HWND hWndChild[NUMBER_OF_CHILD_WINDOWS];
};

App s_app;

#ifdef G_OS_WIN32
static BOOL 
SafeTerminateProcess(HANDLE hProcess, UINT uExitCode) 
{
  DWORD dwTID, dwCode, dwErr = 0;
  HANDLE hProcessDup = INVALID_HANDLE_VALUE;
  HANDLE hRT = NULL;
  HINSTANCE hKernel = GetModuleHandle("Kernel32");
  BOOL bSuccess = FALSE;

  BOOL bDup = DuplicateHandle(GetCurrentProcess(), hProcess, GetCurrentProcess(), &hProcessDup, PROCESS_ALL_ACCESS, FALSE, 0);

  if (GetExitCodeProcess((bDup) ? hProcessDup : hProcess, &dwCode) && (dwCode == STILL_ACTIVE)) {
    FARPROC pfnExitProc;
    pfnExitProc = GetProcAddress(hKernel, "ExitProcess");
    hRT = CreateRemoteThread((bDup) ? hProcessDup : hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)pfnExitProc, (PVOID)uExitCode, 0, &dwTID);
    if (hRT == NULL)
      dwErr = GetLastError();
  } else {
    dwErr = ERROR_PROCESS_ABORTED;
  }

  if (hRT) {
    WaitForSingleObject((bDup) ? hProcessDup : hProcess, INFINITE);
    CloseHandle(hRT);
    bSuccess = TRUE;
  }

  if (bDup)
    CloseHandle(hProcessDup);

  if (!bSuccess)
    SetLastError(dwErr);

  return bSuccess;
}
#endif

static void
test_thread (ThreadInfo* ti)
{
	int i;
  int index;

	for(i = 0; i < 300 && !ti->app->exiting; ++i) {
    Sleep(1000);
	}

  g_print ("Thread %d completed\n", ti->thread_index);
}

static GPid 
spawn_gst_player(App* app, const gint64 window_id, const gchar* uri) 
{
  GPid pid = NULL;
  GError* err = NULL;
  gchar* win = g_strdup_printf("%lu", window_id);
  gchar* argv[] = { "gst-player", "--video-sink=d3dvideosink", "-l", "-w", win, "-u", uri, NULL };
  
  g_print("Launching gst-player for window %lu\n", window_id);

  if (!g_spawn_async(NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH, NULL, NULL, &pid, &err))
    g_print("Unable to launch process: %s\n", err->message);

  if (err)
    g_error_free(err);
  if (win)
    g_free(win);

  g_print("Process launched with pid %d\n", pid);
  return pid;
}

int
main (int argc, char *argv[])
{
  int index;
  App *app = &s_app;
  GThread* threads[NUMBER_OF_SIMULTANEOUS_THREADS];
  ThreadInfo* threadInfos[NUMBER_OF_SIMULTANEOUS_THREADS];
  GPid processes[NUMBER_OF_SIMULTANEOUS_PIPELINES];

  memset(app, 0, sizeof(App));

  app->threads = threads;
  app->thread_infos = threadInfos;
  app->thread_count = NUMBER_OF_SIMULTANEOUS_THREADS;

  app->processes = processes;
  app->process_count = NUMBER_OF_SIMULTANEOUS_PIPELINES;

  for(index = 0; index < app->process_count; ++index)
  {
    GPid process;
    process = app->processes[index] = NULL;
  }

  for(index = 0; index < app->thread_count; ++index)
  {
    g_print ("Clearing memory for thread %d\n", index);
    app->thread_infos[index] = malloc(sizeof(ThreadInfo));
    app->thread_infos[index]->process_count = 0;
  }

  g_thread_init(NULL);
  gst_init(&argc, &argv);

  if (argc < 2) {
    g_print ("usage: %s <uri>\n", argv[0]);
    return -1;
  }

  app->uri = argv[1];

  gst_player_start_stop_main(app);

  g_print ("Test complete\n");

  return 0;
}

static void 
gst_player_start_stop_init(App* app) 
{
  gint index;

  /* partition the pipelines into threads */
  for(index = 0; index < app->process_count; ++index)
  {
    ThreadInfo* ti;
    int threadIndex;
    GPid process;
    
    threadIndex = index % app->thread_count;
    ti = app->thread_infos[threadIndex];
    process = app->processes[index] = spawn_gst_player(app, (gint64)app->hWndChild[index], app->uri);

    g_print("Assigning process %d (%d) to thread %d as pipeline %d\n", index, process, threadIndex, ti->process_count);
    ti->app = app;
    ti->thread_index = threadIndex;
    ti->processes[ti->process_count] = process;
    ti->process_count++;
  }

  g_print ("Starting\n");

  for(index = 0; index < app->thread_count; ++index)
  {
    app->threads[index] = g_thread_create((GThreadFunc)test_thread, app->thread_infos[index], TRUE, NULL);
  }
}

static void 
gst_player_start_stop_end(App* app) 
{
  gint index;

  g_print ("Stopping\n");

  app->exiting = TRUE;

  for(index = 0; index < app->thread_count; ++index)
  {
    if (app->threads[index] && app->threads[index]->joinable) {
      g_thread_join(app->threads[index]);
    }
  }

  for(index = 0; index < app->process_count; ++index) 
  {
    if (app->processes[index]) {
      g_print("Terminating process %d\n", app->processes[index]);
      KILL_PROCESS(app->processes[index]);
      g_spawn_close_pid(app->processes[index]);
    }
  }
}

LRESULT CALLBACK MainWndProc(HWND hWnd, UINT Msg,
                             WPARAM wParam, LPARAM lParam)
{

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
    //break;
    break;

  case WM_SIZE: 
    {
      int i;
      int left;
      int width;
      int height;
      RECT rMain, rChild;

      GetClientRect(s_app.hWndMain, &rMain);
      
      left = 0;
      height = rMain.bottom - rMain.top;
      width = (rMain.right - rMain.left) / NUMBER_OF_CHILD_WINDOWS;
      for(i = 0; i < NUMBER_OF_CHILD_WINDOWS; ++i) {
        SetWindowPos(s_app.hWndChild[i], HWND_TOP, left, 0, width, height, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOREDRAW);
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
gst_player_start_stop_main(App* app) {
  gint i;
  MSG   msg;
  RECT rect;
  WNDCLASS WndClass, WndClassXOverlay;
  HINSTANCE hInstance;

  memset(&WndClass, 0, sizeof(WNDCLASS));
  WndClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
  WndClass.hInstance = hInstance = GetModuleHandle(NULL);
  WndClass.lpszClassName = "GST-Test-Gst-Player";
  WndClass.hbrBackground = (HBRUSH) GetStockObject(BLACK_BRUSH);
  WndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
  WndClass.cbClsExtra = 0;
  WndClass.cbWndExtra = 0;
  WndClass.lpfnWndProc = MainWndProc;
  RegisterClass(&WndClass);

  s_app.hWndMain = CreateWindowEx(
    0, 
    WndClass.lpszClassName,
    "Gst-Player Window",
    WS_OVERLAPPEDWINDOW, 
    CW_USEDEFAULT, 
    CW_USEDEFAULT, 
    960 + GetSystemMetrics (SM_CXSIZEFRAME) * 2, 
    480 + GetSystemMetrics(SM_CYCAPTION) + (GetSystemMetrics(SM_CYSIZEFRAME) * 2), 
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
      "Gst-Player Embedded Window",
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

  gst_player_start_stop_init(app);

  while (GetMessage(&msg,NULL, 0,0))
  {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  gst_player_start_stop_end(app);
}
