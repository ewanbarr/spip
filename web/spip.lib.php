<?PHP

// include site specific configuration
include_once ("site.inc.php");

if (!defined("INSTRUMENT"))
{
  echo "<p>ERROR: INSTRUMENT must be defined in site.inc.php</p>\n";
  exit;
}

if (!defined("CONFIG_DIR"))
{
  echo "<p>ERROR: CONFIG_DIR must be defined in site.inc.php</p>\n";
  exit;
}

if (!defined("XML_DEFINITION"))
{
  echo "<p>ERROR: XML_DEFINITION must be defined in site.inc.php</p>\n";
  exit;
}

class spip {

  public function spip ()
  {

  }

  public static function get_config ()
  {
    $config_file = CONFIG_DIR."/spip.cfg";
    return spip::get_config_file ($config_file);
  }

  public static function get_config_file ($config_file)
  {
    $fptr = @fopen($config_file,"r");
    $returnArray = array();

    if (!$fptr) 
    {
      echo "ERROR: Could not open file: $config_file for reading<BR>\n";
    }
    else 
    {
      while ($line = fgets($fptr, 8192))
      {
        $comment_pos = strpos($line,"#");
        if ($comment_pos!==FALSE)
        {
          $line = substr($line, 0, $comment_pos);
        }

        // Remove trailing whitespace
        $line = chop($line);

        // skip blank lines
        if (strlen($line) > 0) {
          $array = preg_split('/\s+/', $line, 2);   // Split into keyword/value
          if (count($array) == 2)
            $returnArray[$array[0]] = $array[1];
        }
      }
    }
    return $returnArray;
  }
}
?>
