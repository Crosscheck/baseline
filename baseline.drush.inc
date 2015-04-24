<?php

/**
 * @file
 * baseline commands for Drush
 */

/**
 * Implements hook_drush_command().
 */
function baseline_drush_command() {
  $items = array();

  $items['baseline'] = array(
    'description' => "Invokes hooks that get all the baseline info and then builds a baseline based on that info. to another.",
    'examples' => array(
      'drush baseline' => 'build baseline',
    ),
    'aliases' => array('bbs'),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
  );

  $items['baseline-permissions-export'] = array(
    'description' => "Exports all the baseline-permissions available and builds them.",
    'examples' => array(
      'drush baseline-export-permissions' => 'export permissions',
    ),
    'aliases' => array('bpe'),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
    'arguments' => array(
      'module' => 'The module where the permissions should be exported to.',
      'role' => 'Which role should have their permissions exported.',
    ),
    'drupal dependencies' => array('baseline'),
  );

  return $items;
}

/**
 * Implementation of hook_drush_help().
 */
function baseline_drush_help($section) {
  switch ($section) {
    case 'drush:baseline':
      return dt("Build the baseline.");
  }
}

/**
 * Actual function run by the baseline command.
 */
function drush_baseline() {
  if (drush_confirm(dt('Do you want to build the baseline'))) {
    module_load_include('module', 'baseline');
    baseline_build();
    drush_print(dt('Success.'));
  }
}

/**
 * Creates an export of user permissions.
 */
function drush_baseline_permissions_export($module, $roles, $components = array()) {
  module_load_include('inc', 'baseline', 'includes/baseline_permissions_export');
  $baseline_permission_exporter = new BaselinePermissionsExporter($module, $roles, $components);

  if (!empty($module) && !empty($roles)) {
    $baseline_permission_exporter->exportPermissionsToModule();
    $errors = $baseline_permission_exporter->getErrors();

    if (!empty($errors)) {
      $baseline_permission_exporter->showMessage('Non critical Errors:');

      foreach ($errors as $error) {
        $baseline_permission_exporter->showMessage($error);
      }
    }

    $baseline_permission_exporter->showMessage('Permissions were built.');
  }
  else if (!module_exists($module)) {
    $baseline_permission_exporter->showMessage('The module @module does not exist', array('@module' => $module));
  }
  else {
    $baseline_permission_exporter->showMessage('The module and the roles need to be filled out.');
  }
}
