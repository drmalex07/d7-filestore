<?php
    
$uri = "store-ftp://foo/hello.txt";
print "Stat (stat) for $uri:" . PHP_EOL;
print_r(stat($uri));

$n = filesize($uri);
print "Reading $n bytes from $uri (fopen + fread) ... ";
$fp = fopen($uri, 'rb');
$stat = fstat($fp);
$content = '';
if ($fp) {
   while (!feof($fp)) {
       $content .= fread($fp, 16);
   }
   fclose($fp);
}
print 'OK' . PHP_EOL;
print "Stat (fstat) for fopen()ed $uri:" .PHP_EOL;
print_r($stat);
print 'Contents: --' . PHP_EOL;
print $content;
print '--' . PHP_EOL;
print 'Download link: ' . file_create_url($uri) . PHP_EOL;


print "Writing to $uri (file_get_contents) ... ";
$content = file_get_contents($uri);
print 'OK' . PHP_EOL;
print 'Contents: --' . PHP_EOL;
print $content;
print '--' . PHP_EOL;
print 'Download link: ' . file_create_url($uri) . PHP_EOL;

