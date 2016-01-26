<?PHP

error_reporting(E_ALL);
ini_set("display_errors", 1);

include_once("../spip.lib.php");
include_once("../spip_webpage.lib.php");
include_once("../spip_socket.lib.php");

class timing extends spip_webpage
{

  function timing()
  {
    spip_webpage::spip_webpage();

    $this->title = "Pulsar Timing";
    $this->nav_item = "timing";

    $this->config = spip::get_config();
    $this->beams = array();
    $this->streams = array();

    $this->plot_width = 240;
    $this->plot_height = 180;


    for ($ibeam=0; $ibeam<$this->config["NUM_BEAM"]; $ibeam++)
    {
      $beam_name = $this->config["BEAM_".$ibeam];
      $primary_subband = $this->config["NUM_SUBBAND"];
      $primary_stream = -1;

      # find the lowest indexed stream for this beam
      for ($istream=0; $istream<$this->config["NUM_STREAM"]; $istream++)
      {
        list ($host, $beam, $subband) = explode (":", $this->config["STREAM_".$istream]);
        if (($beam == $ibeam) && ($subband < $primary_subband))
        {
          $primary_subband = $subband;
          $primary_stream = $istream;
        }
      }

      list ($host, $beam, $subband) = explode (":", $this->config["STREAM_".$primary_stream]);
      $this->beams[$ibeam] = array ("name" => $beam_name, "host" => $host);
      $this->streams[$beam_name] = $primary_stream;
    }
  }

  function javaScriptCallback()
  {
    return "timing_request();";
  }

  function printJavaScriptHead()
  {
?>
    <script type='text/javascript'>

      function handle_timing_request(t_xml_request)
      {
        if (t_xml_request.readyState == 4)
        {
          var xmlDoc = t_xml_request.responseXML;
          var tcs_utcs = new Array();

          if (xmlDoc != null)
          {
            var xmlObj = xmlDoc.documentElement;

            // process the TCS state first
            var tcs_state = xmlObj.getElementsByTagName("tcs_state")[0];
            var beams = tcs_state.getElementsByTagName("beam");

            var i, j, k;      
            var source, name, ra, dec;
            var params, observer, pid, mode, start, elapsed, tobs
            var observation, integrated, snr;

            for (i=0; i<beams.length; i++)
            {
              beam = beams[i];
              var beam_name  = beam.getAttribute("name");
              var beam_state = beam.getAttribute("state");

              document.getElementById(beam_name + "_state").innerHTML = "Beam " + beam_name + ": " + beam_state

              if (beam_state == "Recording")
              {
                source = beam.getElementsByTagName("source")[0];
                name   = source.getElementsByTagName("name")[0].childNodes[0].nodeValue;
                ra     = source.getElementsByTagName("ra")[0].childNodes[0].nodeValue;
                dec    = source.getElementsByTagName("dec")[0].childNodes[0].nodeValue;
    
                params   = beam.getElementsByTagName("observation_parameters")[0];
                observer = params.getElementsByTagName("observer")[0].childNodes[0].nodeValue;
                pid      = params.getElementsByTagName("pid")[0].childNodes[0].nodeValue;
                mode     = params.getElementsByTagName("mode")[0].childNodes[0].nodeValue;
                start    = params.getElementsByTagName("utc_start")[0].childNodes[0].nodeValue;
                elapsed  = params.getElementsByTagName("elapsed_time")[0].childNodes[0].nodeValue;
                tobs     = params.getElementsByTagName("expected_length")[0].childNodes[0].nodeValue;

                document.getElementById(beam_name + "_source").innerHTML = name;
                document.getElementById(beam_name + "_ra").innerHTML = ra;
                document.getElementById(beam_name + "_dec").innerHTML = dec;
                document.getElementById(beam_name + "_observer").innerHTML = observer;
                document.getElementById(beam_name + "_pid").innerHTML = pid;
                document.getElementById(beam_name + "_mode").innerHTML = mode;
                document.getElementById(beam_name + "_start").innerHTML = start;
                document.getElementById(beam_name + "_elapsed").innerHTML = elapsed;
                document.getElementById(beam_name + "_tobs").innerHTML = tobs;

                tcs_utcs[beam_name] = start
              }
              else
              {
                tcs_utcs[beam_name] = ""
              }
            }

            var repack_state = xmlObj.getElementsByTagName("repack_state")[0];
            var beams = repack_state.getElementsByTagName("beam");
  
            for (i=0; i<beams.length; i++)
            {
              var beam = beams[i];

              var beam_name = beam.getAttribute("name");
              var active    = beam.getAttribute("active");

              var plots = Array();

              if (active == "True")
              {
                observation = beam.getElementsByTagName("observation")[0]
                start = observation.getElementsByTagName("start")[0].childNodes[0].nodeValue;
                integrated = parseFloat(observation.getElementsByTagName("integrated")[0].childNodes[0].nodeValue);
                snr = parseFloat(observation.getElementsByTagName("snr")[0].childNodes[0].nodeValue);
                plots = beam.getElementsByTagName("plot");
    
                if (start = tcs_utcs[beam_name])
                {
                  document.getElementById(beam_name + "_integrated").innerHTML = integrated.toFixed(2);
                  document.getElementById(beam_name + "_snr").innerHTML = snr.toFixed(2);
                }

                for (j=0; j<plots.length; j++)
                {
                  var plot = plots[j]
                  var plot_type = plots[j].getAttribute("type")  
                  var plot_timestamp = plots[j].getAttribute("timestamp")  
  
                  var plot_id = beam_name + "_" + plot_type
                  var plot_ts = beam_name + "_" + plot_type + "_ts"

                  // if the image has been updated, reacquire it
                  //alert (plot_timestamp + " ?=? " + document.getElementById(plot_ts).value)
                  if (plot_timestamp != document.getElementById(plot_ts).value)
                  {
                    //var new_image = new Image();
                    //new_image.id = plot_id;
                    //new_image.src = "/spip/timing/index.php?update=true&beam_name="+
                    //                beam_name+"&pol=0&type=plot&plot="+plot_type+"&ts="+plot_timestamp;;
                    //new_image.onload = function() {
                    //  var img = document.getElementById(plot_id);
                    //  img.parentNode.insertBefore(new_image, img);
                    //  img.parentNode.removeChild(img);
                    //}

                    url = "/spip/timing/index.php?update=true&beam_name="+beam_name+"&type=plot&pol=0&plot="+plot_type+"&ts="+plot_timestamp;
                    document.getElementById(plot_id).src = url;
                    document.getElementById(plot_ts).value = plot_timestamp;
                  }
                }
              }
            }
          }
        }
      }

      function timing_request() 
      {
        var url = "?update=true";

        if (window.XMLHttpRequest)
          t_xml_request = new XMLHttpRequest();
        else
          t_xml_request = new ActiveXObject("Microsoft.XMLHTTP");

        t_xml_request.onreadystatechange = function()
        {
          handle_timing_request(t_xml_request)
        };
        t_xml_request.open("GET", url, true);
        t_xml_request.send(null);
      }


    </script>

    <style type='text/css'>
      #obsTable table {
        border: 1;
      }

      #obsTable th {
        text-align: right;
        padding-right: 5px;
      }

      #obsTable td {
        text-align: left;
      }

      #plotTable table {
        border: 0;
      }

      #plotTable td {
        text-align: center;
      }

    </style>
<?php
  }

  function printHTML()
  {
    foreach ($this->beams as $ibeam => $beam)
    {
      echo "<h2 id='".$beam["name"]."_state'>Beam ".$beam["name"]."</h2>\n";

      $this->renderObsTable($beam["name"]);

      $this->renderPlotTable($beam["name"], $this->streams[$beam["name"]]);
    }
  }

  function printUpdateHTML($get)
  {
    if (isset($get["plot"]))
    {
      $this->renderImage($get);
      return;
    }
    $xml = "<timing_update>";

    $repack_socket = new spip_socket();
    
    $xml_req  = XML_DEFINITION;
    $xml_req .= "<repack_request>";
    $xml_req .= "<requestor>timing page</requestor>";
    $xml_req .= "<type>state</type>";
    $xml_req .= "</repack_request>";

    foreach ($this->beams as $ibeam => $beam)
    {
      $host = $beam["host"];
      $port = $this->config["STREAM_REPACK_PORT"] + $ibeam;

      if ($repack_socket->open ($host, $port, 0) == 0)
      {
        $repack_socket->write ($xml_req."\r\n");
        list ($rval, $reply) = $repack_socket->read();
        $xml .= rtrim($reply);
        $repack_socket->close();
      }
      else
      {
        $xml .= "<repack_state><beam beam_name='".$beam["name"]."' active='False'></beam></repack_state>";
      }
    }

    # get all information from TCS too
    $host = $this->config["TCS_INTERFACE_HOST"];
    $port = $this->config["TCS_REPORT_PORT"];

    $tcs_socket = new spip_socket();
    if ($tcs_socket->open ($host, $port, 0) == 0)
    {
      $tcs_socket->write ($xml_req."\r\n");
      list ($rval, $reply) = $tcs_socket->read();
      $xml .= rtrim($reply);
      $tcs_socket->close();
    }
    else
    {
      $xml .= "<tcs_state></tcs_state>";
    }

    $xml .= "</timing_update>";

    header('Content-type: text/xml');
    echo $xml;
  }

  // will contact a repacker to request current image information
  function renderImage($get)
  {
    $beam_name     = $get["beam_name"];
    $ibeam = -1;
    foreach ($this->beams as $ibeam => $beam)
    {
      if ($beam["name"] == $beam_name)
        $ibeam = $ibeam;
    }
    if ($ibeam < 0)
    {
      echo "ERROR: could not identify beam name<br/>\n";
      return;
    }

    if (($get["plot"] == "flux_vs_phase") ||
        ($get["plot"] == "freq_vs_phase") ||
        ($get["plot"] == "time_vs_phase") ||
        ($get["plot"] == "bandpass") ||
        ($get["plot"] == "snr_vs_time"))
    {
      $host      = $this->beams[$ibeam]["host"];
      $port      = $this->config["STREAM_REPACK_PORT"];
      if ($ibeam >= 0)
        $port += $ibeam;

      $xml_req  = XML_DEFINITION;
      $xml_req .= "<repack_request>";
      $xml_req .= "<requestor>timing page</requestor>";
      $xml_req .= "<type>plot</type>";
      $xml_req .= "<beam>".$beam_name."</beam>";
      $xml_req .= "<plot>".$get["plot"]."</plot>";
      $xml_req .= "</repack_request>";

      $repack_socket = new spip_socket(); 
      $rval = 0;
      $reply = 0;
      if ($repack_socket->open ($host, $port, 0) == 0)
      {
        $repack_socket->write ($xml_req."\r\n");
        list ($rval, $reply) = $repack_socket->read_raw();
      }
      else
      {
        // TODO generate PNG with error text
        echo "ERROR: could not connect to ".$host.":".$port."<BR>\n";
        return;
      }
      $repack_socket->close();
      
      if ($rval == 0)
      {
        header('Content-type: image/png');
        header('Content-Disposition: inline; filename="image.png"');
        echo $reply;
      }
    }
    else if (($get["plot"] == "histogram") || 
             ($get["plot"] == "freq_vs_time"))
    {
      $istream   = $this->streams[$beam_name];
      $host      = $this->beams[$ibeam]["host"];
      $port      = $this->config["STREAM_STAT_PORT"] + $istream;

      $xml_req  = XML_DEFINITION;
      $xml_req .= "<stat_request>";
      $xml_req .= "<requestor>stat page</requestor>";
      $xml_req .= "<type>plot</type>";
      $xml_req .= "<plot>".$get["plot"]."</plot>";
      $xml_req .= "<pol>".$get["pol"]."</pol>";
      $xml_req .= "</stat_request>";

      $stat_socket = new spip_socket();
      $rval = 0;
      $reply = 0;
      if ($stat_socket->open ($host, $port, 0) == 0)
      {
        $stat_socket->write ($xml_req."\r\n");
        list ($rval, $reply) = $stat_socket->read_raw();
      }
      else
      {
        // TODO generate PNG with error text
        echo "ERROR: could not connect to ".$host.":".$port."<BR>\n";
        return;
      }
      $stat_socket->close();

      if ($rval == 0)
      {
        header('Content-type: image/png');
        header('Content-Disposition: inline; filename="image.png"');
        echo $reply;
      }
    }
    else
    {
      ;
    }
  }

  function renderObsTable ($beam)
  {
    $cols = 4;
    $fields = array( $beam."_source" => "Source",
                     $beam."_start" => "UTC_START",
                     $beam."_pid" => "Project ID",
                     $beam."_tobs" => "Tobs",
                     $beam."_ra" => "RAJ",
                     $beam."_mode" => "Mode",
                     $beam."_observer" => "Observer",
                     $beam."_elapsed" => "Elapsed",
                     $beam."_dec" => "DECJ",
                     $beam."_snr" => "SNR",
                     $beam."_integrated" => "Integrated");

    echo "<table id='obsTable' width='100%'>\n";

    $keys = array_keys($fields);
    for ($i=0; $i<count($keys); $i++)
    {
      if ($i % $cols == 0)
        echo "  <tr>\n";
      echo "    <th>".$fields[$keys[$i]]."</th>\n";
      echo "    <td><span id='".$keys[$i]."'>--</span></td>\n";
      if (($i+1) % $cols == 0)
        echo "  </tr>\n";
    }
    echo "</table>\n";
  }

  function renderPlotTable ($beam, $stream)
  {
    $img_params = "src='/spip/images/blankimage.gif' width='".$this->plot_width."px' height='".$this->plot_height."px'";

    echo "<table  width='100%' id='plotTable'>\n";

    echo "<tr>\n";
    echo   "<td><img id='".$beam."_flux_vs_phase' ".$img_params."/><input type='hidden' id='".$beam."_flux_vs_phase_ts' value='not set'/></td>\n";
    echo   "<td><img id='".$beam."_freq_vs_phase' ".$img_params."/><input type='hidden' id='".$beam."_freq_vs_phase_ts' value='not set'/></td>\n";
    echo   "<td><img id='".$beam."_time_vs_phase' ".$img_params."/><input type='hidden' id='".$beam."_time_vs_phase_ts' value='not set'/></td>\n";
    echo   "<td><img id='".$beam."_bandpass' ".$img_params."/><input type='hidden' id='".$beam."_bandpass_ts' value='not set'/></td>\n";
    echo "<tr><td>Flux</td><td>Freq</td><td>Time</td><td>Bandpass</td></tr>\n";
    echo "</tr>\n";
    echo "<table>\n";

    echo "<table  width='100%' id='plotTable'>\n";
    echo "<tr>\n";
    echo   "<td><img id='".$beam."_snr_vs_time' ".$img_params."/><input type='hidden' id='".$beam."_snr_vs_time_ts' value='not set'/></td>\n";
    echo   "<td><img id='".$beam."_snr_histogram' ".$img_params."/><input type='hidden' id='".$beam."_snr_histogram_ts'/></td>\n";
    echo   "<td><img id='".$stream."_input_histogram' ".$img_params."/><input type='hidden' id='".$stream."_input_histogram_ts'/></td>\n";
    echo   "<td><img id='".$stream."_freq_vs_time' ".$img_params."/><input type='hidden' id='".$stream."_freq_vs_time_ts'/></td>\n";
    echo "<tr><td>SNR</td><td>SNR HG</td><td>Input HG</td><td>Freq vs Time</td></tr>\n";
    echo "</tr>\n";

    echo "</table>\n";
  }
    
}
if (!isset($_GET["update"]))
  $_GET["single"] = "true";
handleDirect("timing");

