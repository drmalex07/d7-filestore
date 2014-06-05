<?php

module_load_include('inc', 'filestore', 'filestore');

function filestore_admin_config_form($form, &$form_state)
{
    $config = (array) FTP_Stream::get_config();
    
    $form['ftp_stream'] = array(
        '#type' => 'fieldset',
        '#title' => 'FTP Streams',
        '#description' => t('Configure FTP-based stream wrappers (!scheme://).', array(
            '!scheme' => FILESTORE_FTP_SCHEME
        )),
        '#collapsible' => true,
        '#collapsed' => false,
        '#tree' => true,
    );

    $form['ftp_stream']['host'] = array(
        '#title' => 'Host',
        '#type' => 'textfield',
        '#description' => t('Enter the hostname of the FTP site.'),
        '#attributes' => array(),
        '#default_value' => $config['host'],
        '#size' => 35,
        '#maxlength' => 45,
        '#required' => true,
    );
    
    $form['ftp_stream']['username'] = array(
        '#title' => 'Username',
        '#type' => 'textfield',
        '#default_value' => $config['username'],
        '#size' => 35,
        '#maxlength' => 45,
        '#required' => true,
    );
    
    $form['ftp_stream']['password'] = array(
        '#title' => 'Password',
        '#type' => 'password',
        '#default_value' => '',
        '#size' => 35,
        '#maxlength' => 45,
        '#required' => false,
    );

    $form['ftp_stream']['use_passive'] = array( 
        '#title' => 'Use PASSIVE operation mode',
        '#description' => t('If you are behind a firewall, you should choose PASSIVE mode.'),
        '#type' => 'checkbox',
        '#default_value' => !empty($config['use_passive']),
    );
    
    $form['ftp_stream']['use_site_commands'] = array(
        '#title' => 'Use SITE commands',
        '#description' => t('Note that certain stream methods may request for CHMOD (available as a SITE command).'),
        '#type' => 'checkbox',
        '#default_value' => !empty($config['use_site_commands']), 
    );
   
    $form['ftp_stream']['test_connection'] = array(
        '#type' => 'submit',
        '#value' => t('Test FTP connection'),
    );
   
    $form['save'] = array(
        '#type' => 'submit',
        '#value' => t('Save'),
    );
    
    $form['cancel'] = array(
        '#type' => 'submit',
        '#value' => t('Cancel'),
    );

    return $form;
}

function filestore_admin_config_form_submit($form, &$form_state)
{  
    $values =& $form_state['values'];
    $op = $values['op'];
    //var_dump($values);
    
    if ($op == t('Save')) {
        $config = array(
            'host' => $values['ftp_stream']['host'],
            'username' => $values['ftp_stream']['username'],
            'password' => $values['ftp_stream']['password'],
            'use_site_commands' => !empty($values['ftp_stream']['use_site_commands']),
            'use_passive' => !empty($values['ftp_stream']['use_passive']),
        );
        $config = FTP_Stream::update_config($config);
        drupal_set_message('Saved configuration settings', 'status');
    } elseif ($op == t('Test FTP connection')) { 
        $form_state['redirect'] = false;
        $store = new Net_FTP();
        $host = $values['ftp_stream']['host'];
        $res = $store->connect($host);
        if ($res instanceof Pear_Error) {
            drupal_set_message("Failed to connect to ftp://${host}", 'error');
            return;
        } else {
            drupal_set_message("Successfully connected to ftp://${host}", 'status'); 
        } 
        $user = $values['ftp_stream']['username'];
        $pass = $values['ftp_stream']['password'];
        $res = $store->login($user, $pass);
        if ($res instanceof Pear_Error) {
            drupal_set_message("Failed to authenticate to ftp://${host} as ${user}", 'error');
            return; 
        } else {
            drupal_set_message("Successfully authenticated to ftp://${host} as ${user}", 'status'); 
        }
    } else {
        // noop
    }
    
    return;
}

function filestore_admin_config_form_validate($form, &$form_state)
{
    return;
}

function filestore_admin_config_migrate_fields()
{
    // @note
    // We attempt to migrate file URIs to another storage scheme, e.g. 
    // from public://foo/hello.txt to store-1://foo/hello.txt.
    // We do NOT attempt to migrate file data to another storage container.

    // Retreive basic info for file-related fields

    $q1 = db_select('field_config', 'f')
        ->fields('f', array('field_name', 'type', 'data'))
        ->condition('type', array('file','image'), 'IN')
        ->condition('active', 1);
    
    $stream_wrappers = file_get_stream_wrappers();

    $field_info = array();
    $r1 = $q1->execute();
    while (($row = $r1->fetchAssoc()) != null) {
        $data = unserialize($row['data']);
        $field_name = $row['field_name'];
        $field_data_table = "field_data_${field_name}";
        // How many files are associated with this field ?
        $q2 = db_select($field_data_table, 'd')
            ->fields('d')
            ->countQuery();
        $count = $q2->execute()->fetchField();
        // Which content types are using this field ?
        $q3 = db_select('field_config_instance', 'i')
            ->fields('i', array('bundle'))
            ->condition('field_name', $field_name);
        $bundles = $q3->execute()->fetchCol();
        // Gather info
        $field_info[$field_name] = array(
            'name' => $field_name,
            'type' => $row['type'],
            'bundles' => $bundles,
            'uri_scheme' => $data['settings']['uri_scheme'],
            'class' => $stream_wrappers[$data['settings']['uri_scheme']]['class'],
            'count_files' => $count,
        );
    }
   
    //fb($field_info);
  
    // Render info in a table
    
    $header = array(
        'Field Name',
        'Type',
        'Bundles',
        'URI Scheme',
        'Class',
        'Number',
        'Operations',
    );

    $rows = array_map(function ($f) { 
        return array(
            '<tt>' . $f['name'] . '</tt>',
            '<tt>' . $f['type'] . '</tt>',
            '<tt>' . join(',', $f['bundles']) . '</tt>',
            '<tt>' . $f['uri_scheme'] . '://' . '</tt>',
            '<tt>' . $f['class'] . '</tt>',
            $f['count_files'],
            l('Migrate', "admin/config/media/filestore/migrate-field/" . $f['name']),
        );
    }, $field_info);

    $table = array('sticky' => true, 'header' => & $header, 'rows' => & $rows);
    
    //var_dump($rows);

    $ra['results'] = array(
        '#type' => 'markup',
        '#attributes' => array(),
        '#markup' => theme('table', $table),
    );

    return $ra;
}

function filestore_admin_config_migrate_field_form($form, &$form_state, $field_name)
{
    //fb($form_state);

    $step = isset($form_state['step'])? ($form_state['step']) : 'select';
    
    $field_config = _filestore_admin_get_field_config($field_name); 

    switch ($step) {
    case 'confirm':
        return filestore_admin_config_migrate_field_form_step_confirm($form, $form_state, $field_config);
        break;
    case 'select': 
    default:
        return filestore_admin_config_migrate_field_form_step_select($form, $form_state, $field_config);
        break;
    }
}

function filestore_admin_config_migrate_field_form_step_select($form, &$form_state, &$field_config)
{     
    $field_name = $field_config['field_name'];
    $field_data_table = "field_data_${field_name}";

    $scheme = $field_config['data']['settings']['uri_scheme'];
    
    $registered_schemes = array_keys(file_get_stream_wrappers());

    // Build form
    
    $form['name'] = array(
        '#type' => 'hidden', 
        '#value' => $field_name,
    );
        
    $form['current_scheme'] = array(
        '#type' => 'hidden', 
        '#value' => $scheme,
    );

    $form['name_item'] = array(
        '#type' => 'item', 
        '#title' => 'Field:', 
        '#markup' => "<tt>${field_name}</tt>",
    );
        
    $form['current_scheme_item'] = array(
        '#type' => 'textfield',
        '#size' => 16,
        '#title' => 'Current Scheme:', 
        '#default_value' => $scheme,
        '#attributes' => array('readonly' => 'readonly'),
    );
    
    $form['target_scheme'] = array(
        '#type' => 'select', 
        '#title' => 'Target Scheme:',
        '#options' => drupal_map_assoc(array_diff($registered_schemes, array($scheme, 'temporary'))),
    );
  
    $form['options'] = array(
        '#type' => 'checkboxes',
        '#title' => 'Migration Options:',
        '#options' => array(
            'migrate_data' => t('Migrate data. Create a cron task to accomplish this.')
        ),
        '#default_value' => array(
            'migrate_data'
        ),
    );
    
    $form['next'] = array('#type' => 'submit', '#value' => t('Next'));
    $form['cancel'] = array('#type' => 'submit', '#value' => t('Cancel'));
    
    return $form;
}

function filestore_admin_config_migrate_field_form_step_confirm($form, &$form_state, &$field_config)
{
    $field_name = $field_config['field_name'];
    $field_data_table = "field_data_${field_name}";
    
    $count_affected = null;
    $sample_affected = _filestore_admin_get_field_managed_files($field_name, $count_affected, $limit=20);
    
    $table = array(
        'sticky' => true, 
        'header' => array('Id', 'URI', 'Web URL', 'Bundle', 'Entity URL'), 
        'rows' => array_map(function ($row) { 
            $entity_ref = $row['entity.ref'];
            $web_url = file_create_url($row['file.uri']);
            return array(
                $row['file.id'],
                $row['file.uri'],
                l($web_url, $web_url, array('attributes' => array('target' => '_blank'))),
                $row['entity.bundle'],
                l(url($entity_ref), $entity_ref, array('attributes' => array('target' => '_blank'))),
            );
        }, $sample_affected),
    );
    
    $report_link = "admin/config/media/filestore/get-managed-files/${field_name}";

    $form['info-wrapper'] = array(
        '#type' => 'container',
        '#attributes' => array(),
        'heading' => array(
            '#type' => 'markup',
            '#markup' => t('<h3>Confirm changes</h3>'),
        ),
        'summary' => array(
            '#type' => 'markup',
            '#markup' => t('<p>You are about to change URI scheme for field <strong><tt>!name</tt></strong> and migrate file URIs from <tt>!from</tt> to <tt>!to</tt>.</p>', array(
                '!from' => $form_state['state']['current_scheme'] . '://', 
                '!to' => $form_state['state']['target_scheme'] . '://', 
                '!name' => $field_name,
            )),
        ),
        'details-1' => array(
            '#type' => 'markup',
            '#markup' => t('<p>If you proceed, a total of <strong>!count</strong> files will be affected. Export !report_link.</p>', array(
                '!count' => $count_affected,
                '!report_link' => l(t('report'), $report_link),
            )),
        ),
        'sample' => array(
            '#type' => 'markup',
            '#markup' => theme('table', $table),
        ),
    );

    $form['migrate'] = array('#type' => 'submit', '#value' => t('Migrate'));
    $form['previous'] = array('#type' => 'submit', '#value' => t('Previous'));
    
    return $form;
}

function filestore_admin_config_migrate_field_form_submit($form, &$form_state)
{
    $step = isset($form_state['step'])? ($form_state['step']) : 'select';
    
    $values =& $form_state['values'];
    $op = $values['op'];    

    switch ($step) {
    case 'confirm':
        {
            // All needed input is collected: apply changes.
            if ($op == t('Previous')) {
                $form_state['step'] = 'select';
                $form_state['state'] = array();
                $form_state['rebuild'] = true;
            } else {
                _filestore_admin_migrate_field(
                    $field_name = $form_state['state']['name'], 
                    $current_scheme = $form_state['state']['current_scheme'],
                    $target_scheme = $form_state['state']['target_scheme'],
                    $migrate_data = $form_state['state']['migrate_data']
                );
                $form_state['redirect'] = 'admin/config/media/filestore/migrate-fields';
            }
        }
        break;
    case 'select': 
    default:
        {
            // Prepare for next step: confirm
            if ($op == t('Cancel')) {
                $form_state['redirect'] = 'admin/config/media';
            } else {
                $form_state['step'] = 'confirm';
                $form_state['state'] = array(
                    'name' => $values['name'],
                    'current_scheme' => $values['current_scheme'],
                    'target_scheme' => $values['target_scheme'],
                    'migrate_data' => !empty($values['options']['migrate_data']),
                );
                $form_state['rebuild'] = true;
            }
        }
        break;
    }

    
    return;
}

function _filestore_admin_migrate_field($field_name, $current_scheme, $target_scheme, $migrate_data=true)
{
    $field_data_table = "field_data_${field_name}";
    $field_config = _filestore_admin_get_field_config($field_name);  
   
    $files = _filestore_admin_get_field_managed_files($field_name, $count_files);
    
    $field_config['data']['settings']['uri_scheme'] = $target_scheme;
    $update_scheme = db_update('field_config')
        ->fields(array('data' => serialize($field_config['data'])))
        ->condition('field_name', $field_name)
        ->condition('active', 1);
   
    $migration_tasks = DrupalQueue::get('filestore_field_migrate_data');
    
    $x = db_transaction();
    $count_tasks = 0;
    try {
        // Update field's URI scheme
        $updated = $update_scheme->execute(); 
        // Create tasks for data migration
        if ($migrate_data) {
            foreach (array_keys($files) as $fid) {
                $task = (object) array(
                    'type' => 'field-migrate-data',
                    'fid' => $fid,
                    'current_scheme' => $current_scheme,
                    'target_scheme' => $target_scheme,
                );
                $migration_tasks->createItem($task);
                $count_tasks++;
            }
        }
    } catch (Exception $e) {
        $x->rollback();
        drupal_set_message('Failed to migrate file URIs', 'error');
        watchdog('filestore', 'Failed to migrate file URIs: @msg', array('@msg' => $e->getMessage()));
        return false;
    }
    unset($x); /* commit */

    watchdog('filestore', 'Updated URI scheme (!scheme) for field !field. Created !n data migration tasks.', array(
        '!scheme' => $target_scheme . '://', '!field' => $field_name, '!n' => $count_tasks,
    )); 
    drupal_set_message("Migrated ${count_files} file URIs to scheme ${target_scheme}://.", 'status');

    return true;
}

function filestore_admin_get_field_managed_files($field_name)
{
    $return = _filestore_admin_get_field_managed_files($field_name, $count);
    drupal_json_output($return);
}

function _filestore_admin_get_field_managed_files($field_name, &$count, $limit=null)
{
    $field_data_table = "field_data_${field_name}";
    
    $q = db_select($field_data_table, 'd')
        ->fields('d', array('entity_type', 'entity_id', 'bundle'));
    $a = $q->join('file_managed', 'f', "f.fid = d.${field_name}_fid");
    $q = $q->fields('f');
    
    $count = $q->countQuery()->execute()->fetchField();

    if (is_integer($limit)) {
        $q = $q->range(0, $limit);
    }
    
    $res = $q->execute();
    $return = array();
    while (($row = $res->fetchAssoc()) != null) {
        $fid = $row['fid'];
        $return[$fid] = array(
            'file.id' => $fid,
            'file.uri' => $row['uri'],
            'file.mimetype' => $row['filemime'],
            'file.name' => $row['filename'],
            'entity.bundle' => $row['bundle'],
            'entity.ref' => $row['entity_type'] . '/' . $row['entity_id'],
        );
    }
    
    return $return;
}

function _filestore_admin_get_field_config($field_name)
{
    $q = db_select('field_config', 'f')
        ->fields('f', array('field_name', 'type', 'data'))
        ->condition('field_name', $field_name)
        ->condition('active', 1);

    $field_config = $q->execute()->fetchAssoc();
    $field_config['data'] = unserialize($field_config['data']);
    
    return $field_config;
}

function _filestore_admin_get_field_instance_config($field_name)
{
    $q = db_select('field_config_instance', 'f')
        ->fields('f', array('field_name', 'entity_type', 'bundle', 'data'))
        ->condition('field_name', $field_name)
        ->condition('deleted', 0);

    $field_config = $q->execute()->fetchAssoc();
    $field_config['data'] = unserialize($field_config['data']);
    
    return $field_config;
}

function _filestore_admin_migrate_field_data_process($item)
{
    // @Note: This is an asynchronous task processed by a queue worker. 
    // When it should be considered as FAILED (and so retried) we MUST throw an exception!

    watchdog('filestore', 'Processing queue item: !w', array('!w' => print_r($item,1)), WATCHDOG_DEBUG);
   
    assert ($item->{'type'} == 'field-migrate-data');
   
    $fid = (int) $item->{'fid'};
    $target_scheme = $item->{'target_scheme'};
    
    $q = db_select('file_managed', 'f')
        ->fields('f')
        ->condition('fid', $fid);

    $file = $q->execute()->fetch();
    $new_file = null;
    if (is_object($file)) {
        $uri = $file->{'uri'};
        // Compute destination 
        $target_uri = $target_scheme . '://' . file_uri_target($uri);
        $target_dir = drupal_dirname($target_uri);
        // Prepare directory
        $prepared = file_prepare_directory($target_dir, FILE_CREATE_DIRECTORY);
        if (!$prepared) {
            watchdog('filestore', 'Failed to prepare directory !target_dir', array(
                '!target_dir' => $target_dir,
            ), WATCHDOG_ERROR);
            throw new RuntimeException("Failed to prepare directory ${target_dir}");
        }
        // Move file to target-uri
        $new_file = file_move($file, $target_uri);
        if (!is_object($new_file)) {
             watchdog('filestore', 'Failed to move file #!fid (!uri) to !target_uri', array(
                '!fid' => $fid, '!uri' => $uri, '!target_uri' => $target_uri,
            ), WATCHDOG_ERROR);
            throw new RuntimeException("Failed to move file #${fid} under ${target_scheme}://");
        } else {
            watchdog('filestore', 'Moved file #!fid (!uri) to !new_uri', array(
                '!fid' => $fid, '!uri' => $uri, '!new_uri' => $new_file->{'uri'},
            ), WATCHDOG_INFO);
        }
    }
    
    return;
}
