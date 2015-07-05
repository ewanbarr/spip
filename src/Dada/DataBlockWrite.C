/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/DataBlockWrite.h"

#include <sstream>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

using namespace std;

spip::DataBlockWrite::DataBlockWrite (const char * key_string) : spip::DataBlock(key_string)
{
}

spip::DataBlockWrite::~DataBlockWrite ()
{
}

void spip::DataBlockWrite::lock ()
{
  if (!connected)
    throw runtime_error ("not connected to data block");

  if (locked)
    throw runtime_error ("data block already locked");

  if (ipcbuf_lock_write (header_block) < 0)
    throw runtime_error ("could not lock header block for writing");

  if (ipcio_open (data_block, 'W') < 0)
    throw runtime_error ("could not lock data block for writing");

  locked = true;
}

void spip::DataBlockWrite::unlock ()
{
  if (!connected)
    throw runtime_error ("not connected to data block");

  if (!locked)
    throw runtime_error ("not locked for writing on data block");

  if (ipcio_is_open (data_block))
    if (ipcio_close (data_block) < 0)
      throw runtime_error ("could not unlock data block from writing");

  if (ipcbuf_unlock_write (header_block) < 0)
    throw runtime_error ("could not unlock header block from writing");

  locked = true;
}

void spip::DataBlockWrite::write_header (const char* header)
{
  if (!connected)
    throw runtime_error ("not connected to data block");

  if (!locked)
    throw runtime_error ("not locked as writer");

  cerr << "spip::DataBlockWrite::write_header ipcbuf_get_next_write()" << endl;
  char * header_buf = ipcbuf_get_next_write (header_block);
  if (!header_buf)
    throw runtime_error ("could not get next header buffer");

  size_t to_copy = strlen (header);
  if (to_copy > header_bufsz - 1)
    to_copy = header_bufsz -1;

  memcpy (header_buf, header, to_copy);

  if (ipcbuf_mark_filled (header_block, header_bufsz) < 0)
    throw runtime_error ("could not mark header buffer filled");
}

ssize_t spip::DataBlockWrite::write_data (void * buffer, size_t bytes)
{
  if (!connected)
    throw runtime_error ("not connected to data block");

  if (!locked)
    throw runtime_error ("not locked as writer");

  ssize_t bytes_written = ipcio_write (data_block, (char *) buffer, bytes);
  if (bytes_written < 0)
    throw runtime_error ("could not write bytes to data block");

  return bytes_written;
}


void * spip::DataBlockWrite::open_block ()
{
  if (!connected)
    throw runtime_error ("not connected to data block");

  if (!locked)
    throw runtime_error ("not locked as writer");

  curr_buf = (void *) ipcio_open_block_write (data_block, &curr_buf_id);
  block_open = true;
  return curr_buf;
}


ssize_t spip::DataBlockWrite::close_block (uint64_t bytes)
{
  if (!connected)
    throw runtime_error ("not connected to data block");

  if (!locked)
    throw runtime_error ("not locked as writer");

  block_open = false;
  return ipcio_close_block_write (data_block, bytes);
}

ssize_t spip::DataBlockWrite::update_block (uint64_t bytes)
{
  if (!connected)
    throw runtime_error ("not connected to data block");

  if (!locked)
    throw runtime_error ("not locked as writer");

  return ipcio_update_block_write (data_block, bytes);
}
