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

#ifdef HAVE_CUDA
#include "ipcio_cuda.h"
#endif

using namespace std;

spip::DataBlockWrite::DataBlockWrite (const char * key_string) : spip::DataBlock(key_string)
{
#if HAVE_CUDA
  dev_id = -1;
  dev_bytes = 0;
  dev_ptr = NULL;
#endif
}

spip::DataBlockWrite::~DataBlockWrite ()
{
#ifdef _DEBUG
  cerr << "spip::DataBlockWrite::~DataBlockWrite()" << endl;
#endif
#ifdef HAVE_CUDA
  if (dev_ptr)
  {
    cudaError_t error_id = cudaFree (dev_ptr);
    if (error_id != cudaSuccess)
      throw runtime_error ("could not deallocate dev_ptr");
    dev_ptr = NULL;
  }
#endif
}

#ifdef HAVE_CUDA
void spip::DataBlockWrite::set_device (int id)
{
  dev_id = id;
  cudaError_t error_id = cudaSetDevice (dev_id);
  if (error_id != cudaSuccess)
    throw runtime_error ("could not set cuda device");

  dev_bytes = data_bufsz;
  error_id = cudaMalloc (&(dev_ptr), dev_bytes);  
  if (error_id != cudaSuccess)
    throw runtime_error ("could not allocate bytes for dev_ptr");
}
#endif

void spip::DataBlockWrite::open ()
{
  if (!connected)
    throw runtime_error ("not connected to data block");

  if (!locked)
    throw runtime_error ("data block not locked for writing");

  if (ipcio_open (data_block, 'W') < 0)
    throw runtime_error ("could not lock data block for writing");
}

void spip::DataBlockWrite::lock ()
{
  if (!connected)
    throw runtime_error ("not connected to data block");

  if (locked)
    throw runtime_error ("data block already locked");

  if (ipcbuf_lock_write (header_block) < 0)
    throw runtime_error ("could not lock header block for writing");
  locked = true;
}

void spip::DataBlockWrite::page ()
{
  if (ipcbuf_page ((ipcbuf_t*) data_block) < 0)
    throw runtime_error ("could not page in data block buffers");
}


void spip::DataBlockWrite::close ()
{
#ifdef _DEBUG
  cerr << "spip::DataBlockWrite::close()" << endl;
#endif
  if (!connected)
    throw runtime_error ("not connected to data block");

  if (!locked)
    throw runtime_error ("not locked for writing on data block");
  if (ipcio_is_open (data_block))
    if (ipcio_close (data_block) < 0)
      throw runtime_error ("could not unlock data block from writing");
}

void spip::DataBlockWrite::unlock ()
{
#ifdef _DEBUG
  cerr << "spip::DataBlockWrite::unlock()" << endl;
#endif
  if (!connected)
    throw runtime_error ("not connected to data block");

  if (!locked)
    throw runtime_error ("not locked for writing on data block");

  // if we are unlocking, we must close first
  close();

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

void spip::DataBlockWrite::zero_next_block ()
{
  if (!connected)
    throw runtime_error ("not connected to data block");

  if (!locked)
    throw runtime_error ("not locked as writer");

#ifdef HAVE_CUDA
  if (dev_ptr)
  {
    if (ipcio_zero_next_block_cuda (data_block, dev_ptr, dev_bytes, stream) < 0)
      throw runtime_error ("could not zero next block");
  }
  else
  if (ipcio_zero_next_block (data_block) < 0)
    throw runtime_error ("could not zero next block");
#else
  if (ipcio_zero_next_block (data_block) < 0)
    throw runtime_error ("could not zero next block");
#endif
}

