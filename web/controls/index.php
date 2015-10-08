<?PHP

error_reporting(E_ALL);
ini_set("display_errors", 1);

include_once("../spip_webpage.lib.php");
include_once("../spip.lib.php");
include_once("../spip_socket.lib.php");

class controls extends spip_webpage
{
  var $server_daemons = array();
  var $client_daemons = array();

  function controls ()
  {
    spip_webpage::spip_webpage ();

    $this->title = "Controls";
    $this->nav_item = "controls";

    $this->config = spip::get_config();
    $this->independent_beams = true;

    $this->topology = array();

    if (strcmp($this->config["INDEPENDENT_BEAMS"], "true") !== TRUE)
    {
      $this->independent_beams = false;
      $host = $this->config["SERVER_HOST"];
      if (!array_key_exists($host, $this->topology))
        $this->topology[$host] = array();
      array_push($this->topology[$host], array("beam" =>"server", "subband" => "", "stream_id" => "-1"));
    }

    for ($i=0; $i<$this->config["NUM_STREAM"]; $i++)
    {
      list ($host, $beam, $subband) = explode (":", $this->config["STREAM_".$i]);

      if (!array_key_exists($host, $this->topology))
        $this->topology[$host] = array();
      array_push($this->topology[$host], array("beam" => $beam, "subband" => $subband, "stream_id" => $i));
    }

    // prepare server daemons
    $list = explode (" ", $this->config["SERVER_DAEMONS"]);
    foreach ($list as $item)
    {
      list ($daemon, $level) = explode (":", $item);
      array_push ($this->server_daemons, array("daemon" => $daemon, "level" => $level));
    }

    // prepare client daemons
    $list = explode (" ", $this->config["CLIENT_DAEMONS"]);
    foreach ($list as $item)
    {
      list ($daemon, $level) = explode (":", $item);
      array_push ($this->client_daemons, array("daemon" => $daemon, "level" => $level));
    }
  }

  function javaScriptCallback()
  {
    return "control_request();";
  }

  function printJavaScriptHead()
  {
?>
    <script type='text/javascript'>

      function handle_control_request(c_xml_request)
      {
        if (c_xml_request.readyState == 4)
        {
          var xmlDoc = c_xml_request.responseXML;
          if (xmlDoc != null)
          {
            var xmlObj=xmlDoc.documentElement;

            //var http_server = xmlObj.getElementsByTagName("http_server")[0].childNodes[0].nodeValue;
            //var url_prefix  = xmlObj.getElementsByTagName("url_prefix")[0].childNodes[0].nodeValue;
            
            var lmcs = xmlObj.getElementsByTagName("lmc")
            var i, j, k;      
  
            for (i=0; i<lmcs.length; i++)
            {
              var lmc = lmcs[i];

              var host = lmc.getAttribute("host");
              var port = lmc.getAttribute("port");

              var state = lmc.getElementsByTagName("state")[0];
              var lmc_light = document.getElementById(host + "_lmc_light")
              if (state.childNodes[0].nodeValue == "Running")
                lmc_light.src = "/spip/images/green_light.png";
              else
                lmc_light.src = "/spip/images/red_light.png";

              var streams = lmc.getElementsByTagName("stream")
              for (j=0; j<streams.length; j++)
              {
                var stream = streams[j];
                var stream_id = stream.getAttribute("id")

                daemons = stream.getElementsByTagName("daemon");
                for (k=0; k<daemons.length; k++)
                {
                  var daemon = daemons[k];
                  var daemon_name = daemon.getAttribute("name");
                  var daemon_running = daemon.childNodes[0].nodeValue;
                  var daemon_id = host + "_" + daemon_name + "_" + stream_id + "_light";
                  try {
                    var daemon_light = document.getElementById(daemon_id)
                    if (daemon_running == "True")
                      daemon_light.src = "/spip/images/green_light.png";
                    else
                      daemon_light.src = "/spip/images/red_light.png";
                  } catch (e) {
                    alert("ERROR: id=" + daemon_id + " Error=" + e)
                  }
                  
                }
              }
            }
          }
        }
      }

      function control_request() 
      {
        var url = "?update=true";

        if (window.XMLHttpRequest)
          c_xml_request = new XMLHttpRequest();
        else
          c_xml_request = new ActiveXObject("Microsoft.XMLHTTP");

        c_xml_request.onreadystatechange = function()
        {
          handle_control_request(c_xml_request)
        };
        c_xml_request.open("GET", url, true);
        c_xml_request.send(null);
      }


    </script>
<?php
  }


  function printHTML ()
  {
    echo "<h1>Controls</h1>\n";

    echo "<center>\n";

    # if the beams are to be operated independently of each other, then separate
    # server and client daemons will exist for each beam. However, if the beams
    # are used together (i.e. for a multi-beam survey), only 1 set of server daemons
    # will be used

    echo "<table cellpadding='5px' border=1 width='600px'>\n";

    echo "<tr>\n";
    echo "<th>Host</th>\n";
    echo "<th>LMC</th>\n";
    if ($this->config["NUM_BEAM"] > 1)
      echo "<th>Beam</th>\n";
    if ($this->config["NUM_SUBBAND"] > 1)
      echo "<th>Sub-band</th>\n";
    echo "<th>Daemons</th>\n";
    echo "</tr>\n";

    $hosts = array_keys($this->topology);

    foreach ($hosts as $host)
    {
      $host_rows = count($this->topology[$host]);
      for ($i=0; $i<count($this->topology[$host]); $i++)
      {
        $stream = $this->topology[$host][$i];

        echo "<tr>\n";

        if ($host_rows == 1 || ($host_rows > 1 && $i == 0))
        {
          echo "<td rowspan=".$host_rows.">".$host."</td>\n";
          # each host has a single LMC instance that manages all child daemons
          echo "<td rowspan=".$host_rows.">\n";
            echo "<img border='0' id='".$host."_lmc_light' src='/images/grey_light.png' width='15px' height='15px'>\n";
            echo "<input type='button' value='Start' onClick='startLMC(\"".$host."\")'/>\n";
            echo "<input type='button' value='Stop' onClick='stopLMC(\"".$host."\")'/>\n";
          echo "</td>\n";
        }

        if ($this->config["NUM_BEAM"] > 1)
          echo "<td>".$stream["beam"]."</td>\n";
        if ($this->config["NUM_SUBBAND"] > 1)
          echo "<td>".$stream["subband"]."</td>\n";

        echo "<td>\n";
        if ($stream["beam"] == "server")
        {
          foreach ($this->server_daemons as $d)
          {
            $id = $host."_".$d["daemon"]."_".$stream["stream_id"];
            echo "<span style='padding-right: 10px;'>\n";
            echo "<img border='0' id='".$id."_light' src='/images/grey_light.png' width='15px' height='15px'>\n";
            echo $d["daemon"];
            echo "</span>\n";
          }
        }
        else
        {
          foreach ($this->client_daemons as $d)
          {
            echo "<span style='padding-right: 10px;'>\n";
            $id = $host."_".$d["daemon"]."_".$stream["stream_id"];
            echo "<img border='0' id='".$id."_light' src='/images/grey_light.png' width='15px' height='15px'>\n";
            echo $d["daemon"];
            echo "</span>\n";
          }
        }
        echo "</td>\n";
       
        echo "</tr>\n";
      }
    }

/*
    echo "<tr>\n";

    echo "<td colspan=2>";
    echo "<td>\n";
    echo "<input type='button' value='Start All' onClick='startServer(\"all\",\"all\")'/>\n";
    echo "<input type='button' value='Stop All' onClick='stopServer(\"all\",\"all\")'/>\n";
    echo "</td>\n";
    echo "<td>\n";
    echo "<input type='button' value='Start All' onClick='startClient(\"all\",\"all\")'/>\n";
    echo "<input type='button' value='Stop All' onClick='stopClient(\"all\",\"all\")'/>\n";
    echo "</td>\n";

    echo "</tr>\n";
*/

    echo "</table>\n";
  }
  
  function printUpdateHTML($get)
  {
    # spip_lmc script runs on each client/server, check that it is running and responsive
    $xml = "<controls_update>";

    # check if the LMC script is running on the specified host
    $hosts = array_keys($this->topology);
    $port  = $this->config["LMC_PORT"];
    $lmc_socket = new spip_socket();

    $xml_cmd  = XML_DEFINITION;
    $xml_cmd .= "<lmc_cmd>";
    $xml_cmd .= "<requestor>controls page</requestor>";
    $xml_cmd .= "<command>daemon_status</command>";
    $xml_cmd .= "</lmc_cmd>";

    foreach ($hosts as $host)
    {
      $xml .= "<lmc host='".$host."' port='".$port."'>";
      if ($lmc_socket->open ($host, $port, 0) == 0)
      {
        $xml .= "<state>Running</state>";
        $lmc_socket->write ($xml_cmd."\r\n");
        list ($rval, $reply) = $lmc_socket->read();
        $xml .= $reply;
        $lmc_socket->close();
      }
      else
      {
        $xml .= "<state>Offline</state>";
      }
      $xml .= "</lmc>";
    }

    $xml .= "</controls_update>";

    header('Content-type: text/xml');
    echo $xml;
  }
}

if (!isset($_GET["update"]))
  $_GET["single"] = "true";
handleDirect("controls");

