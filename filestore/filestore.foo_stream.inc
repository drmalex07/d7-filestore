<?php

class Foo_Stream extends DrupalPublicStreamWrapper 
{
    public function getDirectoryPath() 
    {
        return '/var/local/data/storage_1/';
    }
}

