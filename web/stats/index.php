<?PHP

error_reporting(E_ALL);
ini_set("display_errors", 1);

include_once("../spip.lib.php");
include_once("../spip_webpage.lib.php");
include_once("../spip_socket.lib.php");

class stat extends spip_webpage
{

  function stat()
  {
    spip_webpage::spip_webpage();

    $this->title = "Input Statistics";
    $this->nav_item = "stats";

    $this->config = spip::get_config();
    $this->streams = array();

    $this->plot_width = 240;
    $this->plot_height = 180;

    for ($istream=0; $istream<$this->config["NUM_STREAM"]; $istream++)
    {
      list ($host, $ibeam, $subband) = explode (":", $this->config["STREAM_".$istream]);

      $beam_name = $this->config["BEAM_".$ibeam];
      $this->streams[$istream] = array("beam_name" => $beam_name, "host" => $host);
    }
  }

  function javaScriptCallback()
  {
    return "stat_request();";
  }

  function printJavaScriptHead()
  {
?>
    <script type='text/javascript'>

      function handle_stat_request(t_xml_request)
      {
        if (t_xml_request.readyState == 4)
        {
          var xmlDoc = t_xml_request.responseXML;
          var tcs_utcs = new Array();

          if (xmlDoc != null)
          {
            var xmlObj = xmlDoc.documentElement;

            // process the TCS state first
            var stat_states = xmlObj.getElementsByTagName("stat_state");
            for (h=0; h<stat_states.length; h++)
            {
              var stat_state = stat_states[h];
              var streams = stat_state.getElementsByTagName("stream");

              var i, j, k;      
              for (i=0; i<streams.length; i++)
              {
                var stream = streams[i];

                var stream_id = stream.getAttribute("id");
                var beam_name = stream.getAttribute("beam_name");
                var active    = stream.getAttribute("active");

                var plots = Array();

                if (active == "True")
                {
                  var polarisations = stream.getElementsByTagName("polarisation");
                  for (j=0; j<polarisations.length; j++)
                  {
                    polarisation = polarisations[j];
                    pol_name = polarisation.getAttribute("name")

                    var dimensions = polarisation.getElementsByTagName ("dimension");
                    for (k=0; k<dimensions.length; k++)
                    {
                      var dimension = dimensions[k];
                      var dim_name = dimension.getAttribute("name")
                      if (dim_name != "none")
                      {
                        var hg_mean   = parseFloat(dimension.getElementsByTagName("histogram_mean")[0].childNodes[0].nodeValue);
                        var hg_stddev = parseFloat(dimension.getElementsByTagName("histogram_stddev")[0].childNodes[0].nodeValue);
                        var hg_mean_id = stream_id + "_histogram_mean_" + pol_name + "_" + dim_name;
                        var hg_stddev_id = stream_id + "_histogram_stddev_" + pol_name + "_" + dim_name;
              
                        document.getElementById(hg_mean_id).innerHTML = hg_mean.toFixed(2);
                        document.getElementById(hg_stddev_id).innerHTML = hg_stddev.toFixed(2);
                      }

                      plots = dimension.getElementsByTagName("plot");
                      for (l=0; l<plots.length; l++)
                      {
                        var plot = plots[l]
                        var plot_type = plot.getAttribute("type")  
                        var plot_timestamp = plot.getAttribute("timestamp")  
    
                        var plot_id = stream_id + "_" + plot_type + "_" + pol_name + "_" + dim_name
                        var plot_ts = stream_id + "_" + plot_type + "_" + pol_name + "_" + dim_name + "_ts"

                        // if the image has been updated, reacquire it
                        //alert (plot_timestamp + " ?=? " + document.getElementById(plot_ts).value)
                        if (plot_timestamp != document.getElementById(plot_ts).value)
                        {
                          url = "/spip/stats/index.php?update=true&istream="+stream_id+"&type=plot&plot="+plot_type+"&pol="+pol_name+"&dim="+dim_name+"&ts="+plot_timestamp;
                          document.getElementById(plot_id).src = url;
                          document.getElementById(plot_ts).value = plot_timestamp;
                        }
                      }
                    }
                  }
                }
                else
                {
                  // assume 2 pols and 2 dims [TODO fix]
                  var polarisations = Array("0", "1");
                  var dimensions = Array("real", "imag");
                  for (j=0; j<polarisations.length; j++)
                  {
                    pol_name = polarisations[j];
                    for (k=0; k<dimensions.length; k++)
                    {
                      dim_name = dimensions[k];
                      var hg_mean_id = stream_id + "_histogram_mean_" + pol_name + "_" + dim_name;  
                      var hg_stddev_id = stream_id + "_histogram_stddev_" + pol_name + "_" + dim_name;
                      document.getElementById(hg_mean_id).innerHTML = "--";
                      document.getElementById(hg_stddev_id).innerHTML = "--";
                    }
                  }
                }
              }
            }
          }
        }
      }

      function stat_request() 
      {
        var url = "?update=true&pref_chan=10";

        if (window.XMLHttpRequest)
          t_xml_request = new XMLHttpRequest();
        else
          t_xml_request = new ActiveXObject("Microsoft.XMLHTTP");

        t_xml_request.onreadystatechange = function()
        {
          handle_stat_request(t_xml_request)
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
    foreach ($this->streams as $istream => $stream)
    {
      $this->renderObsTable($istream);
      $this->renderPlotTable($istream);
    }
  }

  function printUpdateHTML($get)
  {
    if (isset($get["plot"]))
    {
      $this->renderImage($get);
      return;
    }
    $xml = "<stat_update>";

    
    $xml_req  = XML_DEFINITION;
    $xml_req .= "<stat_request>";
    $xml_req .= "<requestor>stat page</requestor>";
    $xml_req .= "<type>state</type>";
    $xml_req .= "<pref_chan>".$get["pref_chan"]."</pref_chan>";
    $xml_req .= "</stat_request>";

    foreach ($this->streams as $istream => $stream)
    {
      $stat_socket = new spip_socket();

      $host = $stream["host"];
      $port = $this->config["STREAM_STAT_PORT"] + $istream;

      #echo "host=".$host." port=".$port."<br>\n";
      if ($stat_socket->open ($host, $port, 0) == 0)
      {
        $stat_socket->write ($xml_req."\r\n");
        list ($rval, $reply) = $stat_socket->read();
        $xml .= rtrim($reply);
        $stat_socket->close();
      }
      else
      {
        $xml .= "<stat_state><stream id='".$istream."' beam_name='".$stream["beam_name"]."' active='False'></stream></stat_state>";
      }
    }

    $xml .= "</stat_update>";

    header('Content-type: text/xml');
    echo $xml;
  }

  // will contact stat to request current image information
  function renderImage($get)
  {
    $istream = $get["istream"];
    $host      = $this->streams[$istream]["host"];
    $port      = $this->config["STREAM_STAT_PORT"] + $istream;

    $xml_req  = XML_DEFINITION;
    $xml_req .= "<stat_request>";
    $xml_req .= "<requestor>stat page</requestor>";
    $xml_req .= "<type>plot</type>";
    $xml_req .= "<plot>".$get["plot"]."</plot>";
    $xml_req .= "<pol>".$get["pol"]."</pol>";
    $xml_req .= "<dim>".$get["dim"]."</dim>";
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

  function renderObsTable ($stream)
  {
    $cols = 4;
    $fields = array( $stream."_histogram_mean_0_real" => "Mean [0 Re]",
                     $stream."_histogram_mean_0_imag" => "Mean [0 Im]",
                     $stream."_histogram_mean_1_real" => "Mean [1 Re]",
                     $stream."_histogram_mean_1_imag" => "Mean [1 Im]",
                     $stream."_histogram_stddev_0_real" => "StdDev [0 Re]",
                     $stream."_histogram_stddev_1_real" => "StdDev [1 Re]",
                     $stream."_histogram_stddev_0_imag" => "StdDev [0 Im]",
                     $stream."_histogram_stddev_1_imag" => "StdDev [1 Im]" );

    echo "<table id='obsTable' width='100%' border=0>\n";

    echo "<tr><th colspan=8 style='text-align: center;'>Stream ".$stream."</th></tr>\n";

    echo "<tr><th colspan=4 style='text-align: center;'>Pol 0</th>".
             "<th colspan=4 style='text-align: center;'>Pol 1</th></tr>\n";


    echo "<tr>\n";
    echo "<th width='10%'>Mean</th>\n";
    echo "<td width='15%'>(<span id='".$stream."_histogram_mean_0_real'></span>,".
                           "<span id='".$stream."_histogram_mean_0_imag'></span>)</td>\n";

    echo "<th width='10%'>Std. Dev.</th>\n";
    echo "<td width='15%'>(<span id='".$stream."_histogram_stddev_0_real'></span>,".
                           "<span id='".$stream."_histogram_stddev_0_imag'></span>)</td>\n";

    echo "<th width='10%'>Mean</th>\n";
    echo "<td width='15%'>(<span id='".$stream."_histogram_mean_1_real'></span>,".
                           "<span id='".$stream."_histogram_mean_1_imag'></span>)</td>\n";
    echo "<th width='10%'>Std. Dev.</th>\n";
    echo "<td width='15%'>(<span id='".$stream."_histogram_stddev_1_real'></span>,".
                           "<span id='".$stream."_histogram_stddev_1_imag'></span>)</td>\n";
    
    echo "</tr>\n";

    /*
    $keys = array_keys($fields);
    for ($i=0; $i<count($keys); $i++)
    {
      if ($i % $cols == 0)
        echo "  <tr>\n";
      echo "    <th>".$fields[$keys[$i]]."</th>\n";
      echo "    <td width='100px'><span id='".$keys[$i]."'>--</span></td>\n";
      if (($i+1) % $cols == 0)
        echo "  </tr>\n";
    }
    */
    echo "</table>\n";
  }

  function renderPlotTable ($stream)
  {
    $img_params = "src='/spip/images/blankimage.gif' width='".$this->plot_width."px' height='".$this->plot_height."px'";

    echo "<table  width='100%' id='plotTable'>\n";
    echo "<tr>\n";
    echo   "<td><img id='".$stream."_histogram_0_real' ".$img_params."/><input type='hidden' id='".$stream."_histogram_0_real_ts'/></td>\n";
    echo   "<td><img id='".$stream."_histogram_0_imag' ".$img_params."/><input type='hidden' id='".$stream."_histogram_0_imag_ts'/></td>\n";
    echo   "<td><img id='".$stream."_histogram_1_real' ".$img_params."/><input type='hidden' id='".$stream."_histogram_1_real_ts'/></td>\n";
    echo   "<td><img id='".$stream."_histogram_1_imag' ".$img_params."/><input type='hidden' id='".$stream."_histogram_1_imag_ts'/></td>\n";
    echo "</tr>\n";
    echo "<tr><td>Input HG Real</td><td>Input HG Imag</td><td>Input HG Real</td><td>Input HG Imag</td></tr>\n";

    echo "<tr>\n";
    echo   "<td><img id='".$stream."_freq_vs_time_0_none' ".$img_params."/><input type='hidden' id='".$stream."_freq_vs_time_0_none_ts'/></td>\n";
    echo   "<td><img id='".$stream."_histogram_0_none' ".$img_params."/><input type='hidden' id='".$stream."_histogram_0_none_ts'/></td>\n";
    echo   "<td><img id='".$stream."_freq_vs_time_1_none' ".$img_params."/><input type='hidden' id='".$stream."_freq_vs_time_1_none_ts'/></td>\n";
    echo   "<td><img id='".$stream."_histogram_1_none' ".$img_params."/><input type='hidden' id='".$stream."_histogram_1_none_ts'/></td>\n";
    echo "<td></td>\n";
    echo "</tr>\n";
    echo "<tr><td>Freq v Time</td><td></td><td>Freq v Time</td><td></td></tr>\n";

    echo "</table>\n";
  }
    
}
if (!isset($_GET["update"]))
  $_GET["single"] = "true";
handleDirect("stat");

