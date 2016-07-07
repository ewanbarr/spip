//-*-C++-*-
/***************************************************************************
 *
 *   Copyright (C) 2009 by Willem van Straten
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#ifndef __spip_HasOutput_h
#define __spip_HasOutput_h

namespace spip
{
  //! Attaches to Operations with outputs
  template <class Out>
  class HasOutput
  {
  public:

    //! Destructor
    virtual ~HasOutput () {}

    //! Set the container into which output data will be written
    virtual void set_output (Out* _output) { output = _output; }

    //! Return pointer to the container into which output data will be written
    virtual Out* get_output () const { return output; }

    //! Returns true if output is set
    bool has_output() const { return output.ptr(); }

  protected:

    //! Container into which output data will be written
    Out * output;
  };
}

#endif

