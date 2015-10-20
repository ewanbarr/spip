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
    }
  }

  function printHTML()
  {
    echo "<h1>Timing</h1>\n";
    echo "<p>Here is the pulsar timing page</p>\n";

    echo "<table  width='100%' border=1>\n";
    echo "<tr><th>Beam</th><th>Flux</th><th>Time</th><th>Freq</th><th>Details</th></tr>\n";

    foreach ($this->beams as $ibeam => $beam)
    {
      echo "<tr>\n";
      echo   "<td>".$beam["name"]."</td>\n";
      echo   "<td><img src='' id='".$ibeam."_flux'/></td>\n";
      echo   "<td><img src='' id='".$ibeam."_time'/></td>\n";
      echo   "<td><img src='' id='".$ibeam."_freq'/></td>\n";
      echo   "<td><span id='".$ibeam."_details'></span></td>\n";
      echo "</tr>\n";
    }
    echo "</table>\n";
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
        $xml .= "<repack_state><beam id='".$beam["name"]."'></beam></repack_state>";
      }
    }

    $xml .= "</timing_update>";

    header('Content-type: text/xml');
    echo $xml;
  }

  // will contact a repacker to request current image information
  function renderImage($get)
  {
    $ibeam     = $get["ibeam"];
    $beam_name = $this->beams[$ibeam]["name"];
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
    $reply = 0;
    if ($repack_socket->open ($host, $port, 0) == 0)
    {
      $repack_socket->write ($xml_req."\r\n");
      list ($rval, $reply) = $repack_socket->read();
    }
    else
    {
      // TODO generate PNG with error text
      echo "ERROR: could not connect to ".$host.":".$port."<BR>\n";
    }
    $repack_socket->close();

    header('Content-type: image/png');
    echo $reply;
  }
}
if (!isset($_GET["update"]))
  $_GET["single"] = "true";
handleDirect("timing");

