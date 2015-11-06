/***************************************************************************
 *
 *   Copyright (C) 2015 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/TCPSocketServer.h"
#include "spip/DataBlockStats.h"
#include "sys/time.h"

#include "ascii_header.h"

#include <cstring>
#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <fstream>
#include <new>
#include <vector>
#include <pthread.h>


#define DataBlockStats_CMD_NONE 0
#define DataBlockStats_CMD_START 1
#define DataBlockStats_CMD_STOP 2
#define DataBlockStats_CMD_QUIT 3

using namespace std;

spip::DataBlockStats::DataBlockStats(const char * key_string)
{
  cerr << "spip::DataBlockStats::DataBlockStats DataBlockView(" << key_string << ")" << endl;
  db = new DataBlockView (key_string);

  cerr << "spip::DataBlockStats::DataBlockStats db->connect()" << endl;
  db->connect();
  cerr << "spip::DataBlockStats::DataBlockStats db->lock()" << endl;
  db->lock();

  bufsz = db->get_data_bufsz();
  buffer = malloc(bufsz);
  cerr << "spip::DataBlockStats::DataBlockStats buffer is " << bufsz << " bytes" << endl;

  control_port = -1;
  header = (char *) malloc (DADA_DEFAULT_HEADER_SIZE);

  control_cmd = None;
  control_state = Idle;
  verbose = true;
  poll_time = 5;

}

spip::DataBlockStats::~DataBlockStats()
{
  db->unlock();
  db->disconnect();

  delete db;

  free (header);
  free (buffer);
}

int spip::DataBlockStats::configure (const char * config)
{
  if (ascii_header_get (config, "NCHAN", "%u", &nchan) != 1)
    throw invalid_argument ("NCHAN did not exist in header");

  if (ascii_header_get (config, "NBIT", "%u", &nbit) != 1)
    throw invalid_argument ("NBIT did not exist in header");

  if (ascii_header_get (config, "NPOL", "%u", &npol) != 1)
    throw invalid_argument ("NPOL did not exist in header");

  if (ascii_header_get (config, "NDIM", "%u", &ndim) != 1)
    throw invalid_argument ("NDIM did not exist in header");

  if (ascii_header_get (config, "TSAMP", "%f", &tsamp) != 1)
    throw invalid_argument ("TSAMP did not exist in header");

  if (ascii_header_get (config, "BW", "%f", &bw) != 1)
    throw invalid_argument ("BW did not exist in header");

  channel_bw = bw / nchan;

  bits_per_second  = (nchan * npol * ndim * nbit * 1000000) / tsamp;
  bytes_per_second = bits_per_second / 8;

  if (ascii_header_get (config, "START_CHANNEL", "%u", &start_chan) != 1)
    throw invalid_argument ("START_CHANNEL did not exist in header");
  if (ascii_header_get (config, "END_CHANNEL", "%u", &end_chan) != 1)
    throw invalid_argument ("END_CHANNEL did not exist in header");

  // save the header for use on the first open block
  strncpy (header, config, strlen(config)+1);
}

void spip::DataBlockStats::prepare ()
{
  // allocate a private buffer to be used for stats analysis
  cerr << "spip::DataBlockStats::prepare db->read_header()" << endl; 
  db->read_header();

  if (ascii_header_get (db->get_header(), "UTC_START", "%s", utc_start) < 0)
    throw runtime_error ("could not read UTC_START from header");

  cerr << "spip::DataBlockStats::prepare UTC_START=" << utc_start << endl;
}

void spip::DataBlockStats::start_control_thread (int port)
{
  control_port = port;

  int errno = pthread_create (&control_thread_id, 0, control_thread_wrapper, this);
  if (errno != 0)
    throw runtime_error ("pthread_create");
}

// wrapper method to start control thread
void * spip::DataBlockStats::control_thread_wrapper (void * ptr)
{
  reinterpret_cast<DataBlockStats*>( ptr )->control_thread ();
  return 0;
}

void spip::DataBlockStats::stop_control_thread ()
{
  control_cmd = Quit;
}

// start a control thread that will monitor commands from the TCS/LMC
void spip::DataBlockStats::control_thread()
{
#ifdef _DEBUG
  cerr << "spip::DataBlockStats::control_thread starting" << endl;
#endif

  if (control_port < 0)
  {
    cerr << "ERROR: no control port specified" << endl;
    return;
  }

  cerr << "spip::DataBlockStats::control_thread creating TCPSocketServer" << endl;
  spip::TCPSocketServer * control_sock = new spip::TCPSocketServer();

  // open a listen sock on all interfaces for the control port
  cerr << "spip::DataBlockStats::control_thread open socket on port=" 
       << control_port << endl;
  control_sock->open ("any", control_port, 1);

  int fd = -1;
  int verbose = 1;

  char * cmds = (char *) malloc (DADA_DEFAULT_HEADER_SIZE);
  char * cmd  = (char *) malloc (32);

  control_cmd = None;

  // wait for a connection
  while (control_cmd != Quit && fd < 0)
  {
    // accept with a 1 second timeout
#ifdef _DEBUG
    cerr << "control_thread : ctrl_sock->accept_client(1)" << endl;
#endif
    fd = control_sock->accept_client (1);
#ifdef _DEBUG
    cerr << "control_thread : fd=" << fd << endl;
#endif
    if (fd >= 0 )
    {
      if (verbose > 1)
        cerr << "control_thread : reading data from socket" << endl;
      ssize_t bytes_read = read (fd, cmds, DADA_DEFAULT_HEADER_SIZE);

      if (verbose)
        cerr << "control_thread: bytes_read=" << bytes_read << endl;

      control_sock->close_client();
      fd = -1;

      // now check command in list of header commands
      if (ascii_header_get (cmds, "COMMAND", "%s", cmd) != 1)
        throw invalid_argument ("COMMAND did not exist in header");
      if (verbose)
        cerr << "control_thread: cmd=" << cmd << endl;
      if (strcmp (cmd, "START") == 0)
      {
        strcat (header, cmds);
        if (ascii_header_del (header, "COMMAND") < 0)
          throw runtime_error ("Could not remove COMMAND from header");

        // write header
        if (verbose)
          cerr << "control_thread: control_cmd = Start" << endl;
        control_cmd = Start;
      }
      else if (strcmp (cmd, "STOP") == 0)
      {
        if (verbose)
          cerr << "control_thread: control_cmd = Stop" << endl;
        control_cmd = Stop;
      }
      else if (strcmp (cmd, "QUIT") == 0)
      {
        if (verbose)
          cerr << "control_thread: control_cmd = Quit" << endl;
        control_cmd = Quit;
      }
    }
  }

#ifdef _DEBUG
  cerr << "spip::DataBlockStats::control_thread exiting" << endl;
#endif
}

// monitor the most recently data to the data block to produce
// statistics on the data stream
bool spip::DataBlockStats::monitor (std::string stats_dir, unsigned stream_id)
{
  cerr << "spip::DataBlockStats::monitor ()" << endl;

  if (stats_dir.length() > 0)
    stats_dir += "/";

  keep_monitoring = true;
  const unsigned nbin = 256;

  vector <vector <vector <unsigned> > > hist;
  hist.resize(npol);
  for (unsigned ipol=0; ipol<npol; ipol++)
  {
    hist[ipol].resize(ndim);
    for (unsigned idim=0; idim<ndim; idim++)
    {
      hist[ipol][idim].resize(nbin);
    }
  }    

  const unsigned ntime = 512;
  const unsigned nfreq = 512;
  vector <vector <vector <float> > > freq_time;
  freq_time.resize(npol);
  for (unsigned ipol=0; ipol<npol; ipol++)
  {
    freq_time[ipol].resize(nfreq);
    for (unsigned ifreq=0; ifreq<nfreq; ifreq++)
    {
      freq_time[ipol][ifreq].resize(ntime);
    }
  }

  cerr << "spip::DataBlockStats::monitor: db->read (" << buffer << ", " << bufsz << ")" << endl;

  int64_t bytes_read = db->read (buffer, bufsz);
  const unsigned nbytes_per_sample = (nchan * ndim * npol * nbit) / 8;

  // TODO make this part of the format
  const unsigned sample_block_resolution = bufsz / nbytes_per_sample;
  const unsigned nblock = bufsz / (nbytes_per_sample * sample_block_resolution);

  const unsigned nsample_per_time = sample_block_resolution / ntime;
  const unsigned nchan_per_freq = nchan / nfreq;

  char local_time[32];
  char command[128];

  stringstream ss;
  ss << stats_dir << utc_start;
  sprintf (command, "mkdir -p %s", ss.str().c_str());
  if (verbose)
    cerr << "spip::DataBlockStats::monitor " << command << endl;
  system (command);

  while (keep_monitoring)
  {
    // if EOD, then stop monitoring
    if (db->eod ())
    {
      if (verbose)
        cerr << "spip::DataBlockStats::monitor encountered EOD, monitoring stopped " << endl;
      keep_monitoring = false;
    }
    // seek to the end of the current data block
    else if (db->view_eod (db->get_data_bufsz()) < 0)
    {
      if (verbose)
        cerr << "spip::DataBlockStats::monitor db->view_eod failed, monitoring stopped" << endl;
      keep_monitoring = false;
    }
    else
    {
      if (verbose)
        cerr << "spip::DataBlockStats::monitor reading data block" << endl;

      bytes_read = db->read (buffer, bufsz);

      int8_t * in = (int8_t *) buffer;
      int8_t val; 
 
      // zero all the histograms     
      for (unsigned ipol=0; ipol<npol; ipol++)
      {
        for (unsigned idim=0; idim<ndim; idim++)
        {
          fill ( hist[ipol][idim].begin(), hist[ipol][idim].end(), 0);
        }
        for (unsigned ifreq=0; ifreq<nfreq; ifreq++)
        {
          fill(freq_time[ipol][ifreq].begin(), freq_time[ipol][ifreq].end(), 0);
        }
      }

      // histogram all the samples
      uint64_t idat = 0;
      unsigned ibin, ifreq, itime;
      int re, im;
      unsigned power;
      for (unsigned iblock=0; iblock<nblock; iblock++)
      {
        for (unsigned ichan=0; ichan<nchan; ichan++)
        {
          ifreq = ichan / nchan_per_freq;

          for (unsigned ipol=0; ipol<npol; ipol++)
          {
            for (unsigned isamp=0; isamp<sample_block_resolution; isamp++)
            {
              re = (int) in[idat];
              im = (int) in[idat+1];

              ibin = re + 128;
              hist[ipol][0][ibin]++;

              ibin = im + 128;
              hist[ipol][1][ibin]++;

              // detect and average the timesamples into a NPOL sets of NCHAN * 512 waterfalls
              power = (unsigned) ((re * re) + (im * im));
              itime = isamp / nsample_per_time;
              freq_time[ipol][ifreq][itime] += power;

              idat += 2;
            }
          }
        } 
      }

      // write the data files to disk (not sure if this is the best long term idea...)
      time_t now = time(0);
      strftime (local_time, 32, DADA_TIMESTR, localtime(&now));

      ss.str("");

      ss << stats_dir << utc_start << "/" << local_time << "." << stream_id << ".hg.stats";
      if (verbose)
        cerr << "spip::DataBlockStats::monitor creating HG stats file " << ss.str() << endl;
      ofstream hg_file (ss.str().c_str(), ofstream::binary);

      hg_file.write (reinterpret_cast<const char *>(&npol), sizeof(npol));
      hg_file.write (reinterpret_cast<const char *>(&ndim), sizeof(ndim));
      hg_file.write (reinterpret_cast<const char *>(&nbin), sizeof(nbin));
      for (unsigned ipol=0; ipol<npol; ipol++)
      {
        for (unsigned idim=0; idim<ndim; idim++)
        {
          const char * buffer = reinterpret_cast<const char *>(&hist[ipol][idim][0]);
          hg_file.write(buffer, hist[ipol][idim].size() * sizeof(unsigned));
        }
      }
      hg_file.close();
      
      ss.str("");

      ss << stats_dir << utc_start << "/" << local_time << "." << stream_id << ".ft.stats";
      if (verbose)
        cerr << "spip::DataBlockStats::monitor creating FT stats file " << ss.str() << endl;
      ofstream ft_file (ss.str().c_str(), ofstream::binary);

      ft_file.write (reinterpret_cast<const char *>(&npol), sizeof(npol));
      ft_file.write (reinterpret_cast<const char *>(&nfreq), sizeof(nfreq));
      ft_file.write (reinterpret_cast<const char *>(&ntime), sizeof(ntime));
      for (unsigned ipol=0; ipol<npol; ipol++)
      {
        for (unsigned ifreq=0; ifreq<nfreq; ifreq++)
        {
          const char * buffer = reinterpret_cast<const char*>(&freq_time[ipol][ifreq][0]);
          ft_file.write(buffer, freq_time[ipol][ifreq].size() * sizeof(unsigned));
        }
      }   
      ft_file.close();

      if (verbose)
        cerr << "spip::DataBlockStats::monitor sleep(" << poll_time << ")" << endl;
      int to_sleep = poll_time;
      while (control_cmd != Quit && to_sleep > 0)
      {
        sleep (1);
        to_sleep--;
      }
    }
    if (control_cmd == Quit)
      keep_monitoring = false;
  }

  return true;
}
