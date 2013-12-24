<?php
/**
 * @file
 * Creates a new core for the Drupal instance.
 */

define('DEV_SOLR_BASE', 'http://localhost:8983/solr/');

/**
 * Implements hook_drush_command().
 */
function dev_solr_drush_command() {
  $items = array();

  $common_option = array(
    'options' => array(
      'solr-multicore-home' => 'Path to multicore directory within Solr installation.',
      'solr-config' => 'Site-specific Solr config directory.',
    ),
  );

  $items['dev-solr-setup'] = array(
    'callback' => 'dev_solr_setup',
    'description' => 'Creates a new Solr core for the site',
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_FULL,
  ) + $common_option;
  $items['dev-solr-start'] = array(
    'callback' => 'dev_solr_start',
    'description' => 'Starts the multi-core Solr engine',
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
  ) + $common_option;
  $items['dev-solr-stop'] = array(
    'callback' => 'dev_solr_stop',
    'description' => 'Stops the multi-core Solr engine',
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
  ) + $common_option;

  $items['dev-solr-restart'] = array(
    'callback' => 'dev_solr_restart',
    'description' => 'Restarts the multi-core Solr engine',
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
  ) + $common_option;
  $items['dev-solr-configure'] = array(
    'callback' => 'dev_solr_configure',
    'description' => 'Configures the site to use the local multi-core Solr',
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_FULL,
  );
  $items['dev-solr-get-config'] = array(
    'callback' => 'dev_solr_get_config',
    'description' => 'Shows configuration for the given site',
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_FULL,
  );

  return $items;
}

/**
 * Returns the home of the multicore directory within the Solr installation.
 */
function dev_solr_get_multicore() {
  return drush_get_option('solr-multicore-home', '/opt/apache-solr-3.6.2/dev/multicore');
}

/**
 * Command to create a Solr core for the site.
 */
function dev_solr_setup() {
  $path = FALSE;
  if (module_exists('search_api_solr')) {
    $path = drupal_get_path('module', 'search_api_solr');
    $drupal_solr_conf_path = realpath($path . '/solr-conf/3.x');
  }
  elseif (module_exists('apachesolr')) {
    $path = drupal_get_path('module', 'apachesolr');
    $drupal_solr_conf_path = realpath($path . '/solr-conf/solr-3.x');
  }

  $solr_config_opt = drush_get_option('solr-config', FALSE);
  if ($solr_config_opt !== FALSE) {
    $drupal_solr_conf_path = $solr_config_opt;
  }

  $core_path = dev_solr_get_multicore();
  $solr_conf = $core_path . '/solr.xml';

  $doc = new DOMDocument();
  $doc->preserveWhiteSpace = FALSE;
  $doc->formatOutput = TRUE;
  $doc->load($solr_conf);
  $core_name = dev_solr_get_core_name();
  $xpath = new DOMXpath($doc);

  // Do not recreate existing cores.
  if ($xpath->query("//cores/core[@name='{$core_name}']")->length > 0) {
    return drush_set_error('existing', 'Core already exists');
  }

  // Adds new core to config file.
  $elements = $xpath->query("//cores");
  $cores = $elements->item(0);
  $child = $doc->createElement('core');
  $child->setAttribute('name', $core_name);
  $child->setAttribute('instanceDir', $core_name);
  $cores->appendChild($child);
  file_put_contents($solr_conf, $doc->saveXML());

  // Initializes core and copies configuration.
  mkdir($core_path . '/' . $core_name);
  mkdir($core_path . '/' . $core_name . '/conf');
  dev_solr_copy_config($core_path . '/../solr/conf', $core_path . '/' . $core_name . '/conf');
  dev_solr_copy_config($drupal_solr_conf_path, $core_path . '/' . $core_name . '/conf');
}

/**
 * Command to start the multi-core Solr engine.
 */
function dev_solr_start() {
  popen('cd ' . dev_solr_get_multicore() . '/../ && nohup java -Dsolr.solr.home=multicore -jar start.jar &
echo $! > ' . dev_solr_get_pidfile_path(), 'r');
}

/**
 * Command to restart the multi-core Solr engine.
 */
function dev_solr_restart() {
  dev_solr_stop();
  dev_solr_start();
}

/**
 * Command to stop the multi-core Solr engine.
 */
function dev_solr_stop() {
  $pid = file_get_contents(dev_solr_get_pidfile_path());
  if (!empty($pid)) {
    popen('kill `pgrep -P ' . $pid . '`', 'r');
  }
  else {
    drush_log('There\'s no running Solr engine', 'warning');
    return;
  }
  // Waits for Solr to stop completely.
  do {
    usleep(200000);
    $pids = array();
    exec('pgrep -P ' . $pid, $pids);
  } while (!empty($pids));
  unlink(dev_solr_get_pidfile_path());
}

/**
 * Shows the configuration for the given site.
 */
function dev_solr_get_config() {
  drush_print(DEV_SOLR_BASE . dev_solr_get_core_name());
}

/**
 * Copies directory recursively.
 */
function dev_solr_copy_config($source, $dest) {
  $source = realpath($source);
  $dest = realpath($dest);
  foreach ($iterator = new RecursiveIteratorIterator(new RecursiveDirectoryIterator($source, RecursiveDirectoryIterator::SKIP_DOTS), RecursiveIteratorIterator::SELF_FIRST) as $item) {
    if ($item->isDir()) {
      mkdir($dest . DIRECTORY_SEPARATOR . $iterator->getSubPathName());
    }
    else {
      copy($item, $dest . DIRECTORY_SEPARATOR . $iterator->getSubPathName());
    }
  }
}

/**
 * Returns a generated Solr core name for the current site.
 */
function dev_solr_get_core_name() {
  $db = _drush_sql_get_db_spec();
  return str_replace('-', '_', drupal_clean_css_identifier($db['database']));
}

/**
 * Returns a suitable path for a file to store the process ID.
 */
function dev_solr_get_pidfile_path() {
  $dir = $_SERVER['HOME'] . '/.drush_dev_solr';
  if (!file_exists($dir)) {
    mkdir($dir);
  }
  return $dir . '/pid';
}
