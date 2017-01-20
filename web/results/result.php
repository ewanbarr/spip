<?PHP

ini_set("display_errors", "on");
error_reporting(E_ALL);

include_once("../spip_webpage.lib.php");
include_once("../spip.lib.php");
include_once("../spip_socket.lib.php");

class result extends spip_webpage
{
  function result ()
  {
    spip_webpage::spip_webpage ();

    $this->title = "Observation Result";
    $this->nav_item = "result";

    $this->config = spip::get_config();
    $this->streams = array();

    $this->plot_width = 640;
    $this->plot_height = 480;

    $this->beam = isset($_GET["beam"]) ? $_GET["beam"] : "Error";
    $this->utc_start = isset($_GET["utc_start"]) ? $_GET["utc_start"] : "Error";
    $this->source = isset($_GET["source"]) ? $_GET["source"] : "Error";
    $this->source = str_replace("%2B", "+", $this->source);

    $this->host = "Error";
    $this->port = -1;

    for ($istream=0; $istream<$this->config["NUM_STREAM"]; $istream++)
    {
      list ($host, $ibeam, $subband) = explode (":", $this->config["STREAM_".$istream]);
      $beam_name = $this->config["BEAM_".$ibeam];
      if ($beam_name == $this->beam)
      {
        $this->host = $host;
        $this->port =  $this->config["STREAM_RESULTS_PORT"] + $istream;
      }
    }
    $this->update_url = "result.php?update=true&beam=".$this->beam."&utc_start=".$this->utc_start."&source=".str_replace("+", "%2B", $this->source);
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

      function handle_result_request(r_xml_request)
      {
        if (r_xml_request.readyState == 4)
        {
          var xmlDoc = r_xml_request.responseXML;

          if (xmlDoc != null)
          {
            var xmlObj = xmlDoc.documentElement;

            var i;      
            var obs_info = xmlObj.getElementsByTagName("results_obs_info")[0];
            for (i=0; i<obs_info.childNodes.length; i++)
            {
              var param = obs_info.childNodes[i];
              var name = param.tagName;
              if (name == "plot")
              {
                var type = param.getAttribute("type");
                document.getElementById(type).src = "<?php echo $this->update_url;?>&plot="+type;
              }
              else
              {
                try {
                  var value = get_node_value(param);
                  document.getElementById(name).innerHTML = value;
                } catch (e) {
                  alert("ERROR: param=" + name + " value=" + value + " Error=" + e)
                }
              }
            }
          }
        }
      }

      function result_request() 
      {
        var url = "<?php echo $this->update_url;?>";
        if (window.XMLHttpRequest)
          r_xml_request = new XMLHttpRequest();
        else
          r_xml_request = new ActiveXObject("Microsoft.XMLHTTP");

        r_xml_request.onreadystatechange = function()
        {
          handle_result_request(r_xml_request)
        };
        r_xml_request.open("GET", url, true);
        r_xml_request.send(null);
      }

    </script>
<?php
  } 

  function printJavaScriptTail()
  { 
?>
    <script type='text/javascript'>
      result_request();
    </script>
<?php
  }

  function printHTML ()
  {
?>
<h1>Observation Result</h1>

<table cellpadding='3px' border=0 cellspacing=2px width='100%'>
  <tr>
    <td valign=top width='50%'>

      <table>
<?php
        $this->printInfoRow("UTC START", 'utc_start');
        $this->printInfoRow("Beam", 'beam');
        $this->printInfoRow("Source", 'source');
        $this->printInfoRow("RA", 'ra');
        $this->printInfoRow("DEC", 'dec');
        $this->printInfoRow("Centre Frequency", 'centre_frequency');
        $this->printInfoRow("Bandwidth", 'bandwidth');
        $this->printInfoRow("Num Channels", 'nchannels');
        $this->printInfoRow("Subarray ID", 'subarray_id');
        $this->printInfoRow("Project ID", 'project_id');
        $this->printInfoRow("SNR", 'snr');
        $this->printInfoRow("Length", 'length');
?>
      </table>

    </td>

    <td valign=top>
      <table>
<?php
        $this->printImgRow("Flux vs Phase", "flux_vs_phase");
        $this->printImgRow("Freq vs Phase", "freq_vs_phase");
        $this->printImgRow("Time vs Phase", "time_vs_phase");
        $this->printImgRow("Bandpass", "bandpass");
?>
    </td>
  </tr>
</table>

<?php
  }

  function printInfoRow($name, $id)
  { 
    echo "        <tr>\n";
    echo "          <th style=text-align:left;'>".$name."</th>\n";
    echo "          <td><span id='".$id."'></span></td>\n";
    echo "        </tr>\n";
  }

  function printImgRow($name, $id)
  {
    echo "        <tr>\n";
    echo "          <td>\n";
    echo "            <div style='text-align:center;'>".$name."</div>\n";
    echo "            <img id='".$id."' src='' width='".($this->plot_width+1)."px' height='".($this->plot_height+1)."px'/><br/>\n";
    echo "          </td>\n";
    echo "        </tr>\n";
  }

  function printUpdateHTML($get)
  {
    if (isset($get["plot"]))
    {
      $this->renderImage($get);
      return;
    }
    $xml = "<result_update>";

    $result_socket = new spip_socket();

    $xml_req  = XML_DEFINITION;
    $xml_req .= "<results_request>";
    $xml_req .= "<requestor>result page</requestor>";
    $xml_req .= "<type>obs_info</type>";
    $xml_req .= "<beam>".$get["beam"]."</beam>";
    $xml_req .= "<utc_start>".$get["utc_start"]."</utc_start>";
    $xml_req .= "<source>".$get["source"]."</source>";
    $xml_req .= "</results_request>";

    if ($result_socket->open ($this->host, $this->port, 0) == 0)
    {
      $result_socket->write ($xml_req."\r\n");
      list ($rval, $reply) = $result_socket->read();
      $xml .= rtrim($reply);
      $result_socket->close();
    }

    $xml .= "</result_update>";

    header('Content-type: text/xml');
    echo $xml;
  }

  // will contact a resulter to request current image information
  function renderImage($get)
  {
    $beam = $get["beam"];
    $utc_start = $get["utc_start"];
    $source = $get["source"];
    if (($get["plot"] == "flux_vs_phase") ||
        ($get["plot"] == "freq_vs_phase") ||
        ($get["plot"] == "time_vs_phase") ||
        ($get["plot"] == "bandpass"))
    {
      $xml_req  = XML_DEFINITION;
      $xml_req .= "<results_request>";
      $xml_req .= "<requestor>resultpage</requestor>";
      $xml_req .= "<type>plot</type>";
      $xml_req .= "<beam>".$beam."</beam>";
      $xml_req .= "<source>".$source."</source>";
      $xml_req .= "<utc_start>".$utc_start."</utc_start>";
      $xml_req .= "<plot>".$get["plot"]."</plot>";
      $xml_req .= "<xres>".$this->plot_width."</xres>";
      $xml_req .= "<yres>".$this->plot_height."</yres>";
      $xml_req .= "</results_request>";

      $result_socket = new spip_socket();
      $rval = 0;
      $reply = 0;

      if ($result_socket->open ($this->host, $this->port, 0) == 0)
      {
        $result_socket->write ($xml_req."\r\n");
        list ($rval, $reply) = $result_socket->read_raw();
      }
      else
      {
        // TODO generate PNG with error text
        echo "ERROR: could not connect to ".$host.":".$port."<BR>\n";
        return;
      }
      $result_socket->close();

      if ($rval == 0)
      {
        header('Content-type: image/png');
        header('Content-Disposition: inline; filename="image.png"');
        echo $reply;
      }
    }
  }
}

if (!isset($_GET["update"]))
  $_GET["single"] = "true";
handleDirect("result");
