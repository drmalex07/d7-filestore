<?php

function filestore_tests_test_reading()
{
    $module_path = drupal_get_path('module','filestore');
    
    drupal_add_css ($module_path . '/css/tests.css', 'file');
    
    if (isset($_GET['path'])) {
        $path = $_GET['path'];
        $uri = FILESTORE_FTP_SCHEME . "://${path}";    
    } else {
        $uri = FILESTORE_FTP_SCHEME . "://foo/hello.txt";  
    }
    
    $web_url = file_create_url($uri);

    $ra0 = array(
        'd-0' => array(
            '#type' => 'container',
            '#attributes' => array('class' => array('span-70p')),
            'd-0-uri' => array(
                '#type' => 'markup',
                '#markup' => '<dt>URI</dt><dd><tt>' . check_plain($uri). '</tt></dd>'
            ),            
            'd-0-size' => array(
                '#type' => 'markup',
                '#markup' => '<dt>Size</dt><dd><tt>' . filesize($uri). '</tt></dd>'
            ),
            'd-0-web-url' => array(
                '#type' => 'markup',
                '#markup' => '<dt>Web URL</dt><dd><tt>' . l($web_url, $web_url) . '</tt></dd>'
            ),

        ),
    );
 
    // Test 1 - fopen in read mode

    $fp = fopen($uri, 'rb');
    $content = '';
    if ($fp) {
        while (!feof($fp)) {
            $content .= fread($fp, 16);

        }
        fclose($fp);
    }

    $ra1 = array(
        'd-1' => array(
            '#type' => 'container',
            '#attributes' => array('class' => array('span-70p')),
            'h-1' => array(
                '#type' => 'markup',
                '#markup' => "<h4>Testing <tt>fopen</tt> mode=<tt>rb</tt></h4>",
            ),
            'pre-1' => array(
                '#type' => 'markup',
                '#markup' => "<pre>" . filter_xss($content) . "</pre>", 
            ),
        ),
    );

    // Test 2 - file_get_contents

    $content = file_get_contents($uri);
    
    $ra2 = array(
        'd-2' => array(
            '#type' => 'container',
            '#attributes' => array('class' => array('span-70p')),
            'h-2' => array(
                '#type' => 'markup',
                '#markup' => "<h4>Testing <tt>file_get_contents</tt></h4>",
            ),
            'pre-2' => array(
                '#type' => 'markup',
                '#markup' => "<pre>" . filter_xss($content) . "</pre>", 
            ),
        ),
    );

    return array_merge($ra0, $ra1, $ra2);
}


function filestore_tests_test_writing()
{
    // Todo
    return 'Todo';
}
