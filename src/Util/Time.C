/***************************************************************************
 *
 *   Copyright (C) 2016 Andrew Jameson
 *   Licensed under the Academic Free License version 2.1
 *
 ***************************************************************************/

#include "spip/Time.h"

#include <time.h>
#include <ctype.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <iostream>

using namespace std;

spip::Time::Time(const char * s)
{
  char * str_utc = (char *) malloc(sizeof(char) * (strlen(s) + 4 + 1));
  sprintf (str_utc, "%s UTC", s);

  const char * format = "%Y-%m-%d-%H:%M:%S %Z";
  struct tm time_tm;

  strptime (str_utc, format, &time_tm);
  epoch = timegm (&time_tm);

  free(str_utc);
}

spip::Time::Time (time_t now)
{
  epoch = now;
}

spip::Time::~Time()
{
}

time_t spip::Time::mjd2utctm (double mjd)
{
  const int seconds_in_day = 86400;
  int days = (int) mjd;
  double fdays = mjd - (double) days;
  double seconds = fdays * (double) seconds_in_day;
  int secs = (int) seconds;
  double fracsec = seconds - (double) secs;
  if (fracsec - 1 < 0.0000001)
    secs++;

  int julian_day = days + 2400001;

  int n_four = 4  * (julian_day+((6*((4*julian_day-17918)/146097))/4+1)/2-37);
  int n_dten = 10 * (((n_four-237)%1461)/4) + 5;

  struct tm gregdate;
  gregdate.tm_year = n_four/1461 - 4712 - 1900; // extra -1900 for C struct tm
  gregdate.tm_mon  = (n_dten/306+2)%12;         // struct tm mon 0->11
  gregdate.tm_mday = (n_dten%306)/10 + 1;

  gregdate.tm_hour = secs / 3600;
  secs -= 3600 * gregdate.tm_hour;


  gregdate.tm_min = secs / 60;
  secs -= 60 * (gregdate.tm_min);

  gregdate.tm_sec = secs;

  gregdate.tm_isdst = -1;
  time_t date = mktime (&gregdate);

  return date;
}

string spip::Time::get_localtime ()
{
  return spip::Time::format_localtime (epoch);
}

string spip::Time::get_gmtime ()
{
  return spip::Time::format_gmtime (epoch);
}

string spip::Time::format_localtime (time_t e)
{
  char time_str[20];
  const char * format = "%Y-%m-%d-%H:%M:%S";
  struct tm * timeinfo = localtime (&e);
  strftime (time_str, 20, format, timeinfo);
  return string (time_str);
}

string spip::Time::format_gmtime (time_t e)
{
  char time_str[20];
  const char * format = "%Y-%m-%d-%H:%M:%S";
  struct tm * timeinfo = gmtime (&e);
  strftime (time_str, 20, format, timeinfo);
  return string (time_str);
}

