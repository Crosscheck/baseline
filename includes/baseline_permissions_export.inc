<?php

/**
 * @file
 *
 * Contains logic for the drush baseline permissions export command.
 */
class BaselinePermissionsExporter {

  public $module;
  public $roles;
  public $components;
  public $old_permissions = array();
  public $requested_permissions;
  public $new_permissions;
  protected $needs_cache_clear;
  protected $hook = 'baseline_info_user_permissions';
  protected $directory;
  protected $config_file;
  protected $errors;

  /**
   * Constructor function.
   */
  public function __construct($module, $roles, $components) {
    $this->setComponents($components);
    $this->setModule($module);
    $this->setRoles($roles);
    $this->setDirectory();
    $this->setFilename();

    if ($this->fileExists()) {
      $this->setOldPermissions();
    }
  }

  /**
   * Exports permissions to a module.
   */
  public function exportPermissionsToModule() {
    if (!empty($this->components) && $this->fileExists()) {
      $this->setRequestedComponentPermissions();
    }
    else {
      $this->setRequestedPermissions();
    }

    $this->setNewPermissions();
    $this->writePermissionsFile();
    $this->writeRequireEntry();
    $this->clearCache();
  }

  /**
   * Sets additional arguments.
   *
   * @param mixed $components
   */
  public function setComponents($components) {
    if (empty($components)) {
      $this->components = $components;
    }
    else if (strtolower($components) == 'all') {
      $this->components = $components;
    }
    else {
      $components = explode(',', $components);
      $components = $this->filterCorrectComponents($components);
      $this->components = $components;
    }
  }

  protected function filterCorrectComponents($components) {
    $filtered_components = array();

    foreach ($components as $component) {
      if (module_exists($component) && $this->hasPermissions($component)) {
        $filtered_components[] = $component;
      }
      else if (!$this->hasPermissions($component)) {
        $this->errors[] = t('The component @component has no permissions', array('@component' => $component));
      }
      else {
        $this->errors[] = t('The component @component does not exist', array('@component' => $component));
      }
    }

    return $filtered_components;
  }

  /**
   * Checks wether or not a module has a component.
   *
   * @param $component
   *  the module to check.
   *
   * @return bool
   *  Whether or not there were permissions found.
   */
  protected function hasPermissions($component) {
    $permissions = $this->getAllPermissionsByComponent($component);

    if (!empty($permissions)) {
      return TRUE;
    }

    return FALSE;
  }

  /**
   * Sets the module name.
   *
   * @param mixed $module
   */
  public function setModule($module) {
    if (module_exists($module)) {
      $this->module = $module;
    }
    else {
      $this->errors[] = t('This module does not exist');
    }
  }

  /**
   * Sets the wanted roles.
   *
   * @param mixed $roles_argument
   */
  public function setRoles($roles_argument) {
    if (strtolower($roles_argument) == 'all') {
      $roles = $this->getAllRoles();
    }
    else {
      $roles = explode(',', $roles_argument);
      $roles = $this->getRequestedRoles($roles);
    }

    $this->roles = $roles;
  }

  /**
   * Creates an array containing the asked roles.
   *
   * Filters out the non-existing roles.
   *
   * @param $requested_roles
   *  The roles array as entered by a user.
   *
   * @return array
   *  An of array all roles.
   */
  public function getRequestedRoles($requested_roles) {
    $roles = array();

    foreach ($requested_roles as $requested_role) {
      $user_role = $this->getRequestedRole($requested_role);

      if ($user_role) {
        $roles[] = $user_role;
      }
    }

    return $roles;
  }

  /**
   * Returns all roles.
   */
  public function getAllRoles() {
    $roles = array();
    $current_roles = user_roles();

    foreach ($current_roles as $role_name) {
      $user_role = user_role_load_by_name($role_name);

      if ($user_role) {
        $roles[] = $user_role;
      }
    }

    return $roles;
  }

  /**
   * Sets the requested content permissions
   */
  protected function setRequestedComponentPermissions() {
    foreach ($this->roles as $role) {
      $role_permissions[$role->name] = $this->getComponentPermissionsByRole($role);
    }

    $permissions = $this->replaceComponentPermissions($role_permissions);
    $this->requested_permissions = $permissions;
  }

  /**
   * Retrieves the old permissions and adds the reqeusted component permissions
   *
   * @param $role_permissions
   *  The requested component permissions per role.
   *
   * @return array
   *  The requested components.
   */
  protected function replaceComponentPermissions($role_permissions) {
    $permissions = array();
    $old_permissions = $this->old_permissions;

    // Loop through the roles and requested components.
    foreach ($role_permissions as $role => $components) {
      foreach ($components as $component => $requested_component_permissions) {
        // Retrieve all component permissions.
        $all_component_permissions = $this->getAllPermissionsByComponent($component);

        // Loop through them and unset them from our role array.
        foreach ($all_component_permissions as $permission) {
          if(isset($old_permissions[$role]) && (($key = array_search($permission, $old_permissions[$role])) !== false)) {
            unset($old_permissions[$role][$key]);
          }
        }

        // Add the new permissions to the array.
        if (isset($old_permissions[$role])) {
          $permissions[$role] = array_merge($old_permissions[$role], $requested_component_permissions);
          $old_permissions[$role] = $permissions[$role];
        }
        else {
          $permissions[$role] = $requested_component_permissions;
        }
      }
    }

    return $permissions;
  }

  /**
   * Returns all permission by module.
   *
   * @param $component
   *  The requested module.
   *
   * @return array
   *  The permissions for a certain module.
   */
  protected function getAllPermissionsByComponent($component) {
    $permissions = module_invoke($component, 'permission');

    if (!empty($permissions)) {
      return array_keys($permissions);
    }

    return array();
  }

  /**
   * Returns all permissions by roles and components.
   *
   * @param $role
   *  The requested role.
   *
   * @return array
   *  All module permissions per role.
   */
  protected function getComponentPermissionsByRole($role) {
    $requested_permissions = array();

    foreach ($this->components as $component) {
      $query = db_select('role_permission', 'rp');
      $query->fields('rp', array('permission'));
      $query->condition('rp.rid', $role->rid, '=');
      $query->condition('rp.module', $component, '=');
      $result = $query->execute()->fetchAllAssoc('permission', PDO::FETCH_ASSOC);
      $component_permissions = array_keys($result);
      $requested_permissions[$component] = $component_permissions;
    }

    return $requested_permissions;
  }

  /**
   * Sets the old permissions.
   */
  public function setOldPermissions() {
    $old_permissions = array();
    $function = $this->module . '_' . $this->hook;

    if (function_exists($function)) {
      $old_permissions = $function();
    }

    $this->old_permissions = $this->formatKeys($old_permissions);
  }

  /**
   * Sets the requested permissions.
   */
  public function setRequestedPermissions() {
    $requested_permissions = array();

    foreach ($this->roles as $role) {
      $query = db_select('role_permission', 'rp');
      $query->fields('rp', array('permission'));
      $query->condition('rp.rid', $role->rid, '=');
      $query->orderBy('module');
      $result = $query->execute()->fetchAllAssoc('permission', PDO::FETCH_ASSOC);
      $role_permissions = array_keys($result);
      $requested_permissions[$role->name] = $role_permissions;
    }

    $this->requested_permissions = $requested_permissions;
  }

  /**
   * Returns a role based on the name or id.
   *
   * @param $requested_role
   *  The role id or name.
   *
   * @return mixed
   *  The matched role.
   */
  public function getRequestedRole($requested_role) {
    if (is_numeric($requested_role)) {
      return user_role_load($requested_role);
    }
    else {
      return user_role_load_by_name($requested_role);
    }
  }

  /**
   * Set the permissions that will be written to the file.
   */
  public function setNewPermissions() {
    $old_permissions = $this->old_permissions;
    $requested_roles = array_keys($this->requested_permissions);

    foreach ($requested_roles as $role_name) {
      unset($old_permissions[$role_name]);
    }

    $this->new_permissions = array_merge($this->requested_permissions, $old_permissions);
  }

  /**
   * Change role id keys to name keys.
   *
   * @param $permissions
   *  The permissions array to reformat.
   *
   * @return array
   *  The formatted permissions array.
   */
  protected function formatKeys($permissions) {
    $formatted_keys = array();

    foreach ($permissions as $user_role_key => $permission_array) {
      $user_role = $this->getRequestedRole($user_role_key);

      if ($user_role) {
        $formatted_keys[$user_role->name] = $permission_array;
      }
    }

    return $formatted_keys;
  }

  /**
   * Checks to see if the current config file already exists.
   *
   * If so, it includes it so we can read it and use the contents within it.
   *
   * @return bool
   *  Whether or not the file exists.
   */
  protected function fileExists() {
    if (file_exists($this->directory . '/' . $this->config_file)) {
      module_load_include('inc', $this->module, 'includes/' . str_replace('.inc', '', $this->config_file));
      $this->needs_cache_clear = FALSE;
      return TRUE;
    }

    $this->needs_cache_clear = TRUE;
    return FALSE;
  }

  /**
   * Sets the directory name where the user permissions file will be stored.
   */
  protected function setDirectory() {
    $this->directory = drupal_get_path('module', $this->module) . '/includes';
  }

  /**
   * Sets the filename of the file containing the user permissions.
   */
  protected function setFilename() {
    $this->config_file = $this->module . '.user_permissions.inc';
  }

  /**
   * Writes a file to a specified location.
   *
   * @param string $location.
   *  The uri of the location.
   *
   * @param string $content
   *  The content of the file.
   */
  protected function writeFile($location, $content) {
    $fp = fopen($location, 'w');
    fwrite($fp, $content);
    fclose($fp);
  }

  /**
   * Builds and writes the permissions file.
   */
  protected function writePermissionsFile() {
    $php_file = "<?php\n\n/** \n* @file \n*  Contains the exported user permissions. \n*/\n";
    $php_file .= "\n\n/** \n* Implements hook_" . $this->hook . "(). \n*/";
    $php_file .= "\nfunction " . $this->module . "_" . $this->hook . "() {\n";
    $php_file .= "  " . '$permissions = array();'. "\n\n";

    foreach ($this->new_permissions as $key => $role_permissions) {
      $export = preg_replace('/^/m', " " , preg_replace('(\d+\s=>)', "", var_export($role_permissions, true)));

      if ($key == 'anonymous user') {
        $php_file .= "  " . '$permissions[DRUPAL_ANONYMOUS_RID] = ' . $export . ';' . "\n\n";

      }
      else if ($key == 'authenticated user') {
        $php_file .= "  " . '$permissions[DRUPAL_AUTHENTICATED_RID] = ' . $export . ';' . "\n\n";
      }
      else {
        $php_file .= "  " . '$permissions[\'' . $key .'\'] = ' . $export . ';' . "\n\n";
      }

    }

    $php_file .= '  return $permissions;' . "\n";
    $php_file .= "}";
    $this->writeFile($this->directory . '/' . $this->config_file, $php_file);
  }

  /**
   * Replaces the <?php tag with a <?php tag and a
   * require entry to the permissions file.
   */
  protected function writeRequireEntry() {
    $php_file = $this->readModuleFile();

    if (strpos($php_file, "require_once('includes/". $this->config_file . "');") === FALSE){
      $php_file = str_replace('<?php', "<?php \n\nrequire_once('includes/". $this->config_file . "');", $php_file);
    }

    $this->writeFile(str_replace('/includes', '', $this->directory) . '/' . $this->module . '.module', $php_file);
  }

  /**
   * Returns the content of the .module file, of the module that was requested.
   *
   * @return string
   *  The content of the .module file.
   */
  protected function readModuleFile() {
    $file = str_replace('/includes', '', $this->directory) . '/' . $this->module . '.module';
    return file_get_contents($file);
  }

  /**
   * Returns the set errors.
   */
  public function getErrors() {
    return $this->errors;
  }

  /**
   * Clears the cache
   */
  protected function clearCache() {
    if ($this->needs_cache_clear) {
      cache_clear_all();
    }
  }

  /**
   * Shows a message to the command line interface.
   *
   * @param $message
   *  The message to print.
   */
  public function showMessage($message) {
    drush_print(dt($message));
  }
}
