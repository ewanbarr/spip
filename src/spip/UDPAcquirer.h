
#ifndef __UDPAcquisition_h
#define __UDPAcquisition_h

#include "spip/Acquisition.h"
#include "spip/UDPSocket.h"

namespace spip {

  class UDPAcquisition : public Acquisition {

    public:

      UDPAcquisition ();

      ~UDPAcquisition ();

    private:

      UDPSocket * socket;

  }

}

