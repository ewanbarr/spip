<?PHP

class spip_webpage 
{
  var $css = array("/spip/spip.css");
  var $ejs = array();
  var $title = "spip";
  var $callback_freq = 4000;
  var $doc_type = "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/html4/loose.dtd\">";

  var $nav_item = "";
  var $logo_text = "";
  var $sidebar_width = "250";

  function spip_webpage() 
  {

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

  function printUpdateHTML()
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
         "boxImage: '/images/jsprogress/percentImage_40.png', ".
         "barImage : Array( '/images/jsprogress/percentImage_back1_40.png', ".
         "'/images/jsprogress/percentImage_back2_40.png', ".
         "'/images/jsprogress/percentImage_back3_40.png', ".
        "'/images/jsprogress/percentImage_back4_40.png') } );\n";
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

    // print the main navigation panel
    if ($obj->nav_item != "")
    {
      echo "<table height='60px' width='100%' border=1>\n";
      echo "<tr>\n";
      echo "<td width='210px' height='60px'>";
      echo "<img src='/spip/images/spip_logo.png' width='200px' height='60px'>";
      echo "</td>\n";
      echo "<td width='100px'><a href='/spip/timing/'>Timing</a></td>\n";
      //echo "<td width='100px'><a href='/spip/transients/'>Transients</a></td>\n";
      echo "<td width='100px'><a href='/spip/status/'>Status</a></td>\n";
      echo "<td width='100px'><a href='/spip/controls/'>Controls</a></td>\n";
      echo "<td width='100px'><a href='/spip/test/'>Test</a></td>\n";
      echo "</tr>\n";
      echo "</table>\n";
    }

    if (method_exists($obj, "printSideBarHTML"))
    {
      echo "<table width='100%' cellpadding='10px' border=0>\n";
      echo "  <tr>\n";
      echo "    <td style='vertical-align: top; width: ".$obj->sidebar_width."px'>\n";
      $obj->printSideBarHTML();
      echo "    </td>\n";
      echo "    <td style='vertical-align: top;'>\n";
      $obj->printHTML();
      echo "    </td>\n";
      echo "  </tr>\n";
      echo "</table>\n";
    }
    else
    {
      $obj->printHTML();
    }

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

