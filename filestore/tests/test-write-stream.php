<?php
    
$uri = "store-ftp://bar/hello.txt";
print "Writing to $uri (fopen + fwrite) ... ";
$fp = fopen($uri, 'wb');
fwrite($fp, 'Another greeting:' . PHP_EOL);
fwrite($fp, 'Hello World' . PHP_EOL);
fclose($fp);
print 'OK' . PHP_EOL;
print 'Download link: ' . file_create_url($uri) . PHP_EOL;

$uri = "store-ftp://bar/goodbye.txt";
print "Writing to $uri (file_put_contents) ... ";
file_put_contents($uri, 'Goodbye cruel world'.PHP_EOL);
print 'OK' . PHP_EOL;
print 'Download link: ' . file_create_url($uri) . PHP_EOL;

$uri = "store-ftp:///baz/tests";
print "Creating directory $uri (mkdir) ... ";
mkdir($uri, 0755, $recurse=true);
print 'OK' . PHP_EOL;

$uri = "store-ftp:///baz/tests/hello.txt";
print "Writing to $uri (file_put_contents) ... ";
file_put_contents($uri, 'Goodbye cruel world'.PHP_EOL);
print 'OK' . PHP_EOL;
print 'Download link: ' . file_create_url($uri) . PHP_EOL;
