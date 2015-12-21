/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/Socket.h"

#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <stdexcept>
#include <cstdlib>
#include <iostream>

using namespace std;

spip::Socket::Socket ()
{
  fd = 0;

  bufsz = 1500;               // standard MTU on most networks

#ifdef HAVE_HWLOC
  hwloc_topology_init(&topology);
  hwloc_topology_load(topology);
  int core_depth;
  hwloc_obj_t obj = hwloc_get_obj_by_depth (topology, core_depth, cpu_core);
  if (obj)
  {
    // Get a copy of its cpuset that we may modify.
    hwloc_cpuset_t cpuset = hwloc_bitmap_dup (obj->cpuset);

    // Get only one logical processor (in case the core is SMT/hyperthreaded)
    hwloc_bitmap_singlify (cpuset);

    hwloc_membind_policy_t policy = HWLOC_MEMBIND_BIND;
    hwloc_membind_flags_t flags = HWLOC_MEMBIND_THREAD;

    int result = hwloc_set_membind (topology, cpuset, policy, flags);
    if (result < 0)
    {
      fprintf (stderr, "dada_db: failed to set memory binding policy: %s\n",
               strerror(errno));
    }

    // Free our cpuset copy
    hwloc_bitmap_free(cpuset);
  }
#endif

  buf = (char *) malloc(bufsz);
  if (!buf)
  {
    cerr << "spip::Socket::Socket numa_alloc_local failed" << endl;
    buf = 0;
  }
}

spip::Socket::~Socket ()
{
  close_me();

  if (buf)
    free (buf);
  buf = 0;

#ifdef HAVE_HWLOC
  hwloc_topology_destroy(topology);
#endif
}

void spip::Socket::close_me ()
{
  if (fd > 0)
    close (fd);
  fd = 0;
}

void spip::Socket::resize (size_t new_bufsz)
{
  if (new_bufsz > bufsz)
  {
    cerr << "spip::Socket::resize old=" << bufsz << " new=" << new_bufsz << endl;
    if (buf)
      free (buf);
    bufsz = new_bufsz;
    buf = (char *) malloc (bufsz);
    if (!buf)
      cerr << "spip::Socket::resize malloc failed" << endl;
  }
}

int spip::Socket::set_nonblock ()
{
  if (fd)
  {
    int flags;
    flags = fcntl(fd,F_GETFL);
    flags |= O_NONBLOCK;
    non_block = 1;
    return fcntl(fd,F_SETFL,flags);
  }
  else
    throw runtime_error ("socket was not open");
}

int spip::Socket::set_block ()
{
  if (fd)
  {
    int flags;
    flags = fcntl(fd,F_GETFL);
    flags &= ~(O_NONBLOCK);
    non_block = 0;
    return fcntl(fd,F_SETFL,flags);
  }
  else
    throw runtime_error ("socket was not open");
}

struct in_addr* spip::Socket::atoaddr (const char *address)
{
  struct hostent *host;
  static struct in_addr saddr;

  /* First try it as aaa.bbb.ccc.ddd. */
  saddr.s_addr = inet_addr(address);
  if ((int) saddr.s_addr != -1)
  {
    return &saddr;
  }
  host = gethostbyname(address);
  if (host != NULL)
  {
    return (struct in_addr *) *host->h_addr_list;
  }
  return NULL;
}


