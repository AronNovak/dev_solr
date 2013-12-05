<?php
/**
 * Creates a new core for the Drupal instance.
 */

/**
 * Implements hook_drush_command().
 */
function dev_solr_drush_command() {
  $items = array();

  $items['dev-solr-setup'] = array(
    'callback' => 'dev_solr_setup',
    'description' => 'Creates a new Solr core for the site',
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_FULL,
  );
  return $items;
}

function dev_solr_get_multicore() {
  return '/opt/apache-solr-3.6.2/dev/multicore';
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
  $core_path = dev_solr_get_multicore();
  $solr_conf = $core_path . '/solr.xml';

  $doc = new DOMDocument();
  $xmlDoc->preserveWhiteSpace = false;
  $xmlDoc->formatOutput = true;
  $doc->load($solr_conf);
  $db = _drush_sql_get_db_spec();
  $core_name = str_replace('-', '_', drupal_clean_css_identifier($db['database']));
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

