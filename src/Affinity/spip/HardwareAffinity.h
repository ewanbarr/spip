
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

      void bind_to_cpu_core (int);

      void bind_to_memory (int);

    private:

#ifdef HAVE_HWLOC
      hwloc_topology_t topology;
#endif

  };
}

#endif

