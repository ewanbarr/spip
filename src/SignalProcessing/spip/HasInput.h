//-*-C++-*-
/***************************************************************************
 *
 *   Copyright (C) 2009 by Willem van Straten
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#ifndef __spip_HasInput_h
#define __spip_HasInput_h

namespace spip
{
  //! Attaches to Operations with input
  template <class In>
  class HasInput
  {
  public:

    //! Destructor
    virtual ~HasInput () {}

    //! Set the container from which input data will be read
    virtual void set_input (const In* _input) { input = _input; }

    //! Return pointer to the container from which input data will be read
    const In* get_input () const { return input; }

    //! Returns true if input is set
    bool has_input() const { return input; }

  protected:

    //! Container from which input data will be read
    const In * input;
  };
}

#endif

