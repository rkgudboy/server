IF(HAVE_EncryptAes128Ctr)
  MYSQL_ADD_PLUGIN(EXAMPLE_KEY_MANAGEMENT_PLUGIN example_key_management_plugin.cc
                   MODULE_ONLY)
ENDIF()
