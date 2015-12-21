/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/HardwareAffinity.h"

#include <cstdlib>
#include <iostream>

using namespace std;

spip::HardwareAffinity::HardwareAffinity ()
{
#ifdef HAVE_HWLOC
  hwloc_topology_init(&topology);
  hwloc_topology_load(topology);
#endif
}

// bind the current thread to the specified CPU
spip::HardwareAffinity::~HardwareAffinity ()
{
#ifdef HAVE_HWLOC
  hwloc_topology_destroy(topology);
#endif
}

void spip::HardwareAffinity::bind_to_cpu_core (int cpu_core)
{
#ifdef HAVE_HWLOC
  // determine the depth of CPU cores in the topology
  int core_depth = hwloc_get_type_or_below_depth (topology, HWLOC_OBJ_CORE);

  int n_cpu_cores = hwloc_get_nbobjs_by_depth (topology, core_depth);

  if (core_depth >= 0 && core_depth < n_cpu_cores)
  {
    // get the object corresponding to the specified core
    hwloc_obj_t obj = hwloc_get_obj_by_depth (topology, core_depth, cpu_core);

    // get a copy of its cpuset that we may modify
    hwloc_cpuset_t cpuset = hwloc_bitmap_dup (obj->cpuset);

    // Get only one logical processor (in case the core is SMT/hyper-threaded)
    hwloc_bitmap_singlify(cpuset);

    // try to bind this current process to the CPU set
    if (hwloc_set_cpubind(topology, cpuset, 0))
    {
      char *str;
      int error = errno;
      hwloc_bitmap_asprintf(&str, obj->cpuset);
      cerr << "Could not bind to cpuset " << str << ": " << strerror(error) << endl;
      free(str);
    }

    hwloc_bitmap_free (cpuset);
  }
#endif
}

void spip::HardwareAffinity::bind_to_memory (int cpu_core)
{
#ifdef HAVE_HWLOC
  // determine the depth of CPU cores in the topology
  int core_depth = hwloc_get_type_or_below_depth (topology, HWLOC_OBJ_CORE);

  int n_cpu_cores = hwloc_get_nbobjs_by_depth (topology, core_depth);

  if (core_depth >= 0 && core_depth < n_cpu_cores)
  {
    // get the object corresponding to the specified core
    hwloc_obj_t obj = hwloc_get_obj_by_depth (topology, core_depth, cpu_core);

    // get a copy of its cpuset that we may modify
    hwloc_cpuset_t cpuset = hwloc_bitmap_dup (obj->cpuset);

    // Get only one logical processor (in case the core is SMT/hyper-threaded)
    hwloc_bitmap_singlify(cpuset);

    hwloc_membind_policy_t policy = HWLOC_MEMBIND_BIND;
    hwloc_membind_flags_t flags = HWLOC_MEMBIND_THREAD;

    int result = hwloc_set_membind (topology, cpuset, policy, flags);
    if (result < 0)
    {
      cerr << "failed to set memory binding policy: " << strerror(errno) << endl;
    }

    // Free our cpuset copy
    hwloc_bitmap_free(cpuset);
  }
#endif
}
