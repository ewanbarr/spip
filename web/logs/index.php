<?PHP

error_reporting(E_ALL);
ini_set("display_errors", 1);

include_once("../spip.lib.php");
include_once("../spip_webpage.lib.php");
include_once("../spip_socket.lib.php");

class logs extends spip_webpage
{

  function logs()
  {
    spip_webpage::spip_webpage();

    $this->title = "Log Files";
    $this->nav_item = "logs";

    $this->config = spip::get_config();

    $this->beams = array();
    $this->streams = array();
    $this->server_logs = array();
    $this->client_logs = array();
    $this->topology = array();

    for ($i=0; $i<$this->config["NUM_BEAM"]; $i++)
      $this->beams[$i] = $this->config["BEAM_".$i];

    for ($i=0; $i<$this->config["NUM_BEAM"]; $i++)
      $this->beams[$i] = $this->config["BEAM_".$i];

    $daemons = explode (" ", $this->config["SERVER_DAEMONS"]);
    foreach ($daemons as $daemon)
    {
      list ($name, $rank) = explode (":", $daemon);
      array_push($this->server_logs, $name);
    }

    $daemons = explode (" ", $this->config["CLIENT_DAEMONS"]);
    foreach ($daemons as $daemon)
    {
      list ($name, $rank) = explode (":", $daemon);
      array_push($this->client_logs, $name);
    }

    for ($i=0; $i<$this->config["NUM_STREAM"]; $i++)
    {
      list ($host, $beam, $subband) = explode (":", $this->config["STREAM_".$i]);

      if (!array_key_exists($beam, $this->topology))
        $this->topology[$beam] = array();
      $this->topology[$beam][$i] = $subband;

      $this->streams[$i] = "Beam ".$this->beams[$beam].", Band ".$subband;
    }
  }

  function printJavaScriptHead()
  {
?>
    <script type='text/javascript'>

      
      function show_server_logs ()
      {
        var log_idx = document.getElementById('server_log').selectedIndex;
        var log = document.getElementById('server_log').options[log_idx].value;

        if (log != "")
        {
          var log_viewer_url = "/spip/logs/viewer.php?server_log="+log;
          log_viewer.document.location = log_viewer_url;
        }
      }

      function show_client_logs ()
      {
        var log_idx = document.getElementById('client_log').selectedIndex;
        var log = document.getElementById('client_log').options[log_idx].value;

        var stream_idx = document.getElementById('client_stream').selectedIndex;
        var stream = document.getElementById('client_stream').options[stream_idx].value;

        if ((log != "") && (stream != ""))
        {
          var log_viewer_url = "/spip/logs/viewer.php?client_log="+log+"&stream="+stream;
          log_viewer.document.location = log_viewer_url;
        }
      }

    </script>

    <style type='text/css'>

      #log_select table {
        border: 1;
      }

      #log_select th {
        text-align: right;
        padding-right: 5px;
        width: 50px;
      }

      #log_select td {
        text-align: left;
      }

    </style>
<?php
  }

  function printSidebarHTML()
  {
    echo "<h3>Server Logs</h3>\n";

    echo "<table id='log_select'>\n";

    echo "  <tr>\n";
    echo "    <th>Log</th>\n";
    echo "    <td>\n";
    echo "      <select name='server_log' id='server_log' onChange='show_server_logs()'>\n";
    echo "        <option value='' selected>--</option>\n";
    foreach ($this->server_logs as $log)
    {
      echo "        <option value='".$log."'>".$log."</option>\n";
    }
    echo "      </select>\n";
    echo "    </td>\n";
    echo "  </tr>\n";
    echo "</table>\n";

    echo "<h3>Client Logs</h3>\n";
   
    echo "<table id='log_select'>\n";

    echo "  <tr>\n";

    echo "    <th>Log</th>\n";
    echo "    <td>\n";

    echo "      <select name='client_log' id='client_log' onChange='show_client_logs()'>\n";
    echo "        <option value='' selected>--</option>\n";
    foreach ($this->client_logs as $log)
    {
      echo "<option value='".$log."'>".$log."</option>\n";
    }
    echo "</select>\n";
    
    echo "    </td>\n";
    echo "  </tr>\n";

    echo "  <tr>\n";
    echo "    <th>Stream</th>\n";
    echo "    <td>\n";
    echo "      <select id='client_stream' name='client_stream'>\n";
    echo "        <option value='' selected>--</option>\n";
    foreach ($this->streams as $stream => $desc)
    {
      echo "        <option value='".$stream."'>".$desc."</option>\n";
    }
    echo "      </select>\n";
    echo "    </td>\n";
    echo "  </tr>\n";


    echo "</table>\n";  

/*
    foreach ($this->topology as $beam => $streams)
    {
      echo "<tr><th colspan=2>".$this->config["BEAM_".$beam.]."</th>\n";
      foreach ($streams as $stream => $subband)
      {
        echo "<tr><td width='20px'>&nbsp;</td><td>

      }
    }
    echo "</table>\n";
*/
  }

  function printHTML()
  {
    echo "<iframe name='log_viewer' frameborder=0 src='/spip/logs/viewer.php' width=700px height=500px>\n";
    echo "</iframe>\n";
  }
}
if (!isset($_GET["update"]))
  $_GET["single"] = "true";
handleDirect("logs");

