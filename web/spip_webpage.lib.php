<?PHP

include_once ("spip.lib.php");

class spip_webpage 
{
  var $css = array("/spip/spip.css");
  var $ejs = array();
  var $title = "spip";
  var $callback_freq = 4000;
  var $doc_type = "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\"\n \"http://www.w3.org/TR/html4/loose.dtd\">";

  var $nav_item = "";
  var $logo_text = "";
  var $sidebar_width = "250";

  function spip_webpage () 
  {
    $this->config = spip::get_config();
  }

  function javaScriptCallBack()
  {

  }

  function printJavaScriptHead() 
  {

  }

  function printJavaScriptBody()
  {

  }

  function printHTML()
  {

  }

  function printUpdateHTML($get)
  {

  }

  function printJavaScriptTail()
  {

  }

  function openBlockHeader($block_title)
  {
    echo "<table class='wrapper'>\n";
    echo "  <tr><th class='wrapper'>".$block_title."</th></tr>\n";
    echo "  <tr><td class='wrapper'>\n";
  }

  function closeBlockHeader()
  {
    echo "  </td></tr>\n";
    echo "</table>\n";
  }

  static function renderProgressBarObject($id)
  {
    $id = str_replace("-","",$id);
    echo $id." = new JS_BRAMUS.jsProgressBar($('".$id."_progress_bar'), 0, ";
    echo " { width : 40, showText : false, animate : false, ".
         "boxImage: '/spip/images/jsprogress/percentImage_40.png', ".
         "barImage : Array( '/spip/images/jsprogress/percentImage_back1_40.png', ".
         "'/spip/images/jsprogress/percentImage_back2_40.png', ".
         "'/spip/images/jsprogress/percentImage_back3_40.png', ".
        "'/spip/images/jsprogress/percentImage_back4_40.png') } );\n";
  }

  static function renderProgressBar($id)
  {
    $id = str_replace("-","",$id);
    echo "       <span id='".$id."_progress_bar'>[ ... ]</span>\n";
  }


} // END CLASS DEFINITION


#
# handle a direct inclusion of the specified class
#
function handleDirect($child_class) 
{
  // if this parameter is defined, output the HTML for the
  // specified pages
  if (array_key_exists("single", $_GET) && ($_GET["single"] == "true"))
  {
    $obj = new $child_class();

    echo $obj->doc_type."\n";
    echo "<html>\n";
    echo "  <head>\n";
    echo "    <title>".$obj->title."</title>\n";
    echo "    <link rel='shortcut icon' href='/spip/images/spip_favicon.ico'/>\n";

    // css and javascript includes
    for ($i=0; $i<count($obj->css); $i++)
      echo "    <link rel='stylesheet' type='text/css' href='".$obj->css[$i]."'>\n";

    for ($i=0; $i<count($obj->ejs); $i++)
      echo "    <script type='text/javascript' src='".$obj->ejs[$i]."'></script>\n";

    // callbacks for javascript pollings
    if ($obj->javaScriptCallback() != "")
    {
      echo "    <script type='text/javascript'>\n";
      echo "      function poll_server()\n";
      echo "      {\n";
      echo "        ".$obj->javaScriptCallback()."\n";
      echo "        setTimeout('poll_server()', ".$obj->callback_freq.");\n";
      echo "      }\n";
      echo "    </script>\n";
    }

    // javascript head scripts
    if ($obj->printJavaScriptHead() != "")  
      $obj->printJavaScriptHead();

    echo "  </head>\n";
  
    // if we need to run the callback
    if ($obj->javaScriptCallback() != "")
      echo "<body onload='poll_server()'>\n";
    else
      echo "<body>\n";

    $obj->printJavaScriptBody();

    echo "<div id='topbg'></div>\n";

    echo "<div id='main'>\n";

    echo "  <div id='header'>\n";
    echo "    <h1>SPIP</h1>\n";
    echo "    <h2 class='sub'>".$obj->config["INSTRUMENT"]."</h2>\n";
    echo "    <div id='hdr-overlay'></div>\n";
    echo "  </div>\n";

    // print the main navigation panel
    if ($obj->nav_item != "")
    {
      $nav_items = array ("/spip/timing/" => "Timing", 
                          "/spip/stats/" => "Stats",
                          "/spip/status/" => "Status",
                          "/spip/results/" => "Results",
                          "/spip/controls/" => "Controls",
                          "/spip/logs/" => "Logs");

      #echo "<table height='60px' width='100%' id='nav'>\n";
      #echo "<tr>\n";
      #echo "<td width='200px' height='80px' style='text-align: left; padding-left: 10px;'>";
      #echo "<img src='/spip/images/spip_logo.png' width='200px' height='60px'>";
      #echo "</td>\n";

      echo "  <ul id='menu'>\n";
      foreach ($nav_items as $key => $val )
      {
        if (strpos($key, $obj->nav_item) !== FALSE)
        {
          echo "    <li><a class='sel' href='".$key."'><span></span>".$val."</a></li>\n";
        }
        else
        {
          echo "    <li><a href='".$key."'><span></span>".$val."</a></li>\n";
        }
      }
      echo "  </ul>\n";

      #echo "</tr>\n";
      #echo "</table>\n";
    }

    echo "  <div id='content'>\n";

    if (method_exists($obj, "printSideBarHTML"))
    {
      echo "    <div id='left'>\n";
      $obj->printSideBarHTML();
      echo "    </div>\n";

      echo "    <div id='right'>\n";
      $obj->printHTML();
      echo "    </div>\n";
    }
    else
    {
      echo "    <div id='centre'>\n";
      $obj->printHTML();
      echo "    </div>\n";
    }
  
    echo "  </div>\n";  // content

    echo "  <div class='cleaner'></div>\n";

    echo "  <div id='footer'>\n";
    echo "Copyright © Swinburne University of Technology | "; 
    echo "<a href='/spip/test/'>Testing Interface</a>\n";
    echo "  </div>\n";

    echo "</div>\n";  // main

    $obj->printJavaScriptTail();

    echo "</body>\n";
    echo "</html>\n";

  } else if (array_key_exists("update", $_GET) && ($_GET["update"] == "true")) {

    $obj = new $child_class();
    $obj->printUpdateHTML($_GET);

  } else  if (array_key_exists("action", $_GET) && ($_GET["action"] != "")) {
    
    $obj = new $child_class();
    $obj->printActionHTML($_GET);

  } else {
    # do nothing :)
  }
}

