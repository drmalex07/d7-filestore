<?php
    
$uri = "store-ftp://foo";

$h = opendir($uri);
var_dump($h);

while (($e = readdir($h)) !== false) {
    print "> $e" . PHP_EOL;
}

closedir($h);


