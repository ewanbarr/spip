<?php

define('ECONNRESET', 104);

class spip_socket
{
  private $sock;

  private $is_open;

  private $errno;

  function __construct ()
  {
    $this->errno = 0;
    $this->is_open = 0;

    $this->sock = socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
    if ($this->sock === FALSE)
    {
      echo "ERROR: failed to create socket<br>\n";
    }
  }

  function __destruct ()
  {
    $this->close();
  }

  public function get_error ()
  {
    if ($this->errno == 0)
    {
      $this->errno = socket_last_error($this->sock);
    }
    $err_str = socket_strerror($this->serrno);
    $this->errno = 0;
  }

  public function open ($host, $port, $timeout=2)
  {
    $time = time();
    $connected = false;

    while (!$connected)
    {
      $connected = @socket_connect ($this->sock, $host, $port);
      if (!$connected)
      {
        if ($timeout > 0)
        {
          time_nanosleep(0,10000000);
          $timeout--;
        }
        else
        {
          return 1;
        }  
      }
    }
    
    if (! socket_set_block($this->sock))
    {
      echo "ERROR: failed to set blocking on socket<br/>\n";
      return -1;
    }

    $this->host = $host;
    $this->port = $port;
    $this->is_open = 1;

    return 0;
  }

  public function read ()
  {
    if ($this->is_open)
    {
      $response = @socket_read ($this->sock, 262144, PHP_NORMAL_READ);
      if ($response === FALSE)
      {
        if (socket_last_error() == ECONNRESET)
        {
          socket_close ($this->sock);
          $this->is_open = 0;
          return array(-1, socket_strerror(ECONNRESET));
        }
      }
      return array(0, $response);
    }
    else
      return array(-1, "socket not open");
  }

  public function read_raw ()
  {
    if ($this->is_open)
    {
      $raw_data = "";
      $data = @socket_read ($this->sock, 8192, PHP_BINARY_READ);
      $raw_data = $data;
      while ($data)
      {
        $data = socket_read($this->sock, 8192, PHP_BINARY_READ);
        $raw_data .= $data;
      }
      if (socket_last_error() == ECONNRESET)
      {
        socket_close ($this->sock);
        $this->is_open = 0;
        return array(-1, socket_strerror(ECONNRESET));
      }
      return array(0, $raw_data);
    }
    else
      return array(-1, "socket not open");
  }


  public function write ($string)
  {
    if ($this->is_open)
    {
      $bytes_to_write = strlen($string);
      $bytes_written = @socket_write($this->sock, $string, $bytes_to_write);

      if ($bytes_written === FALSE)
      {
        echo "Error writing data with socket_write()<BR>\n";
        return 0;
      }

      if ($bytes_written != $bytes_to_write) 
      {
        echo "Error, tried to write".$bytes_to_write.", but only ".$bytes_written." bytes were written<BR>\n";
        return 0;
      }

      return $bytes_written;
    }
  }

  public function close()
  {
    if ($this->is_open)
    {
      socket_close($this->sock);
      $this->sock = 0;
      $this->is_open = 0; 
    }
  }
}
