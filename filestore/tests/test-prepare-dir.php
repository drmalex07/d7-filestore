<?php

$uri = "store-ftp://bar/attachments/hello.txt";
$dir = drupal_dirname($uri);

print "dir: ${dir}" . PHP_EOL;
print "uri: ${uri}" . PHP_EOL;

$r = file_prepare_directory($dir, FILE_CREATE_DIRECTORY);
var_dump(array('file_prepare_directory' => $r));

var_dump(array('is_dir' => is_dir($dir)));
var_dump(array('is_readable' => is_readable($dir)));
var_dump(array('is_writable' => is_writable($dir)));

