<?PHP

ini_set("display_errors", "on");
error_reporting(E_ALL);

include_once("../spip_webpage.lib.php");
include_once("../spip.lib.php");
include_once("../spip_socket.lib.php");

class tests extends spip_webpage
{

  function tests ()
  {
    spip_webpage::spip_webpage ();

    $this->title = "Test System";
    $this->nav_item = "test";

  }

  function printJavaScriptHead()
  {
?>
    <script type='text/javascript'>

      function configureButton()
      {
        document.getElementById("command").value = "configure";
        document.tcs.submit();
      }

      function startButton()
      {
        document.getElementById("command").value = "start";
        document.tcs.submit();
      }

      function stopButton() {
        document.getElementById("command").value = "stop";
        document.tcs.submit();
      }
    </script>
<?php
  } 

  function printHTML ()
  {
?>
<h1>Observing Parameters</h1>

<center>

<form name="tcs" target="spip_response" method="GET">

<input type="hidden" name="command" id="command" value=""/>
<table cellpadding='3px' border=0 cellspacing=20px width='80%'>

<tr>
  <th width=33%>Observation Configuration</th>
  <th width=33%>Instrument Configuration</th>
  <th width=33%>Stream Configuration</th>
</tr>

<tr>
  <td valign=top>

    <table width='100%'>
      <tr>
        <td><b>Key</b></td>
        <td><b>Value</b></td>
      </tr>
  
      <tr> 
        <td>SOURCE</td>
        <td><input type="text" name="source" size="16" value="J0437-4715"></td>
      </tr>
<tr> <td>RA</td>           <td><input type="text" name="ra" size="16" value="04:37:00.0"></td> </tr>
<tr> <td>DEC</td>          <td><input type="text" name="dec" size="16" value="-47:15:00.0"></td> </tr>
<tr> <td>OBSERVER</td>     <td><input type="text" name="observer" size="16" value=""/></td> </tr>
<tr> <td>PROJECT ID</td>   <td><input type="text" name="project_id" size="8" value="" /></td> </tr>
<tr> <td>TOBS</td>         <td><input type="text" name="tobs" size="4" value="10"/></td> </tr>
<tr> <td>MODE</td>         <td><input type="text" name="mode" size="4" value="PSR"/></td> </tr>
<tr> <td>PROC FILE</td>    <td><input type="text" name="processing_file" size="16" value=""/></td> </tr>
<tr> <td>NBEAM</td>        <td><input type="text" name="nbeam" size="2" value="<?php echo $this->config["NUM_BEAM"]?>" readonly/></td> </tr>
<?php
    for ($i=0; $i<$this->config["NUM_BEAM"]; $i++)
    {
      echo "<tr>";
      echo   "<td>BEAM ".$this->config["BEAM_".$i]."</td>";
      echo   "<td>";
      echo     "<input type='radio' name='beam_state_".$i."' id='beam_state_".$i."' value='on'/><label for='beam_state_".$i."'>On</label>";
      echo     "&nbsp;&nbsp;";
      echo     "<input type='radio' name='beam_state_".$i."' id='beam_state_".$i."' value='off' checked/><label for='beam_state_".$i."'>Off</label>";
      echo   "</td>";
      echo "</tr>\n";
    }

?>

    </table>
  </td>

  <td valign=top>
    <table width='100%'>
      <tr>
        <td><b>Key</b></td>
        <td><b>Value</b></td>
      </tr>
<?php
      $this->printInstrumentRow("NBIT", "nbit", "NBIT", 2);
      $this->printInstrumentRow("NDIM", "ndim", "NDIM", 2);
      $this->printInstrumentRow("NPOL", "npol", "NPOL", 2);
      $this->printInstrumentRow("OSRATIO", "oversampling_ratio", "OVERSAMPLING RATIO", 8);
      $this->printInstrumentRow("TSAMP", "sampling_time", "SAMPING TIME", 8);
      $this->printInstrumentRow("CHANBW", "channel_bandwidth", "CHANNEL BW", 8);
      $this->printInstrumentRow("DSB", "dual_sideband", "DUAL SIDEBAND", 8);
      $this->printInstrumentRow("RESOLUTION", "resolution", "RESOLUTION", 8);
      $this->printInstrumentRow("INDEPENDENT_BEAMS", "independent_beams", "INDEPENDENT_BEAMS", 8);
?>
    </table>
  </td>

  <td valign=top>
    <table width='100%'>
      <tr><td><b>Stream</b></td><td><b>Host</b></td><td><b>Beam</b></td><td><b>FREQ</b></td><td><b>BW</b></td><td><b>NCHAN</b></td></tr>
<?php
      for ($i=0; $i<$this->config["NUM_STREAM"]; $i++)
      {
        list($host, $beam, $subband_id) = explode(":", $this->config["STREAM_".$i]);
        $this->printStreamRow($i, $host, $beam, $subband_id);
      }
?>
    </table>
  </td>
</tr>



<tr> 
 <td colspan=2>
  <input type='button' onClick='javascript:configureButton()' value='Configure'/>
  <input type='button' onClick='javascript:startButton()' value='Start'/>
  <input type='button' onClick='javascript:stopButton()' value='Stop'/>
 </tr>
</tr>


</table>
</form>

<iframe name="spip_response" src="" width=80% frameborder=0 height='350px'></iframe>

</center>

<?php
  }

  function printSPIPResponse($get)
  {
    $xmls = array();

    $xml = "<source_parameters>\n";
    $xml .=   "<name epoch='J2000'>".$get["source"]."</name>\n";
    $xml .=   "<ra units='hh:mm:ss'>".$get["ra"]."</ra>\n";
    $xml .=   "<dec units='hh:mm:ss'>".$get["dec"]."</dec>\n";
    $xml .= "</source_parameters>\n";

    $xml .= "<observation_parameters>\n";
    $xml .=   "<observer>".$get["observer"]."</observer>\n";
    $xml .=   "<project_id>".$get["project_id"]."</project_id>\n";
    $xml .=   "<tobs>".$get["tobs"]."</tobs>\n";
    $xml .=   "<mode>".$get["mode"]."</mode>\n";
    $xml .=   "<processing_file>".$get["processing_file"]."</processing_file>\n";
    $xml .=   "<utc_start></utc_start>\n";
    $xml .=   "<utc_stop></utc_stop>\n";
    $xml .= "</observation_parameters>\n";

    $xml .= "<instrument_parameters>\n";
    $xml .=   "<adc_sync_time>0</adc_sync_time>\n";
    $xml .= "</instrument_parameters>\n";

    $html = "";

    # If we can have independent control of the beams 
    if ($this->config["INDEPENDENT_BEAMS"] == "true")
    {
      $beam_hosts = array();
      # get the list of hosts for each beam
      for ($i=0; $i<$this->config["NUM_STREAM"]; $i++)
      {
        list($host, $beam, $subband) = explode(":", $this->config["STREAM_".$i]);
        $beam_hosts[$beam] = $host;
      }

      for ($i=0; $i<$this->config["NUM_BEAM"]; $i++)
      {
        if (strcmp($get["beam_state_".$i], "on") !== FALSE)
        {
          $tcs_beam_host = $beam_hosts[$i];
          $tcs_beam_port = $this->config["TCS_INTERFACE_PORT"] + $i;
  
          $beam_xml  = "<?xml version='1.0' encoding='ISO-8859-1'?>\n";
          $beam_xml .= "<obs_cmd>\n";
          $beam_xml .= "<command>".$get["command"]."</command>\n";
          
          $beam_xml .= $xml;
          $beam_xml .= "<beam_configuration>\n";
          $beam_xml .=   "<nbeam>1</nbeam>\n";
          
          $beam_xml .=   "<beam_state_0 name='".$this->config["BEAM_".$i]."'>".$get["beam_state_".$i]."</beam_state_0>\n";
          $beam_xml .= "</beam_configuration>\n";

          $beam_xml .= "</obs_cmd>\n";

          $tcs_socket = new spip_socket();
          if ($tcs_socket->open ($tcs_beam_host, $tcs_beam_port) == 0)
          {
            $raw_xml = str_replace("\n","", $beam_xml);
            $tcs_socket->write ($raw_xml."\r\n");
            $reply = $tcs_socket->read();
          }
          else
          {
            $html .= "<p>Could not connect to ".$tcs_beam_host.":".$tcs_beam_port."</p>\n";
          }
          $tcs_socket->close();
        }
      }
    }
    # We have only 1 TCS instance for each beam
    else
    {
      $tcs_host = $this->config["SERVER_HOST"];
      $tcs_port = $this->config["TCS_INTERFACE_PORT"];

      $beam_xml  = "<?xml version='1.0' encoding='ISO-8859-1'?>\n";
      $beam_xml .= "<obs_cmd>\n";
      $beam_xml .= "<command>".$get["command"]."</command>\n";
      $beam_xml .= $xml;

      $beam_xml .=   "<beam_configuration>\n";
      $beam_xml .=   "<nbeam>".$get["nbeam"]."</nbeam>\n";
      for ($i=0; $i<$get["nbeam"]; $i++)
        $beam_xml .=   "<beam_state_".$i." name='".$this->config["BEAM_".$i]."'>".$get["beam_state_".$i]."</beam_state_".$i.">\n";
      $beam_xml .=   "</beam_configuration>\n";
      $beam_xml .= "</obs_cmd>\n";

      $tcs_socket = new spip_socket();
      if ($tcs_socket->open ($tcs_host, $tcs_port) == 0)
      {
        $raw_xml = str_replace("\n","", $beam_xml);
        $tcs_socket->write ($raw_xml."\r\n");
        $reply = $tcs_socket->read();
      }
      else
      {
        $html .= "<p>Could not connect to ".$tcs_host.":".$tcs_port."</p>\n";
      }
      $tcs_socket->close();
    }

    echo $html;
  }

  function printInstrumentRow($key, $name, $title, $size, $maxlength="")
  {
    echo "<tr>\n";
    echo "  <td>".$title."</td>";
    echo "  <td>".$this->config[$key]."<input type='hidden' name='".$name."' id='".$name."' value='".$this->config[$key]."' size=".$size;
    if ($maxlength != "")
      echo " maxlength=".$maxlength;
    echo "/ readonly></td>\n";
    echo "</tr>\n";
  }

  function printStreamRow($stream, $host, $beam, $subband_id)
  {
    list ($freq, $bw, $nchan) = explode(":", $this->config["SUBBAND_CONFIG_".$subband_id]);
    echo "<tr>\n";
    echo "  <td>".$stream."</td>\n";
    echo "  <td>".$host."</td>\n";
    echo "  <td>".$beam."</td>\n";
    echo "  <td>".$freq."</td>\n";
    echo "  <td>".$bw."</td>\n";
    echo "  <td>".$nchan."</td>\n";
    echo "</tr>\n";
  }
}

if (isset($_GET["command"]))
{
  $obj = new tests();
  $obj->printSPIPResponse($_GET);
}
else
{
  $_GET["single"] = "true";
  handleDirect("tests");
}
