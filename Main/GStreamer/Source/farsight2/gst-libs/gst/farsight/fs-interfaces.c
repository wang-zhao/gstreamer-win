/*
 * fs-interfaces.c - Source for interface discovery code
 *
 * Farsight Helper functions
 * Copyright (C) 2006 Youness Alaoui <kakaroto@kakaroto.homelinux.net>
 * Copyright (C) 2007 Collabora, Nokia
 * Copyright (C) 2008 Haakon Sporsheim <haakon.sporsheim@tandberg.com>
 * @author: Youness Alaoui <kakaroto@kakaroto.homelinux.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "fs-interfaces.h"

#ifdef G_OS_UNIX

#include <arpa/inet.h>
#include <errno.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#ifdef HAVE_GETIFADDRS
 #include <sys/socket.h>
 #include <ifaddrs.h>
#endif

#include <gst/gst.h>

/**
 * SECTION:fs-interfaces
 * @short_description: Utility functions to discover local network interfaces
 *
 * These utility functions allow the discovery of local network interfaces
 * in a portable manner, they also allow finding the local ip addresses or
 * the address allocated to a network interface.
 */

GST_DEBUG_CATEGORY_EXTERN (fs_base_conference_debug);
#define GST_CAT_DEFAULT fs_base_conference_debug


/**
 * fs_interfaces_get_local_interfaces:
 *
 * Get the list of local interfaces
 *
 * Returns: a newly-allocated #GList of strings. The caller must free it.
 */
#ifdef HAVE_GETIFADDRS
GList *
fs_interfaces_get_local_interfaces (void)
{
  GList *interfaces = NULL;
  struct ifaddrs *ifa, *results;

  if (getifaddrs (&results) < 0) {
    return NULL;
  }

  /* Loop and get each interface the system has, one by one... */
  for (ifa = results; ifa; ifa = ifa->ifa_next) {
    /* no ip address from interface that is down */
    if ((ifa->ifa_flags & IFF_UP) == 0)
      continue;

    if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET)
      continue;

    GST_DEBUG ("Found interface : %s", ifa->ifa_name);
    interfaces = g_list_prepend (interfaces, g_strdup (ifa->ifa_name));
  }

  freeifaddrs (results);

  return interfaces;
}

#else /* ! HAVE_GETIFADDRS */

GList *
fs_interfaces_get_local_interfaces (void)
{
  GList *interfaces = NULL;
  gint sockfd;
  gint size = 0;
  struct ifreq *ifr;
  struct ifconf ifc;

  if ((sockfd = socket (AF_INET, SOCK_DGRAM, IPPROTO_IP)) < 0) {
    GST_ERROR ("Cannot open socket to retreive interface list");
    return NULL;
  }

  ifc.ifc_len = 0;
  ifc.ifc_req = NULL;

  /* Loop and get each interface the system has, one by one... */
  do {
    size += sizeof (struct ifreq);
    /* realloc buffer size until no overflow occurs  */
    if (NULL == (ifc.ifc_req = realloc (ifc.ifc_req, size))) {
      GST_ERROR ("Out of memory while allocation interface configuration structure");
      close (sockfd);
      return NULL;
    }
    ifc.ifc_len = size;

    if (ioctl (sockfd, SIOCGIFCONF, &ifc)) {
      perror ("ioctl SIOCFIFCONF");
      close (sockfd);
      free (ifc.ifc_req);
      return NULL;
    }
  } while (size <= ifc.ifc_len);


  /* Loop throught the interface list and get the IP address of each IF */
  for (ifr = ifc.ifc_req;
       (gchar *) ifr < (gchar *) ifc.ifc_req + ifc.ifc_len;
       ++ifr) {
    GST_DEBUG ("Found interface : %s", ifr->ifr_name);
    interfaces = g_list_prepend (interfaces, g_strdup (ifr->ifr_name));
  }

  free (ifc.ifc_req);
  close (sockfd);

  return interfaces;
}
#endif /* HAVE_GETIFADDRS */


static gboolean
fs_interfaces_is_private_ip (const struct in_addr in)
{
  /* 10.x.x.x/8 */
  if (in.s_addr >> 24 == 0x0A)
    return TRUE;

  /* 172.16.0.0 - 172.31.255.255 = 172.16.0.0/10 */
  if (in.s_addr >> 20 == 0xAC1)
    return TRUE;

  /* 192.168.x.x/16 */
  if (in.s_addr >> 16 == 0xC0A8)
    return TRUE;

  /* 169.254.x.x/16  (for APIPA) */
  if (in.s_addr >> 16 == 0xA9FE)
    return TRUE;

  return FALSE;
}

/**
 * fs_interfaces_get_local_ips:
 * @include_loopback: Include any loopback devices
 *
 * Get a list of local ipv4 interface addresses
 *
 * Returns: a newly-allocated #GList of strings. The caller must free it.
 */

#ifdef HAVE_GETIFADDRS

GList *
fs_interfaces_get_local_ips (gboolean include_loopback)
{
  GList *ips = NULL;
  struct sockaddr_in *sa;
  struct ifaddrs *ifa, *results;
  gchar *loopback = NULL;


  if (getifaddrs (&results) < 0)
      return NULL;

  /* Loop through the interface list and get the IP address of each IF */
  for (ifa = results; ifa; ifa = ifa->ifa_next)
  {
    /* no ip address from interface that is down */
    if ((ifa->ifa_flags & IFF_UP) == 0)
      continue;

    if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET)
      continue;

    sa = (struct sockaddr_in *) ifa->ifa_addr;

    GST_DEBUG ("Interface:  %s", ifa->ifa_name);
    GST_DEBUG ("IP Address: %s", inet_ntoa (sa->sin_addr));
    if ((ifa->ifa_flags & IFF_LOOPBACK) == IFF_LOOPBACK)
    {
      if (include_loopback)
        loopback = g_strdup (inet_ntoa (sa->sin_addr));
      else
        GST_DEBUG ("Ignoring loopback interface");
    }
    else
    {
      if (fs_interfaces_is_private_ip (sa->sin_addr))
        ips = g_list_append (ips, g_strdup (inet_ntoa (sa->sin_addr)));
      else
        ips = g_list_prepend (ips, g_strdup (inet_ntoa (sa->sin_addr)));
    }
  }

  freeifaddrs (results);

  if (loopback)
    ips = g_list_append (ips, loopback);

  return ips;
}

#else /* ! HAVE_GETIFADDRS */

GList *
fs_interfaces_get_local_ips (gboolean include_loopback)
{
  GList *ips = NULL;
  gint sockfd;
  gint size = 0;
  struct ifreq *ifr;
  struct ifconf ifc;
  struct sockaddr_in *sa;
  gchar *loopback = NULL;

  if ((sockfd = socket (AF_INET, SOCK_DGRAM, IPPROTO_IP)) < 0) {
    GST_ERROR ("Cannot open socket to retreive interface list");
    return NULL;
  }

  ifc.ifc_len = 0;
  ifc.ifc_req = NULL;

  /* Loop and get each interface the system has, one by one... */
  do {
    size += sizeof (struct ifreq);
    /* realloc buffer size until no overflow occurs  */
    if (NULL == (ifc.ifc_req = realloc (ifc.ifc_req, size))) {
      GST_ERROR ("Out of memory while allocation interface configuration"
        " structure");
      close (sockfd);
      return NULL;
    }
    ifc.ifc_len = size;

    if (ioctl (sockfd, SIOCGIFCONF, &ifc)) {
      perror ("ioctl SIOCFIFCONF");
      close (sockfd);
      free (ifc.ifc_req);
      return NULL;
    }
  } while  (size <= ifc.ifc_len);


  /* Loop throught the interface list and get the IP address of each IF */
  for (ifr = ifc.ifc_req;
       (gchar *) ifr < (gchar *) ifc.ifc_req + ifc.ifc_len;
       ++ifr) {

    if (ioctl (sockfd, SIOCGIFFLAGS, ifr)) {
      GST_ERROR ("Unable to get IP information for interface %s. Skipping...",
        ifr->ifr_name);
      continue;  /* failed to get flags, skip it */
    }
    sa = (struct sockaddr_in *) &ifr->ifr_addr;
    GST_DEBUG ("Interface:  %s", ifr->ifr_name);
    GST_DEBUG ("IP Address: %s", inet_ntoa (sa->sin_addr));
    if ((ifr->ifr_flags & IFF_LOOPBACK) == IFF_LOOPBACK){
      if (include_loopback)
        loopback = g_strdup (inet_ntoa (sa->sin_addr));
      else
        GST_DEBUG ("Ignoring loopback interface");
    } else {
      if (fs_interfaces_is_private_ip (sa->sin_addr)) {
        ips = g_list_append (ips, g_strdup (inet_ntoa (sa->sin_addr)));
      } else {
        ips = g_list_prepend (ips, g_strdup (inet_ntoa (sa->sin_addr)));
      }
    }
  }

  close (sockfd);
  free (ifc.ifc_req);

  if (loopback)
    ips = g_list_append (ips, loopback);

  return ips;
}

#endif /* HAVE_GETIFADDRS */


/**
 * fs_interfaces_get_ip_for_interface:
 * @interface_name: name of local interface
 *
 * Retreives the IPv4 address of an interface by its name
 *
 * Returns: a newly-allocated string with the IP address
 */
gchar *
fs_interfaces_get_ip_for_interface (gchar *interface_name)
{
  union  {
    struct sockaddr  s_addr;
    struct sockaddr_in sin_addr;
  } sockaddr_union;
  struct ifreq ifr;
  gint sockfd;


  ifr.ifr_addr.sa_family = AF_INET;
  memset (ifr.ifr_name, 0, sizeof (ifr.ifr_name));
  strncpy (ifr.ifr_name, interface_name, sizeof (ifr.ifr_name)-1);

  if ((sockfd = socket (AF_INET, SOCK_DGRAM, IPPROTO_IP)) < 0) {
    GST_ERROR ("Cannot open socket to retreive interface list");
    return NULL;
  }

  if (ioctl (sockfd, SIOCGIFADDR, &ifr) < 0) {
    GST_ERROR ("Unable to get IP information for interface %s",
      interface_name);
    close (sockfd);
    return NULL;
  }

  close (sockfd);
  sockaddr_union.s_addr = ifr.ifr_addr;
  GST_DEBUG ("Address for %s: %s", interface_name,
      inet_ntoa (sockaddr_union.sin_addr.sin_addr));
  return g_strdup (inet_ntoa (sockaddr_union.sin_addr.sin_addr));
}

#else /* G_OS_UNIX */
#ifdef G_OS_WIN32

#include <winsock2.h>
#include <Iphlpapi.h>

#include <gst/gst.h>

static gboolean started_wsa_engine = FALSE;

/*
 * private function that initializes the WinSock engine and
 *  returns a prebuilt socket
 */
SOCKET fs_interfaces_get_WSA_socket ()
{
  WORD wVersionRequested;
  WSADATA wsaData;
  int err;
  SOCKET sock;

  if (started_wsa_engine == FALSE) {
    wVersionRequested = MAKEWORD ( 2, 0 );

    err = WSAStartup ( wVersionRequested, &wsaData );
    if ( err != 0 ) {
      GST_ERROR ("Could not start the winsocket engine");
      return INVALID_SOCKET;
    }
    started_wsa_engine = TRUE;
  }


  if ((sock = socket (AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
    GST_ERROR ("Could not open socket to retreive interface list, error no : %d", WSAGetLastError ());
    return INVALID_SOCKET;
  }

  return sock;
}

GList * fs_interfaces_get_local_interfaces ()
{
  ULONG size = 0;
  PMIB_IFTABLE if_table;
  GList * ret = NULL;

  GetIfTable(NULL, &size, TRUE);

  if (!size)
    return NULL;

  if_table = (PMIB_IFTABLE)g_malloc0(size);

  if (GetIfTable(if_table, &size, TRUE) == ERROR_SUCCESS)
  {
    DWORD i;
    for (i = 0; i < if_table->dwNumEntries; i++)
    {
      ret = g_list_prepend (ret, g_strdup ((gchar*)if_table->table[i].bDescr));
    }
  }

  g_free(if_table);

  return ret;
}

GList * fs_interfaces_get_local_ips (gboolean include_loopback)
{
  ULONG size = 0;
  DWORD pref = 0;
  PMIB_IPADDRTABLE ip_table;
  GList * ret = NULL;

  GetIpAddrTable (NULL, &size, TRUE);

  if (!size)
    return NULL;

  /*
   * Get the best interface for transport to 0.0.0.0.
   * This interface should be first in list!
   */
  if (GetBestInterface (0, &pref) != NO_ERROR)
    pref = 0;

  ip_table = (PMIB_IPADDRTABLE)g_malloc0 (size);

  if (GetIpAddrTable (ip_table, &size, TRUE) == ERROR_SUCCESS)
  {
    DWORD i;
    for (i = 0; i < ip_table->dwNumEntries; i++)
    {
      gchar * ipstr;
      PMIB_IPADDRROW ipaddr = &ip_table->table[i];
      if (!(ipaddr->wType & (MIB_IPADDR_DISCONNECTED | MIB_IPADDR_DELETED)) &&
          ipaddr->dwAddr)
      {
        if (!include_loopback)
        {
		  DWORD type = 0;
          PMIB_IFROW ifr = (PMIB_IFROW)g_malloc0 (sizeof (MIB_IFROW));
          ifr->dwIndex = ipaddr->dwIndex;
		  if (GetIfEntry (ifr) == NO_ERROR)
		    type = ifr->dwType;
		  g_free (ifr);

		  if (type == IF_TYPE_SOFTWARE_LOOPBACK)
		    continue;
        }

        ipstr = g_strdup_printf ("%d.%d.%d.%d",
            (ipaddr->dwAddr      ) & 0xFF,
            (ipaddr->dwAddr >>  8) & 0xFF,
            (ipaddr->dwAddr >> 16) & 0xFF,
            (ipaddr->dwAddr >> 24) & 0xFF);
        if (ipaddr->dwIndex == pref)
          ret = g_list_prepend (ret, ipstr);
        else
          ret = g_list_append (ret, ipstr);
      }
    }
  }

  g_free(ip_table);

  return ret;
}

/*
 * returns ip address as an utf8 string
 */
static gchar *
win32_get_ip_for_interface (IF_INDEX idx)
{
  ULONG size = 0;
  PMIB_IPADDRTABLE ip_table;
  gchar * ret = NULL;

  GetIpAddrTable (NULL, &size, TRUE);

  if (!size)
    return NULL;

  ip_table = (PMIB_IPADDRTABLE)g_malloc0 (size);

  if (GetIpAddrTable (ip_table, &size, TRUE) == ERROR_SUCCESS)
  {
    DWORD i;
    for (i = 0; i < ip_table->dwNumEntries; i++)
    {
      PMIB_IPADDRROW ipaddr = &ip_table->table[i];
      if (ipaddr->dwIndex == idx &&
          !(ipaddr->wType & (MIB_IPADDR_DISCONNECTED | MIB_IPADDR_DELETED)))
      {
        ret = g_strdup_printf ("%d.%d.%d.%d",
            (ipaddr->dwAddr      ) & 0xFF,
            (ipaddr->dwAddr >>  8) & 0xFF,
            (ipaddr->dwAddr >> 16) & 0xFF,
            (ipaddr->dwAddr >> 24) & 0xFF);
        break;
      }
    }
  }

  g_free (ip_table);
  return ret;
}

gchar * fs_interfaces_get_ip_for_interface (gchar *interface_name)
{
  ULONG size = 0;
  PMIB_IFTABLE if_table;
  gchar * ret = NULL;

  GetIfTable (NULL, &size, TRUE);

  if (!size)
    return NULL;

  if_table = (PMIB_IFTABLE)g_malloc0 (size);

  if (GetIfTable (if_table, &size, TRUE) == ERROR_SUCCESS)
  {
    DWORD i;
    gchar * tmp_str;
    for (i = 0; i < if_table->dwNumEntries; i++)
    {
      tmp_str = g_utf16_to_utf8 (
          if_table->table[i].wszName, MAX_INTERFACE_NAME_LEN,
          NULL, NULL, NULL);

      if (g_ascii_strcasecmp (interface_name, tmp_str) == 0)
      {
        ret = win32_get_ip_for_interface (if_table->table[i].dwIndex);
        g_free (tmp_str);
        break;
      }

      g_free (tmp_str);
    }
  }

  g_free (if_table);

  return ret;
}


#else /* G_OS_WIN32 */
#error Can not use this method for retreiving ip list from OS other than unix or windows
#endif /* G_OS_WIN32 */
#endif /* G_OS_UNIX */
