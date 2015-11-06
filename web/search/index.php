<?PHP

ini_set("display_errors", "on");
error_reporting(E_ALL);

include_once("../spip_webpage.lib.php");
include_once("../spip.lib.php");
include_once("../spip_socket.lib.php");

class search extends spip_webpage
{

  function search ()
  {
    spip_webpage::spip_webpage ();

    $this->title = "Search";
    $this->nav_item = "search";

    $this->config = spip::get_config();
  }

  function printHTML ()
  {
?>
<h1>Search</h1>

<p>Not implemented</p>

<?php
  }
}
if (!isset($_GET["update"]))
  $_GET["single"] = "true";
handleDirect("search");
