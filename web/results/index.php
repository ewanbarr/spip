<?PHP

ini_set("display_errors", "on");
error_reporting(E_ALL);

include_once("../spip_webpage.lib.php");
include_once("../spip.lib.php");
include_once("../spip_socket.lib.php");

class results extends spip_webpage
{

  function results ()
  {
    spip_webpage::spip_webpage ();

    $this->title = "Recent Results";
    $this->nav_item = "results";

    $this->config = spip::get_config();
    $this->streams = array();

    $this->callback_freq = 60 * 1000;
    $this->inline_images = true;
    $this->plot_width = 160;
    $this->plot_height = 120;

    $this->index = isset($get["index"]) ? $get["index"] : 0;
    $this->count = isset($get["count"]) ? $get["count"] : 20;
    $this->result_url = "";

    for ($istream=0; $istream<$this->config["NUM_STREAM"]; $istream++)
    {
      list ($host, $ibeam, $subband) = explode (":", $this->config["STREAM_".$istream]);

      $beam_name = $this->config["BEAM_".$ibeam];
      $this->streams[$istream] = array("beam_name" => $beam_name, "host" => $host);
    }
  }

  function printJavaScriptTail()
  {
?>
    <script type='text/javascript'>
      results_request();
    </script>
<?php
  }

  function printJavaScriptHead()
  {
?>
    <script type='text/javascript'>

      function get_node_value(node)
      {
        if (node.childNodes.length > 0)
          return node.childNodes[0].nodeValue;
        else
          return "--";
      }

      function handle_results_request(r_xml_request)
      {
        if (r_xml_request.readyState == 4)
        {
          var xmlDoc = r_xml_request.responseXML;

          if (xmlDoc != null)
          {
            var xmlObj = xmlDoc.documentElement;

            var h, i, j, k;      
            var source, name, ra, dec;
            var params, observer, pid, mode, start, elapsed, tobs
            var observation, integrated, snr;
            var idx = 0;

            // process the TCS state first
            var utc_starts = xmlObj.getElementsByTagName("utc_start");
            for (h=0; h<utc_starts.length; h++)
            {
              var utc_start = utc_starts[h];
              var utc = utc_start.getAttribute("utc");
              var sources = utc_start.getElementsByTagName("source");

              for (i=0; i<sources.length; i++)
              {
                source = sources[i];

                var source_name  = source.getAttribute("jname");
                var source_name_html = source_name.replace("+", "%2B");
                var source_index = source.getAttribute("index");

                beam   = get_node_value(source.getElementsByTagName("beam")[0]);
                ra     = get_node_value(source.getElementsByTagName("ra")[0]);
                dec    = get_node_value(source.getElementsByTagName("dec")[0]);
    
                cfreq     = get_node_value(source.getElementsByTagName("centre_frequency")[0]);
                bandwidth = get_node_value(source.getElementsByTagName("bandwidth")[0]);
                pid       = get_node_value(source.getElementsByTagName("project_id")[0]);
                //mode    = get_node_value(source.getElementsByTagName("mode")[0]);
                length    = get_node_value(source.getElementsByTagName("length")[0]);
                //nchannels = get_node_value(source.getElementsByTagName("nchannels")[0]);
                snr       = get_node_value(source.getElementsByTagName("snr")[0]);
                subid     = get_node_value(source.getElementsByTagName("subarray_id")[0]);

                url = "result.php?beam="+beam+"&utc_start="+utc+"&source="+source_name_html

                document.getElementById("utc_start_" + idx).innerHTML = "<a href='"+url+"'>"+utc+"</a>";
                document.getElementById("beam_" + idx).innerHTML = beam;
                document.getElementById("source_" + idx).innerHTML = source_name;
                //document.getElementById("ra_" + idx).innerHTML = ra;
                //document.getElementById("dec_" + idx).innerHTML = dec;
                //document.getElementById("subid_" + idx).innerHTML = subid;
                document.getElementById("pid_" + idx).innerHTML = pid;
                //document.getElementById("mode_" + idx).innerHTML = mode;
                document.getElementById("snr_" + idx).innerHTML = parseFloat(snr).toFixed(2);
                document.getElementById("length_" + idx).innerHTML = parseFloat(length).toFixed(0);
                //document.getElementById("nchannels_" + idx).innerHTML = nchannels;
                document.getElementById("cfreq_" + idx).innerHTML = parseFloat(cfreq).toFixed(2);
                document.getElementById("bw_" + idx).innerHTML = bandwidth;
                base = "/spip/results/index.php?update=true&beam="+beam+"&utc_start="+utc+"&source="+source_name_html;
                document.getElementById("profile_" + idx).src = base + "&plot=flux_vs_phase";
                document.getElementById("freq_" + idx).src = base + "&plot=freq_vs_phase";
                document.getElementById("time_" + idx).src = base + "&plot=time_vs_phase";
                idx++;
              }
            }
          }
        }
      }

      function results_request() 
      {
        var url = "?update=true";

        if (window.XMLHttpRequest)
          r_xml_request = new XMLHttpRequest();
        else
          r_xml_request = new ActiveXObject("Microsoft.XMLHTTP");

        r_xml_request.onreadystatechange = function()
        {
          handle_results_request(r_xml_request)
        };
        r_xml_request.open("GET", url, true);
        r_xml_request.send(null);
      }

    </script>
<?php
  } 

  function printHTML ()
  {
    #$results = $this->getResults($this->results_dir, $this->offset, $this->length, $this->filter_type, $this->filter_value);

    $results = array();
    $keys = array_keys($results);
    rsort($keys);

?>
<h1>Recent Observations</h1>

<table cellpadding='3px' border=0 cellspacing=2px width='100%'>

  <tr>
    <th style='text-align: left;'>UTC START</th>
    <th style='text-align: left;'>Beam</th>
    <th style='text-align: left;'>Source</th>
    <th style='text-align: left;'>Centre Freq</th>
    <th style='text-align: left;'>Bandwidth</th>
    <!--<th style='text-align: left;'>Num Channels</th>-->
    <th style='text-align: left;'>Project ID</th>
    <th style='text-align: left;'>SNR</th>
    <th style='text-align: left;'>Length</th>
    <th style='text-align: left;'>Profile</th>
    <th style='text-align: left;'>Time</th>
    <th style='text-align: left;'>Freq</th>
  </tr>
<?php

  if ($this->inline_images == "true")
    $style = "";
  else
    $style = "display: none;";

  $this->class = "processing";
  $bg_style = "class='processing'";

  $k = "";
  $r = array("utc_start" => "", "beam" => "", "source" => "", "cfreq" => "", "bw" => "", "nchannels" => "", "pid" => "", "snr" => "", "length" => "", "profile" => "", "time" => "", "freq" => "");
  for ($i=0; $i < $this->count; $i++)
  {
    #$k = $keys[$i];
    #$r = $results[$k];

    $this->printResultRow($i, $k, $r);
  }
?>
</table>

<?php
  }

  function printResultRow($i, $k, $r)
  {
    $url = $this->result_url."?single=true&utc_start=".$k."&class=".$this->class;

    echo "  <tr id='row_".$i."' class='finished'>\n";

    $this->printResultCellSpan("utc_start_".$i, "<a id='link_".$i."' href='".$url."'>".$k."</a>");
    $this->printResultCellSpan("beam_".$i, $r["beam"]);
    $this->printResultCellSpan("source_".$i, $r["source"]);
    $this->printResultCellSpan("cfreq_".$i, $r["cfreq"]);
    $this->printResultCellSpan("bw_".$i, $r["bw"]);
    //$this->printResultCellSpan("nchannels_".$i, $r["nchannels"]);
    $this->printResultCellSpan("pid_".$i, $r["pid"]);
    $this->printResultCellSpan("snr_".$i, $r["snr"]);
    $this->printResultCellSpan("length_".$i, $r["length"]);
    $this->printResultCellImg("profile_".$i, $r["profile"]);
    $this->printResultCellImg("time_".$i, $r["time"]);
    $this->printResultCellImg("freq_".$i, $r["freq"]);

    echo "  </tr>\n";
  }

  function printResultCellSpan($id, $val)
  {
    $bg_style = "class='processing'";
    echo "    <td ".$bg_style."><span id='".$id."'>".$val."</span></td>\n";
  }

  function printResultCellImg($id, $img)
  {
    if ($this->inline_images == "true")
      $style = "";
    else
      $style = "display: none;";

    echo "    <td class='finished'><img style='".$style."' id='".$id."' src='".$img."' width=64 height=48/></td>\n";
  }

  function printUpdateHTML($get)
  {
    if (isset($get["plot"]))
    {
      $this->renderImage($get);
      return;
    }
    $xml = "<results_update>";

    foreach ($this->streams as $istream => $stream)
    {
      $results_socket = new spip_socket();

      $host = $stream["host"];
      $port = $this->config["STREAM_RESULTS_PORT"] + $istream;
      $beam_name = $stream["beam_name"];

      $xml_req  = XML_DEFINITION;
      $xml_req .= "<results_request>";
      $xml_req .= "<requestor>results page</requestor>";
      $xml_req .= "<type>obs_list</type>";
      $xml_req .= "<index>0</index>";
      $xml_req .= "<count>20</count>";
      $xml_req .= "</results_request>";

      if ($results_socket->open ($host, $port, 0) == 0)
      {
        $results_socket->write ($xml_req."\r\n");
        list ($rval, $reply) = $results_socket->read();
        $xml .= rtrim($reply);
        $results_socket->close();
      }
      else
      {
        $xml .= "<results_utc_list/>";
      }
    }

    $xml .= "</results_update>";

    header('Content-type: text/xml');
    echo $xml;
  }

  // will contact a resultser to request current image information
  function renderImage($get)
  {
    $utc_start = $get["utc_start"];
    $source = $get["source"];
    if (($get["plot"] == "flux_vs_phase") ||
        ($get["plot"] == "freq_vs_phase") ||
        ($get["plot"] == "time_vs_phase") ||
        ($get["plot"] == "bandpass"))
    {
      $host = "unknown";
      $port = "-1";
      foreach ($this->streams as $istream => $stream)
      {
        if ($stream["beam_name"] == $get["beam"])
        {
          $host = $stream["host"];
          $port = $this->config["STREAM_RESULTS_PORT"] + $istream;
        }
      }

      $xml_req  = XML_DEFINITION;
      $xml_req .= "<results_request>";
      $xml_req .= "<requestor>resultspage</requestor>";
      $xml_req .= "<type>plot</type>";
      $xml_req .= "<beam>".$get["beam"]."</beam>";
      $xml_req .= "<source>".$source."</source>";
      $xml_req .= "<utc_start>".$utc_start."</utc_start>";
      $xml_req .= "<plot>".$get["plot"]."</plot>";
      $xml_req .= "</results_request>";

      $results_socket = new spip_socket();
      $rval = 0;
      $reply = 0;

      if ($results_socket->open ($host, $port, 0) == 0)
      {
        $results_socket->write ($xml_req."\r\n");
        list ($rval, $reply) = $results_socket->read_raw();
      }
      else
      {
        // TODO generate PNG with error text
        echo "ERROR: could not connect to ".$host.":".$port."<BR>\n";
        return;
      }
      $results_socket->close();

      if ($rval == 0)
      {
        header('Content-type: image/png');
        header('Content-Disposition: inline; filename="image.png"');
        echo $reply;
      }
    }
  }
    
  function getResults($dir, $offset=0, $length=0, $filter_type, $filter_value)
  {
    $all = array();
    
    // BEAM / UTC_START / SOURCE 
    $cmd = "find ".$dir." -mindepth 3 -maxdepth 3 -type d";
    $dirs = array();
    $last = exec($cmd, $dirs, $rval);
    
    foreach ($dirs as $dir)
    {
      $parts = explode ("/", $dir);
      $np = count($parts);
      $source = $parts[$np-1];
      $utc_start = $parts[$np-2];
      $beam = $parts[$np-3];

      $obs = array();
      $obs["utc_start"] = $utc_start;
      $obs["beam"] = $beam;
      $obs["source"] = $source;

      # now extract key parameters from thre header file
      $cmd = "find ".$dir."/".$beam."/".$utc_start."/".$source." -name 'obs.header.*'";
      $lines = array();
      $header_file = exec($cmd, $lines, $rval);
      $header = spip::get_config_file($header_file);
      $obs["cfreq"] = $header["FREQ"];
      $obs["bw"] = $header["BW"];

      # now find existing PNG files
      $cmd = "find ".$dir."/".$beam."/".$utc_start."/".$source." -name '*' -printf '%f\n'";
      $lines = array();
      $last = exec($cmd, $lines, $rval);

      foreach ($lines as $file)
      {
      }

      $obs["length"] = "TBD";
      $obs["length"] = "TBD";
    }

    return $all;
  }

}

if (!isset($_GET["update"]))
  $_GET["single"] = "true";
handleDirect("results");
