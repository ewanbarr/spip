
#ifndef __Affinity_h_
#define __Affinity_h_

#include "config.h"

#ifdef HAVE_HWLOC
#include <hwloc.h>
#endif

namespace spip {

  class HardwareAffinity {

    public:

      HardwareAffinity ();

      ~HardwareAffinity ();

      void bind_thread_to_cpu_core (int core);

      void bind_process_to_cpu_core (int core);

      void bind_to_memory (int);

    private:

      void bind_to_cpu_core (int core, int flags);

#ifdef HAVE_HWLOC
      hwloc_topology_t topology;
#endif

  };
}

#endif

