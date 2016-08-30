/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/DataBlockRead.h"

#include <cstring>
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

  if (ipcbuf_is_reader (header_block))
    ipcbuf_mark_cleared (header_block);

  if (ipcbuf_unlock_read (header_block) < 0)
    throw runtime_error ("could not unlock header block from reading");

  locked = false;
}

void spip::DataBlockRead::close ()
{
  if (!connected)
    throw runtime_error ("not connected to data block");

  if (!locked)
    throw runtime_error ("not locked for reading on data block");

  if (ipcio_is_open (data_block))
    if (ipcio_close (data_block) < 0)
      throw runtime_error ("could not unlock data block from read");
}

char * spip::DataBlockRead::read_header ()
{
  if (!connected)
    throw runtime_error ("not connected to data block");

  if (!locked)
    throw runtime_error ("not locked as reader");

  uint64_t block_id;
  char * header_buf = ipcbuf_get_next_read (header_block, &block_id);
  if (!header_buf)
    throw runtime_error ("could not get next header buffer");

  size_t to_copy = strlen ((const char *) header);
  if (to_copy > header_bufsz - 1)
    to_copy = header_bufsz -1;

  // make a local copy of the header
  memcpy (header, header_buf, to_copy);

  if (ipcbuf_mark_cleared (header_block) < 0)
    throw runtime_error ("could not mark header buffer cleared");

  return (char *) header;
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
