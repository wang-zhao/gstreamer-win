#include <Windows.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

class connector {
public:
	connector (const char *dll = "cygwin1.dll");
	~connector ();

	// A wrapper around GetProcAddress() for fetching symbols from the
	// cygwin DLL.  Can throw windows_error.
	template < class T > void get_symbol (const char *name, T &fn) const;

	// Wrappers for errno() and strerror().
	int err_no () const;
// 	std::string str_error (int) const;
// 
// 	// Converting between the worlds of Windows and Cygwin.
// 	std::string unix_path (const std::string &windows) const;
// 	std::string win_path (const std::string &unix) const;

private:
	HMODULE _library;
// 
// 	int *(*_errno) ();
// 	const char *(*_strerror) (int);
// 	void (*_conv_to_full_posix_path) (const char *, char *);
// 	void (*_conv_to_full_win32_path) (const char *, char *);

	int (__cdecl* fpsigaction)(int, const struct sigaction *, struct sigaction *);
	int (__cdecl* fpkillpg)(pid_t, int);
	pid_t (__cdecl* fpwaitpid)(pid_t, __wait_status_ptr_t, int);
	unsigned (__cdecl* fpalarm)(unsigned __secs );
	pid_t (__cdecl* fpgetpgrp)();
	int (__cdecl* fpsetpgid)(pid_t __pid, pid_t __pgid );
	pid_t (__cdecl* fpfork)();
	char* (__cdecl* fpstrsignal)(int __signo);

public:
	// The constructor will automatically hook you up for receiving
	// cygwin signals.  Just specify a signal and pass in a signal_handler.
// 	typedef std::pointer_to_unary_function<int,void> signal_handler;
// 	signal_handler *set_handler (int signal, signal_handler *);

	int sigaction_proxy(int, const struct sigaction *, struct sigaction *);
	int killpg_proxy(pid_t, int);
	pid_t waitpid_proxy(pid_t, __wait_status_ptr_t, int);
	unsigned alarm_proxy(unsigned __secs );
	pid_t getpgrp_proxy();
	int setpgid_proxy(pid_t __pid, pid_t __pgid );
	pid_t fork_proxy();
	char* strsignal_proxy(int __signo);

private:
	// Cygwin signals can only be received in threads that are calling
	// interruptible functions or otherwise ready to intercept signals, so
	// we spawn a thread that does nothing but call sigwait().

	// This is the entry point:
// 	static DWORD WINAPI signal_thread (void *);
// 	// It runs this:
// 	void await_signal ();
// 	// And will execute this on receipt of any signal for which it's
// 	// registered:
// 	void handle_signals (int);
// 
// 	HANDLE _signal_thread;
// 	bool _waiting_for_signals, _signal_thread_done;
// 	CRITICAL_SECTION _thread_mutex;
// 
// 	typedef std::map< int, signal_handler * > callback_list;
// 	callback_list _signal_handlers;
};

template <class T> void connector::get_symbol (const char *name,
											   T &symbol) const
{
	FARPROC retval = NULL;

	retval = GetProcAddress (_library, name);

	if (retval == NULL)
	{
		symbol = 0;
	}

	symbol = reinterpret_cast < T > (retval);
}

connector::connector (const char *dll)
{
// This should call init.cc:dll_entry() with DLL_PROCESS_ATTACH,
  // which calls dll_crt0_0().
  if ((_library = GetModuleHandle (dll)) == NULL)
  {
	  return;
  }

  // This calls dcrt0.cc:cygwin_dll_init(), which calls dll_crt0_1(),
  // which will, among other things:
  // * spawn the cygwin signal handling thread from sigproc_init()
  // * initialize the thread-local storage for this thread and overwrite
  //   the first 4K of the stack
//   void (*cyginit) ();
//   get_symbol ("cygwin_dll_init", cyginit);
//   (*cyginit) ();

  // Pick up the function pointers for the basic infrastructure.
//   get_symbol ("__errno", _errno);
//   get_symbol ("strerror", _strerror);
//   get_symbol ("cygwin_conv_to_full_posix_path", _conv_to_full_posix_path);
//   get_symbol ("cygwin_conv_to_full_win32_path", _conv_to_full_win32_path);


  get_symbol("sigaction", fpsigaction);
  get_symbol("killpg", fpkillpg);
  get_symbol("waitpid", fpwaitpid);
  get_symbol("alarm", fpalarm);
  get_symbol("getpgrp", fpgetpgrp);
  get_symbol("setpgid", fpsetpgid);
  get_symbol("fork", fpfork);
  get_symbol("strsignal", fpstrsignal);

  // Note that you need to be running an interruptible cygwin function if
  // you want to receive signals.  You can use the standard signal()
  // mechanism if you're willing to have your main thread spend all its time
  // in interruptible cygwin functions like sleep().  Christopher Faylor
  // cautions that this solution "could be slightly racy":  if a second
  // signal comes in before the first one is done processing, the thread
  // won't be back in sigwait() to catch it.

//   _waiting_for_signals = true;
//   _signal_thread_done = false;
//   InitializeCriticalSection (&_thread_mutex);
// 
//   DWORD tid;

//   _signal_thread = CreateThread (NULL,   // Default security.
//                                  32768,  // Adjust the stack size as
//                                          // appropriate for what your signal
//                                          // handler needs in order to run, and
//                                          // then add 4K for cygtls.
//                                  &signal_thread, // Function to call
//                                  this,   // Context
//                                  0,      // Flags
//                                  &tid);  // Thread ID
// 
//   if (_signal_thread == NULL)
//     throw windows_error ("CreateThread", "signal_thread");
}

connector::~connector ()
{
}
// 
// int connector::err_no () const
// {
//   int *e = (*_errno) ();
//   if (e == NULL)
//     {
//       return -1;
//     }
//   return *e;
// }
// 
// std::string connector::str_error (int err_no) const
// {
//  std::string retval;
// 
//   const char *s = (*_strerror) (err_no);
//   if (s != NULL)
//     {
//       retval = s;
//     }
//   else
//     {
//       std::ostringstream o;
//       o << "Unexpected errno " << err_no;
//       retval = o.str ();
//     }
// 
//   return retval;
// }
// 
// std::string connector::unix_path (const std::string &windows) const
// {
//   char buf[MAX_PATH];
// 
//   _conv_to_full_posix_path (windows.c_str (), buf);
// 
//   return std::string (buf);
// }
// 
// std::string connector::win_path (const std::string &unix) const
// {
//   char buf[MAX_PATH];
// 
//   _conv_to_full_win32_path (unix.c_str (), buf);
// 
//   return std::string (buf);
// }

int connector::sigaction_proxy(int _1, const struct sigaction * _2, struct sigaction * _3)
{
	if( fpsigaction)
	{
		return fpsigaction(_1, _2, _3);
	}
	return -1;

}
int connector::killpg_proxy(pid_t _1, int _2)
{
	if( fpkillpg)
	{
		return fpkillpg(_1, _2);
	}
	return -1;
}
pid_t connector::waitpid_proxy(pid_t _1, __wait_status_ptr_t _2, int _3)
{
	if(fpwaitpid )
	{
		return fpwaitpid(_1, _2, _3);
	}
	return -1;
}
unsigned connector::alarm_proxy(unsigned __secs )
{
	if( fpalarm)
	{
		return fpalarm(__secs);
	}
	return 0;
}
pid_t connector::getpgrp_proxy()
{
	if(fpgetpgrp)
	{
		return fpgetpgrp();
	}
	return -1;
}
int connector::setpgid_proxy(pid_t __pid, pid_t __pgid )
{
	if( fpsetpgid )
	{
		return fpsetpgid(__pid, __pgid);
	}
	return -1;
}
pid_t connector::fork_proxy()
{
	if(fpfork)
	{
		return fpfork();
	}
	return -1;
}
char* connector::strsignal_proxy(int __signo)
{
	if( fpstrsignal)
		return fpstrsignal(__signo);
	return "";
}

static connector*  g_c = 0;

connector& getConnector()
{
	if(g_c == 0)
		g_c = new connector();

	return *g_c;
}


#ifdef __cplusplus
extern "C"{
#endif

int sigaction(int _1, const struct sigaction * _2, struct sigaction *_3)
{
	return getConnector().sigaction_proxy(_1, _2, _3);
}
int killpg(pid_t _1, int _2)
{
	return getConnector().killpg_proxy(_1, _2);
}
pid_t waitpid(pid_t _1, __wait_status_ptr_t _2, int _3)
{
	return getConnector().waitpid_proxy(_1, _2, _3);
}
unsigned alarm(unsigned __secs )
{
	return getConnector().alarm_proxy(__secs);
}
pid_t getpgrp()
{
	return getConnector().getpgrp_proxy();
}
int setpgid(pid_t __pid, pid_t __pgid )
{
	return getConnector().setpgid_proxy(__pid, __pgid);
}
pid_t fork()
{
	return getConnector().fork_proxy();
}
char* strsignal(int __signo)
{
	return getConnector().strsignal_proxy(__signo);
}

#ifdef __cplusplus
};
#endif
