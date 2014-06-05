<?php

require_once('Net/FTP.php');

// Todo: A great part of the following stuff must be moved in a base class
// in order to be re-used by other stream wrappers.

class FTP_Stream implements DrupalStreamWrapperInterface 
{
    const SCHEME = FILESTORE_FTP_SCHEME;

    const STAT_S_IFDIR = 0040000; // S_IFDIR
    const STAT_S_IFREG = 0100000; // S_IFREG

    public $context = null;

    protected $config = array();
    
    protected $uri = null;

    protected $open_mode = null;

    protected $local_path = null;

    protected $local_handle = null;

    protected $directory_cursor = null;

    protected $directory_entries = null;

    protected $directory_path = null;

    private $store = null;

    protected static function _stat_mode_for_directory($perms = 0777)
    {
        if (is_string($perms)) {
            $perms = self::_permissions_from_string($perms);
        }
        return (self::STAT_S_IFDIR | $perms);
    }
    
    protected static function _stat_mode_for_file($perms = 0644)
    {
        if (is_string($perms)) {
            $perms = self::_permissions_from_string($perms);
        }
        return (self::STAT_S_IFREG | $perms);
    }
   
    protected static function _permissions_from_string($perms)
    {
        // Expect input as "rwxr-xr--"
        
        $flags = array(
            'r' => 04,
            'w' => 02,
            'x' => 01,
            '-' => 00,
        );
        
        $p = 0;
        for ($i=0; $i<3; $i++) {
            $p = $p << 3;
            $t = substr($perms, $i * 3, 3);
            $n = 0;
            for ($k=0; $k<3; $k++) {
                $n |= $flags[$t[$k]];
            }
            $p += $n;
        }

        return $p;
    }
    
    public static function canonicalize_path($path)
    {
        $path_components = explode('/', $path);
        $stack = array();
        foreach ($path_components as $component) {
            if ($component == '..') {
                // ignore this component, pop from stack
                array_pop($stack);
                continue;
            }
            if ($component === '.') {
                // ignore this component
                continue;
            }
            array_push($stack, $component);
        }
        return implode('/', $stack);
    }
    
    public static function temp_file($prefix = 'tmp')
    { 
        // Create an new empty temporary file, return its path
        $tmp_dir = sys_get_temp_dir();
        $file = tempnam($tmp_dir, $prefix);
        return $file;
    }
    
    public static function get_path($uri) 
    {
        $prefix = self::SCHEME . '://';
        assert(strpos($uri, $prefix) === 0);
        $path = substr($uri, strlen($prefix));
        $path = trim($path, '/');
        $path = self::canonicalize_path($path);
        return $path;
    }

    protected function _debug($msg, $vars = null)
    {
        watchdog('filestore', "FTP_Stream: ${msg}", $vars, WATCHDOG_DEBUG);
    }
    
    protected function _info($msg, $vars = null)
    {
        watchdog('filestore', "FTP_Stream: ${msg}", $vars, WATCHDOG_INFO);
    }

    protected function _error($msg, $vars = null)
    {
        watchdog('filestore', "FTP_Stream: ${msg}", $vars, WATCHDOG_ERROR);
    }

    public static function update_config(&$config)
    {
        // Override existing configuration, save in backend

        $config += (array) variable_get('filestore.ftp_stream.config');
        
        variable_set('filestore.ftp_stream.config', $config);

        return $config;
    }

    public static function get_config()
    {
        // Read user-supplied settings
        $config = (array) variable_get('filestore.ftp_stream.config');
        
        // Add defaults (fallback) settings
        $config += array(
            'host' => 'localhost',
            'username' => 'anonymous',
            'password' => '',
            'timeout' => 5,
            'use_site_commands' => true, /* try to support CHMOD */
            'use_passive' => true,       /* prefer PASSIVE mode for FTP */
        );

        return $config;
    }
    
    /**
     * Due to PHP bug #40459, the constructor of this class isn't always called
     * for some of the methods. This private method calls the constructor if
     * it hasn't been called before.
     *
     * @see https://bugs.php.net/bug.php?id=40459
     */
    protected function _force_constructed($uri = null)
    {
        if (is_null($this->store)) {
            $this->__construct();
        }
        $this->uri = $uri;
    }

    protected function _close_connection()
    {
        if ($this->store != null) {
            $this->store->disconnect();
        }
        $this->store = null;
    }
    
    protected function _open_connection(&$config)
    { 
        $this->store = new Net_FTP();
                
        $res = $this->store->connect($config['host']);
        if ($res instanceof Pear_Error) {
            throw new Exception($res->getMessage());
        }

        $res = $this->store->login($config['username'], $config['password']);
        if ($res instanceof Pear_Error) {
            throw new Exception($res->getMessage());
        }

        return true;
    }
    
    protected function _setup_connection(&$config)
    {
        $this->store->setTimeout((int) $config['timeout']);
        
        if ($config['use_passive']) {
            $this->store->setPassive();
        }

        return true;
    }
  
    protected function _close_local_handle()
    {
        $return = true;
        if ($this->local_handle != null) {
            $return = fclose($this->local_handle);
        }
        $this->local_handle = null;
        return $return;
    }
    
    protected function _open_local_handle($local_path, $mode)
    {
        $this->local_handle = fopen($local_path, $mode);
        if (!$this->local_handle) {
            throw new Exception("Cannot local file at ${local_path}");
        }
    }

    public function __construct() 
    { 
        $this->config = self::get_config();
        
        $this->_open_connection($this->config);

        $this->_setup_connection($this->config);
               
        //$this->_debug('Created an FTP_Stream object');
        
        return;
    }

    public function __destruct()
    {
        $this->_close_local_handle();
        $this->_close_connection();
    }

    function setUri($uri) 
    {
        $this->uri = $uri;
    }

    function getUri() 
    {
        return $this->uri;
    }

    /**
     * Implements getMimeType().
     * Copies most of it's logic from DrupalPublicStreamWrapper.
     */
    static function getMimeType($uri, $mapping = NULL) 
    {
        if (!isset($mapping)) {
            // Load the full map
            include_once DRUPAL_ROOT . '/includes/file.mimetypes.inc';
            $mapping = file_mimetype_mapping();
        }

        $extension = '';
        $file_parts = explode('.', basename($uri));

        // Remove the first part: a full filename should not match an extension.
        array_shift($file_parts);

        // Iterate over the file parts, trying to find a match.
        while ($additional_part = array_pop($file_parts)) {
            $extension = drupal_strtolower($additional_part . ($extension ? '.' . $extension : ''));
            if (isset($mapping['extensions'][$extension])) {
                return $mapping['mimetypes'][$mapping['extensions'][$extension]];
            }
        }

        return 'application/octet-stream';
    }

    /**
     * Overrides getExternalUrl()
     * 
     * Map resource into our public (i.e. external) URI namespace
     */
    function getExternalUrl() 
    {
        $path = self::get_path($this->uri);
        $scheme = self::SCHEME;
        $url = url("public/files/${scheme}/f/${path}", array('absolute' => TRUE));
        return $url;
    }

    /**
     * Implements chmod()
     */
    function chmod($mode) 
    {
        assert (!is_null($this->uri));
        
        $supported = (bool) $this->config['use_site_commands'];
        if (!$supported) {
            // Do not report an error, return success
            $this->_debug('Skipping CHMOD operation (not supported by backend)');
            return true;
        }
        
        // It seems, the FTP backend supports SITE commands ...
        $return = null;
        
        $path = self::get_path($this->uri);
        // Net_FTP expects its mode argument as an octal string, instead of an int (??)
        $mode = sprintf('%o', $mode);
        $res = $this->store->chmod($path, $mode);
        if ($res instanceof Pear_Error) {
            $this->_error('Failed to change mode of @rfile to !mode: @msg', array(
                '@rfile' => $path, '!mode' => $mode, '@msg' => $res->getMessage(),
            ));
            $return = false;
        } else {
            $this->_info('Changed mode of @rfile to !mode', array(
                '@rfile' => $path, '!mode' => $mode,
            ));
            $return = true; 
        }

        return $return;
    }

    /**
     * Implements realpath().
     */
    function realpath() 
    {
        assert (!is_null($this->uri));

        return self::SCHEME . '://' . self::get_path($this->uri);
    }
    
    /**
     * Opens a stream, as for fopen(), file_get_contents(), file_put_contents()
     *
     * @param $uri
     *   A string containing the URI to the file to open.
     * @param $mode
     *   The file mode ("r", "wb" etc.).
     * @param $options
     *   A bit mask of STREAM_USE_PATH and STREAM_REPORT_ERRORS.
     * @param &$opened_path
     *   A string containing the path actually opened.
     *
     * @return
     *   Returns TRUE if file was opened successfully. (Always returns TRUE).
     *
     * @see http://php.net/manual/en/streamwrapper.stream-open.php
     */
    public function stream_open($uri, $mode, $options, &$opened_path) 
    {
        $return = true;

        $this->uri = $uri;
        $this->_debug('Opening !uri at mode !mode', array('!uri' => $uri, '!mode' => $mode)); 

        /* Clear the "b" flag, we will be using FTP_BINARY anyway */
        $mode = str_replace('b', '', $mode);
       
        $this->open_mode = null;
        $this->_close_local_handle();
 
        switch ($mode) {
        case 'a':
        case 'a+':
            {
                // Append mode is not supported
                throw new Exception("Cannot open stream in append '${mode}' mode (not supported)");
            }
            break;
        case 'r+':
        case 'w+':
            {
                // Mixed read/write mode is not supported
                throw new Exception("Cannot open stream in mixed read/write '${mode}' mode (not supported)");
            }
            break;
        case 'x':
        case 'x+':
            {
                // Exclusive mode is not supported
                throw new Exception("Cannot open stream in exclusive '$mode' mode (not supported)");
            }
            break;
        case 'c+':
        case 'c':
            {
                // Locking is not supported anyway
                throw new Exception("Cannot open stream in create '$mode' mode (locking is not supported)");
            }
            break;
        case 'r':
            {
                $lfile = self::temp_file();
                $rfile = self::get_path($this->uri);
                $this->_debug("Downloading file ${rfile} into ${lfile}");
                $res = $this->store->get($rfile, $lfile, $overwrite = true, FTP_BINARY);
                if ($res instanceof Pear_Error) {
                    $this->_error('Failed to download @rfile: @msg', array(
                        '@rfile' => $rfile, '@msg' => $res->getMessage()
                    ));
                    $return = false;
                    $opened_path = null;
                } else {
                    $this->_debug("Downloaded @rfile (!n bytes) into @lfile", array(
                        '@rfile' => $rfile, '@lfile' => $lfile, '!n' => filesize($lfile),
                    ));
                    $this->open_mode = 'r';
                    $this->local_path = $lfile;
                    $this->_open_local_handle($lfile, 'rb');
                    $return = true;
                    $opened_path = $rfile; // XXX Is this ok?
                }
            }
            break;
        case 'w':
            {
                $lfile = self::temp_file();
                $rfile = self::get_path($this->uri);
                $this->_debug("Created temporary file ${lfile} for writing to ${rfile}");
                $this->open_mode = 'w';
                $this->local_path = $lfile;
                $this->_open_local_handle($lfile, 'wb');
                $return = true;
                $opened_path = $rfile; // XXX Is this ok?
            }
            break;
        default:
            {
                throw new Exception("Cannot open stream in '$mode' mode (unknown mode)");
            }
            break;
        }

        return $return;
    }

    /**
     * Support for flock().
     *
     *
     * @param $operation
     *   One of the following:
     *   - LOCK_SH to acquire a shared lock (reader).
     *   - LOCK_EX to acquire an exclusive lock (writer).
     *   - LOCK_UN to release a lock (shared or exclusive).
     *   - LOCK_NB if you don't want flock() to block while locking (not
     *     supported on Windows).
     *
     * @return
     *   Always returns false at the present time. (no support)
     *
     * @see http://php.net/manual/en/streamwrapper.stream-lock.php
     */
    public function stream_lock($operation) 
    {
        throw new BadMethodCallException('Method not supported');
        return false;
    }

    /**
     * Support for fread(), file_get_contents() etc.
     *
     * @param $count
     *   Maximum number of bytes to be read.
     *
     * @return
     *   The string that was read, or FALSE in case of an error.
     *
     * @see http://php.net/manual/en/streamwrapper.stream-read.php
     */
    public function stream_read($count) 
    {
        return fread($this->local_handle, $count);
    }

    /**
     * Support for fwrite(), file_put_contents() etc.
     *
     * @param $data
     *   The string to be written.
     *
     * @return
     *   The number of bytes written (integer).
     *
     * @see http://php.net/manual/en/streamwrapper.stream-write.php
     */
    public function stream_write($data) 
    {
        return fwrite($this->local_handle, $data);
    }

    /**
     * Support for feof().
     *
     * @return
     *   TRUE if end-of-file has been reached.
     *
     * @see http://php.net/manual/en/streamwrapper.stream-eof.php
     */
    public function stream_eof() 
    {
        return feof($this->local_handle);
    }

    /**
     * Support for fseek().
     *
     * @param $offset
     *   The byte offset to got to.
     * @param $whence
     *   SEEK_SET, SEEK_CUR, or SEEK_END.
     *
     * @return
     *   Return 0 on success, -1 on failure
     *
     * @see http://php.net/manual/en/streamwrapper.stream-seek.php
     */
    public function stream_seek($offset, $whence) 
    {
        return fseek($this->local_handle, $offset, $whence);
    }

    /**
     * Support for fflush().
     *
     * @return
     *   TRUE if data was successfully stored (or there was no data to store).
     *
     * @see http://php.net/manual/en/streamwrapper.stream-flush.php
     */
    public function stream_flush() 
    {
        // @note: data can be flushed only once (at fclose()) 
        // noop
        return true;
    }

    /**
     * Support for ftell().
     *
     * @return
     *   The current offset in bytes from the beginning of file.
     *
     * @see http://php.net/manual/en/streamwrapper.stream-tell.php
     */
    public function stream_tell() 
    {
        return ftell($this->local_handle);
    }

    /**
     * Support for fstat().
     *
     * @return
     *   An array with file status, or FALSE in case of an error - see fstat()
     *   for a description of this array.
     *
     * @see http://php.net/manual/en/streamwrapper.stream-stat.php
     */
    public function stream_stat() 
    { 
        // Set timestamps to a not-a-time value
        $return = array(
            'mtime' => -1,
            'ctime' => -1,
            'atime' => -1,
        );

        // Use an fstat() on the local copy ... 
        $return += fstat($this->local_handle);
        
        return $return;
    }

    /**
     * Support for fclose().
     *
     * @return
     *   TRUE if stream was successfully closed.
     *
     * @see http://php.net/manual/en/streamwrapper.stream-close.php
     */
    public function stream_close() 
    {
        $return = false;
        switch($this->open_mode) {
        case 'r':
            {
                // Cleanup local download
                $return = $this->_close_local_handle();
                if (!$return) {
                    $this->_error("Failed to close local handle (temporary download) to @lfile", array(
                        '@lfile' => $this->local_path
                    ));
                }
                if (!unlink($this->local_path)) {
                    $this->_error("Failed to unlink @lfile", array('@lfile' => $this->local_path));
                }
                $this->local_path = null;
            }
            break;
        case 'w':            
            {
                // Upload local staging data to remote target
                $return = $this->_close_local_handle();
                if (!$return) {
                    $this->_error("Failed to close local handle (staging upload) to @lfile", array(
                        '@lfile' => $this->local_path
                    ));
                } else {
                    // The staging upload is now saved on local storage
                    $lfile = $this->local_path;
                    $rfile = self::get_path($this->uri);
                    $res = $this->store->put($lfile, $rfile, $overwrite=false, FTP_BINARY);
                    if ($res instanceof Pear_Error) {
                        $return = false;
                        $this->_error('Failed to upload @lfile: @msg', array(
                            '@lfile' => $lfile, '@msg' => $res->getMessage()
                        ));
                    } else {
                        $return = true;
                        $this->_info('Uploaded @lfile (!n bytes) to @rfile', array(
                            '@lfile' => $lfile, '@rfile' => $rfile, '!n' => filesize($lfile),
                        ));
                        if (!unlink($lfile)) {
                            $this->_error("Failed to unlink @lfile", array('@lfile' => $lfile));
                        }
                        // Try to setup default permissions, do not report any errors.
                        // @see FTP_Stream::chmod() and FTP_Stream::mkdir().
                        $this->store->chmod($rfile, '0644');
                    }
                }
                $this->local_path = null;
            }
            break;
        default:
            break;
        } 
        
        
        return $return;
    }

    /**
     * Support for unlink().
     *
     * @param $uri
     *   A string containing the uri to the resource to delete.
     *
     * @return
     *   TRUE if resource was successfully deleted.
     *
     * @see http://php.net/manual/en/streamwrapper.unlink.php
     */
    public function unlink($uri) 
    {
        $this->_force_constructed($uri);
        $return = null;

        $path = self::get_path($uri);
        
        $res = $this->store->rm($path);
        if ($res instanceof Pear_Error) { 
            $this->_error('Failed to remove @rfile: @msg', array(
                '@rfile' => $path, '@msg' => $res->getMessage(),
            ));
            $return = false;
        } else { 
            $this->_info('Removed @rfile', array('@rfile' => $path));
            $return = true;
        }

        return $return;
    }

    /**
     * Support for rename().
     *
     * @param $src_uri,
     *   The uri to the file to rename.
     * @param $dst_uri
     *   The new uri for file.
     *
     * @return
     *   TRUE if file was successfully renamed.
     *
     * @see http://php.net/manual/en/streamwrapper.rename.php
     */
    public function rename($src_uri, $dst_uri) 
    {
        $this->_force_constructed();
        $return = null;

        $src_path = self::get_path($src_uri);  
        $dst_path = self::get_path($dst_uri);
        
        $res = $this->store->rename($src_path, $dst_path);
        if ($res instanceof Pear_Error) {
            $this->_error('Failed to rename from @src to @dst: @msg', array(
                '@src' => $src_path, '@dst' => $dst_path, '@msg' => $res->getMessage(),
            ));
            $return = false;
        } else {
            $this->_info('Renamed @src to @dst', array(
                '@src' => $src_path, '@dst' => $dst_path,
            ));
            $return = true;
        }

        return $return;
    }

    /**
     * Gets the name of the directory from a given path.
     *
     * @param $uri
     *   A URI.
     *
     * @return
     *   A string containing the directory name.
     */
    public function dirname($uri = null) 
    {
        $this->_force_constructed($uri);
        
        if ($uri == null) {
            $uri = $this->uri;
        }

        $target = self::get_path($uri);
        
        $this->_debug('Trying to compute parent of !uri (target=!target)', array(
            '!uri' => $uri, '!target' => $target));

        if (strpos($target, '/') !== false) {
            $dirname = preg_replace('@/[^/]*$@', '', $target);
        } else {
            $dirname = '';
        }
        
        return self::SCHEME . '://' . $dirname;
    }

    /**
     * Support for mkdir().
     *
     * @param $uri
     *   A string containing the URI to the directory to create.
     * @param $mode
     *   Permission flags - see mkdir().
     * @param $options
     *   A bit mask of STREAM_REPORT_ERRORS and STREAM_MKDIR_RECURSIVE.
     *
     * @return
     *   TRUE if directory was successfully created.
     *
     * @see http://php.net/manual/en/streamwrapper.mkdir.php
     */
    public function mkdir($uri, $mode, $options) 
    { 
        $this->_force_constructed($uri);
        $return = null;
       
        $path = self::get_path($uri);
        $recursive = ($options & STREAM_MKDIR_RECURSIVE);

        $res = $this->store->mkdir($path, $recursive);
        if ($res instanceof Pear_Error) {
            $this->_error('Failed to create directory at @dst: @msg', array(
                '@dst' => $path, '@msg' => $res->getMessage(),
            ));
            $return = false;
        } else {
            $this->_info('Created directory @dst with default mode', array(
                '@dst' => $path,
            ));
            $return = true;
            // Try to set permissions, but do not fail or report errors:
            // this is a non-standard command (SITE command) not always supported.
            $mode = sprintf("%o", $mode); /* Net_FTP expects an octal string */
            $this->store->chmod($path, $mode);
        }

        return $return;
    }

    /**
     * Support for rmdir().
     *
     * @param $uri
     *   A string containing the URI to the directory to delete.
     * @param $options
     *   A bit mask of STREAM_REPORT_ERRORS.
     *
     * @return
     *   TRUE if directory was successfully removed.
     *
     * @see http://php.net/manual/en/streamwrapper.rmdir.php
     */
    public function rmdir($uri, $options) 
    {
        $this->_force_constructed($uri);
        $return = null;
        
        $path = self::get_path($uri);

        // Todo
        throw new BadMethodCallException('Method not implemented');
        
        return $return;
    }

    /**
     * Support for stat().
     *
     * @param $uri
     *   A string containing the URI to get information about.
     * @param $flags
     *   A bit mask of STREAM_URL_STAT_LINK and STREAM_URL_STAT_QUIET.
     *
     * @return
     *   An array with file status, or FALSE in case of an error - see fstat()
     *   for a description of this array.
     *
     * @see http://php.net/manual/en/streamwrapper.url-stat.php
     */
    public function url_stat($uri, $flags) 
    {
        $this->_force_constructed($uri);
 
        $path = self::get_path($uri);
        $name = drupal_basename($path);

        $res = $this->store->ls($path); 
        if ($res instanceof Pear_Error) {
            $this->_error('Cannot list entries at @path', array('@path' => $path));
            return false;
        }
        if (!is_array($res)) {
            // Unknown error
            return false;
        }    
        
        //var_dump($res);
        
        $return = false;
        $count = count($res);
        
        // Decide if a file or a directory, calculate required stat fields.
        $is_file = (($count == 1) and (!($res[0]['is_dir'])) and ($res[0]['name'] == $name));
        if ($is_file) {
            // Net_FTP::ls() returned the file _at_ path
            $return = array(
                'mode' => self::_stat_mode_for_file($res[0]['rights']), 
                'size' => intval($res[0]['size']), 
                'atime' => intval($res[0]['stamp']), /* not accurate */ 
                'mtime' => intval($res[0]['stamp']), 
                'ctime' => -1,
            );
        } elseif ($count > 0) {
            // Net_FTP::ls() returned the entries _inside_ path 
            $return = array(
                'mode' => self::_stat_mode_for_directory(0777), 
                'size' => 0, 
            );
        } else {
            // Net_FTP::ls() returned an empty array: we cannot distinguish between
            // an empty directory or an non-existing entry (directory or file).
            // Try a LIST command on the path's parent ... 
            $parent_path = drupal_dirname($path);
            $this->_debug('Listing parent (@parent) of @path to decide if path exists', array(
                '@path' => $path, '@parent' => $parent_path,
            ));
            $res = $this->store->ls($parent_path, NET_FTP_DIRS_ONLY);
            if ($res instanceof Pear_Error) {
                $this->_error("Failed to list @path", array('@path' => $parent_path));
            } else {
                // Search for the basename inside parent's listing
                $entries = array_map(function ($e) { return $e['name']; }, $res);
                //var_dump(array('parent-entries' => $entries));
                if (in_array($name, $entries)) {
                    $return = array(
                        'mode' => self::_stat_mode_for_directory(0777), 
                        'size' => 0, 
                    ); 
                }
            }
        }
        
        return $return;
    }

    /**
     * Support for opendir().
     *
     * @param $uri
     *   A string containing the URI to the directory to open.
     * @param $options
     *   Unknown (parameter is not documented in PHP Manual).
     *
     * @return
     *   TRUE on success.
     *
     * @see http://php.net/manual/en/streamwrapper.dir-opendir.php
     */
    public function dir_opendir($uri, $options) 
    {
        $this->_force_constructed($uri);
        $return = true;

        assert (is_null($this->directory_cursor));
        
        $path = self::get_path($uri);
        
        $res = $this->store->ls($path);
        if ($res instanceof Pear_Error) {
            $this->_error('Cannot list @rfile', array('@rfile' => $path));
            $return = false;
        } else {
            $this->directory_cursor = 0;
            $this->directory_entries = array_map(function ($e) { return $e['name']; }, $res);
            $this->directory_path = $path;
            $return = true;
        }
        return $return;
    }

    /**
     * Support for readdir().
     *
     * @return
     *   The next filename, or FALSE if there are no more files in the directory.
     *
     * @see http://php.net/manual/en/streamwrapper.dir-readdir.php
     */
    public function dir_readdir() 
    {
        if (is_null($this->directory_cursor)) {
            // No previous call to opendir() before?
            return false;
        }
       
        assert (is_int($this->directory_cursor));
        assert (is_array($this->directory_entries));

        $return = null;
        $i = $this->directory_cursor;
        if ($i < count($this->directory_entries)) {
            $return = $this->directory_entries[$i];
            $this->directory_cursor = $i + 1;
        } else {
            $return = false;
        }
        
        return $return; 
    }

    /**
     * Support for rewinddir().
     *
     * @return
     *   TRUE on success.
     *
     * @see http://php.net/manual/en/streamwrapper.dir-rewinddir.php
     */
    public function dir_rewinddir() 
    {
        assert (is_int($this->directory_cursor));
        $this->directory_cursor = 0;
        return true;
    }

    /**
     * Support for closedir().
     *
     * @return
     *   TRUE on success.
     *
     * @see http://php.net/manual/en/streamwrapper.dir-closedir.php
     */
    public function dir_closedir() 
    {
        assert (is_int($this->directory_cursor));
        $this->directory_pointer = null;
        unset($this->directory_entries);
        return true;
    }
}
