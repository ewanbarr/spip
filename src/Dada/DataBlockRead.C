/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/DataBlockRead.h"

#include <cstdlib>
#include <stdexcept>

using namespace std;

spip::DataBlockRead::DataBlockRead (const char * key_string) : spip::DataBlock (key_string)
{
}

spip::DataBlockRead::~DataBlockRead ()
{
}

void spip::DataBlockRead::lock ()
{
  if (!connected)
    throw runtime_error ("not connected to data block");

  if (locked)
    throw runtime_error ("data block already locked");

  if (ipcbuf_lock_read (header_block) < 0)
    throw runtime_error ("could not lock header block for reading");

  if (ipcio_open (data_block, 'R') < 0)
   throw runtime_error ("could not lock header block for writing");

  locked = true;
}

void spip::DataBlockRead::unlock ()
{
  if (!connected)
    throw runtime_error ("not connected to data block");

  if (!locked)
    throw runtime_error ("not locked for reading on data block");

  //if (header)
  //  free (header);
  //header = 0;

  if (ipcbuf_is_reader (header_block))
    ipcbuf_mark_cleared (header_block);

  if (ipcbuf_unlock_read (header_block) < 0)
    throw runtime_error ("could not unlock header block from reading");

  locked = false;
}

void * spip::DataBlockRead::open_block ()
{
  if (!connected)
    throw runtime_error ("not connected to data block");

  if (!locked)
    throw runtime_error ("not locked as reader");

  curr_buf = (void *) ipcio_open_block_read (data_block, &curr_buf_bytes, &curr_buf_id);
  return curr_buf;
}

ssize_t spip::DataBlockRead::close_block (uint64_t new_bytes)
{
  if (!connected)
    throw runtime_error ("not connected to data block");

  if (!locked)
    throw runtime_error ("not locked as reader");

  curr_buf = 0;
  return ipcio_close_block_read (data_block, new_bytes);
}
