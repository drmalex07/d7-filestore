<?php

define ('FILESTORE_FTP_SCHEME', 'store-ftp');
define ('FILESTORE_FOO_SCHEME', 'store-foo');

module_load_include('inc', 'filestore', 'filestore.foo_stream');
module_load_include('inc', 'filestore', 'filestore.ftp_stream');

