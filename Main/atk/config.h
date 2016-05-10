/* config.h.win32.in Hand written to suit the MSVC/MinGW compilers  */
/* config.h.in.  Generated from configure.in by autoheader.  */

/* always defined to indicate that i18n is enabled */
#define ENABLE_NLS 1

/* Define the gettext package to be used */
#define GETTEXT_PACKAGE "gtk10"

/* Define to 1 if you have the `bind_textdomain_codeset' function. */
#define HAVE_BIND_TEXTDOMAIN_CODESET 1

/* Define to 1 if you have the `dcgettext' function. */
#define HAVE_DCGETTEXT 1

/* Define to 1 if you have the <dlfcn.h> header file. */
/*#undef HAVE_DLFCN_H*/

/* Define if the GNU gettext() function is already present or preinstalled. */
#define HAVE_GETTEXT 1

/* Define to 1 if you have the <inttypes.h> header file. */
#ifndef _MSC_VER
#define HAVE_INTTYPES_H 1
#else
/* #undef HAVE_INTTYPES_H */
#endif

/* Define if your <locale.h> file defines LC_MESSAGES. */
/*#undef HAVE_LC_MESSAGES*/

/* Define to 1 if you have the <locale.h> header file. */
#define HAVE_LOCALE_H 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#ifdef _MSC_VER
#if (_MSC_VER >= 1600)
#define HAVE_STDINT_H 1
#else
/* #undef HAVE_STDINT_H */
#endif
#else /*not MSVC*/
#define HAVE_STDINT_H 1
#endif

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
/*#undef HAVE_STRINGS_H*/

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#ifndef _MSC_VER
#define HAVE_UNISTD_H 1
#else
/* #undef HAVE_UNISTD_H */
#endif

/* Define to the sub-directory in which libtool stores uninstalled libraries.
   */
#define LT_OBJDIR ".libs/"

/* Name of package */
#define PACKAGE "ATK"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "http://bugzilla.gnome.org/enter_bug.cgi?product=atk"

/* Define to the full name of this package. */
#define PACKAGE_NAME "atk"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "atk 2.5.91"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "atk"

/* Define to the home page for this package. */
#define PACKAGE_URL "http://www.gtk.org/"

/* Define to the version of this package. */
#define PACKAGE_VERSION "2.5.91"

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Version number of package */
#define VERSION "2.5.91"
