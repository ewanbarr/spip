<?PHP

error_reporting(E_ALL);
ini_set("display_errors", 1);

include_once("../spip.lib.php");
include_once("../spip_webpage.lib.php");
include_once("../spip_socket.lib.php");

class status extends spip_webpage
{
  function status() 
  {
    spip_webpage::spip_webpage ();

    array_push($this->ejs, "/spip/js/prototype.js");
    array_push($this->ejs, "/spip/js/jsProgressBarHandler.js");

    $this->title = "Status";
    $this->nav_item = "status";

    $this->config = spip::get_config();

    $this->topology = array();
    for ($i=0; $i<$this->config["NUM_STREAM"]; $i++)
    {
      list ($host, $beam, $subband) = explode (":", $this->config["STREAM_".$i]);

      if (!array_key_exists($host, $this->topology))
        $this->topology[$host] = array();
      array_push ($this->topology[$host], $beam.":".$subband.":".$i);
    }
  }

  function javaScriptCallback()
  {
    return "status_request();";
  }


  function printJavaScriptHead()
  {
?>
    <script type='text/javascript'>

      function handle_status_request(s_xml_request)
      {
        if (s_xml_request.readyState == 4)
        {
          var xmlDoc = s_xml_request.responseXML;
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
              var hostsafe = host.replace("-","");
              var port = lmc.getAttribute("port");

              var state = lmc.getElementsByTagName("state")[0];

              var disks = lmc.getElementsByTagName("disk");
              for (j=0; j<disks.length; j++)
              {
                var disk = disks[j];
                var disk_mount = disk.getAttribute("mount");
                var percent_full = Math.floor (100 * parseFloat(disk.getAttribute("percent_full")));

                var disk_pb = eval(hostsafe + "_disk");
                disk_pb.setPercentage(percent_full);

                var disk_value = host + "_disk_value";
                document.getElementById(disk_value).innerHTML = percent_full.toFixed(2) + " %";
              }

              var loads = lmc.getElementsByTagName("system_load");
              for (j=0; j<loads.length; j++)
              {
                var load = loads[j];
                var ncore = parseFloat(load.getAttribute("ncore"));
                var load1 = parseFloat(load.getElementsByTagName("load1")[0].childNodes[0].nodeValue);
                var percent = Math.floor(100 * (load1 / ncore));
                
                var load_pb = eval(hostsafe + "_load");
                load_pb.setPercentage(percent)

                var load_value = host + "_load_value";
                document.getElementById(load_value).innerHTML = load1.toFixed(2);
              }

              var metrics = lmc.getElementsByTagName("metric");
              for (j=0; j<metrics.length; j++)
              {
                if (metrics[j].getAttribute("name") == "system_temp")
                {
                  var id = host + "_temp";
                  var system_temp = parseFloat( metrics[j].childNodes[0].nodeValue);
                  document.getElementById(id).innerHTML = system_temp.toFixed(1);
                }
              }
            }
          }
        }
      }

      function status_request() 
      {
        var url = "?update=true";

        if (window.XMLHttpRequest)
          s_xml_request = new XMLHttpRequest();
        else
          s_xml_request = new ActiveXObject("Microsoft.XMLHTTP");

        s_xml_request.onreadystatechange = function()
        {
          handle_status_request(s_xml_request)
        };
        s_xml_request.open("GET", url, true);
        s_xml_request.send(null);
      }
    </script>
<?php
  }

  function printJavaScriptBody()
  {
    echo "<script type='text/javascript'>\n";
    echo "Event.observe(window, 'load',  function()\n";
    echo "{\n";
    $hosts = array_keys($this->topology);
    foreach ($hosts as $host)
    {
      spip_webpage::renderProgressBarObject ($host."_load");
      spip_webpage::renderProgressBarObject ($host."_disk");
    }
    echo "}, false);\n";
    echo "</script>\n";
  }

  function printHTML()
  {
    echo "<h1>Status Page</h1>\n";

    // list information on all the hardware and software elements of the system
    echo "<table width=80%>\n";

    echo "<tr>\n";
    echo "<td><b>Host</b></td>\n";
    echo "<td><b>Load</b></td>\n";
    echo "<td><b>Disk</b></td>\n";
    echo "<td><b>Temp</b></td>\n";
    echo "<td><b>Beam</b></td>\n";
    echo "<td><b>Sub-band</b></td>\n";
    echo "<td><b>Stream</b></td>\n";
    echo "</tr>\n";

    $hosts = array_keys($this->topology);

    $temp = "0.0";
    $subband = "0.0";

    foreach ($hosts as $host)
    {
      $host_rows = count($this->topology[$host]);
      for ($i=0; $i<count($this->topology[$host]); $i++)
      {
        list ($beam, $subband, $stream) = explode(":",  $this->topology[$host][$i]);

        echo "<tr>\n";

        if ($host_rows == 1 || ($host_rows > 1 && $i == 0))
        {
          echo "<td rowspan=".$host_rows.">".$host."</td>\n";

          echo "<td rowspan=".$host_rows.">";
          spip_webpage::renderProgressBar($host."_load");
          echo "<span id='".$host."_load_value'></span>\n";
          echo "</td>\n";
          echo "<td rowspan=".$host_rows.">";
          spip_webpage::renderProgressBar($host."_disk");
          echo "<span id='".$host."_disk_value'></span>\n";
          echo "</td>\n";
      
          echo "<td rowspan=".$host_rows."><span id='".$host."_temp'></span></td>\n";
        }

        echo "<td>".$beam."</td>\n";
        echo "<td>".$subband."</td>\n";
        echo "<td>".$stream."</td>\n";

        echo "</tr>\n";
      }
    }
    echo "</table>\n";
  }

  function printUpdateHTML()
  {
    # spip_lmc script runs on each client/server, check that it is running and responsive
    $xml = "<status_update>";

    # check if the LMC script is running on the specified host
    $hosts = array_keys($this->topology);
    $port  = $this->config["LMC_PORT"];
    $lmc_socket = new spip_socket();

    $xml_cmd  = XML_DEFINITION;
    $xml_cmd .= "<lmc_cmd>";
    $xml_cmd .= "<requestor>status page</requestor>";
    $xml_cmd .= "<command>host_status</command>";
    $xml_cmd .= "</lmc_cmd>";

    foreach ($hosts as $host)
    {
      $xml .= "<lmc host='".$host."' port='".$port."'>";
      if ($lmc_socket->open ($host, $port, 0) == 0)
      {
        $xml .= "<state>Running</state>";
        $lmc_socket->write ($xml_cmd."\r\n");
        list ($rval, $reply) = $lmc_socket->read();
        $xml .= rtrim($reply);
        $lmc_socket->close();
      }
      else
      {
        $xml .= "<state>Offline</state>";
      }
      $xml .= "</lmc>";
    }

    $xml .= "</status_update>";

    header('Content-type: text/xml');
    echo $xml;
  }
}

if (!isset($_GET["update"]))
  $_GET["single"] = "true";
handleDirect("status");

