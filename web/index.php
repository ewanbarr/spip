<?PHP

error_reporting(E_ALL);
ini_set("display_errors", 1);

include_once("spip_webpage.lib.php");

class home extends spip_webpage
{
  function home()
  {
    spip_webpage::spip_webpage();

    $this->title = "SPIP";
    $this->nav_item = "main";
  }
}

$_GET["single"] = true;
handleDirect("home");
