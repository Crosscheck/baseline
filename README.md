baseline
========

Baseline module for Drupal.

========================================================================================
HOW TO USE: PERMISSIONS EXPORTER COMMANDS
========================================================================================

Main command: drush baseline-export-permissions

Shortcut: drush bep


arguments:

- module (the module to which the permissions should be exported)
- role (the different roles that should have their permissions exported)
- components (the module(s) for which you want to export the permissions


examples:

* export all permissions to your core module:

drush bep [project_core] all

* export only permissions of editor and webmaster role:

drush bep [project_core] "editor","webmaster"

* export only permissions of editor and webmaster role using their rids:

drush bep [project_core] 4,5

* update permissions of a particular module

drush bep [project_core] all "views","devel"


notes:

* administrator will always have the module_invoke_all function
* the anonymous user and authenticated user will be used with their static defined rids.