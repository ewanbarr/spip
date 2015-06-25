/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/DataBlockView.h"

#include <cstdlib>
#include <stdexcept>

using namespace std;

spip::DataBlockView::DataBlockView (const char * key_string) : spip::DataBlock(key_string)
{
}

spip::DataBlockView::~DataBlockView ()
{
}

void spip::DataBlockView::lock ()
{
  if (!connected)
    throw runtime_error ("not connected to data block");

  if (locked)
    throw runtime_error ("data block already locked");

  if (ipcbuf_lock_read (header_block) < 0)
    throw runtime_error ("could not lock header block for viewing");

  if (ipcio_open (data_block, 'r') < 0)
   throw runtime_error ("could not lock header block for viewing");

  locked = true;
}

void spip::DataBlockView::unlock ()
{
  if (!connected)
    throw runtime_error ("not connected to data block");

  if (!locked)
    throw runtime_error ("data block was not locked");

  if (ipcio_close (data_block) < 0)
    throw runtime_error ("could not close data block from viewing");

  locked = false;
}

void * spip::DataBlockView::open_block ()
{
  if (!connected)
    throw runtime_error ("not connected to data block");

  if (!locked)
    throw runtime_error ("not locked as reader");

  throw runtime_error ("not yet implemented");
  return (void *) curr_buf;
}

ssize_t spip::DataBlockView::close_block (uint64_t bytes)
{
  if (!connected)
    throw runtime_error ("not connected to data block");

  if (!locked)
    throw runtime_error ("not locked as reader");

  throw runtime_error ("not yet implemented");
  return 0;
}
