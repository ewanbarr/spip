/***************************************************************************
 *
 *   Copyright (C) 2016 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/ContainerCUDA.h"

using namespace std;

spip::ContainerCUDADevice::ContainerCUDADevice (int d, cudaStream_t s)
{
  device = d;
  stream = s;
  buffer = 0;
}

spip::ContainerCUDADevice::~ContainerCUDADevice ()
{
  if (buffer)
    cudaFree (buffer);
}

void spip::ContainerCUDADevice::resize ()
{
  uint64_t required_size = calculate_buffer_size ();
  if (required_size > size)
  {
    if (buffer)
      cudaFree (buffer);
    buffer = cudaMalloc (required_size);
  }
}

spip::ContainerCUDAPinned::ContainerCUDAPinned ()
{
}

spip::ContainerCUDAPinned::~ContainerCUDAPinned ()
{
  // deallocate any buffer
  if (buffer)
    cudaFreeHost (buffer);
}

void spip::ContainerCUDAPinned::resize ()
{
  uint64_t required_size = calculate_buffer_size ();
  if (required_size > size)
  {
    if (buffer)
      cudaFreeHost(buffer);
    cudaMallocHost((void **) &buffer, required_size);
    size = required_size;
  }
}
