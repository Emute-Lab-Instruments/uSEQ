#ifndef USEQ_LISP_H_
#define USEQ_LISP_H_

////////////////////////////////////////////////////////////////////////////////
/// LISP INTEGRATION FUNCTION DECLARATIONS  
/// This header contains LISP integration function declarations
/// that need to be included within the uSEQ class definition.
////////////////////////////////////////////////////////////////////////////////

// System control functions
LISP_FUNC_DECL(useq_stop_all);
LISP_FUNC_DECL(useq_firmware_info);
LISP_FUNC_DECL(useq_report_firmware_info);

#ifdef ARDUINO
// Flash memory and storage functions
LISP_FUNC_DECL(useq_load_flash_info);
LISP_FUNC_DECL(useq_write_flash_info);
LISP_FUNC_DECL(useq_reboot);
LISP_FUNC_DECL(useq_set_my_id);
LISP_FUNC_DECL(useq_get_my_id);

// Memory management functions
LISP_FUNC_DECL(useq_memory_save);
LISP_FUNC_DECL(useq_memory_restore);
LISP_FUNC_DECL(useq_memory_erase);

// Flash environment functions
LISP_FUNC_DECL(useq_load_flash_env);
LISP_FUNC_DECL(useq_write_flash_env);
LISP_FUNC_DECL(useq_autoload_flash);

// System mode functions
LISP_FUNC_DECL(useq_enter_bootloader_mode);
LISP_FUNC_DECL(useq_enter_sync_mode);
LISP_FUNC_DECL(useq_send_sync_trigger);
#endif

// I2C communication functions
LISP_FUNC_DECL(useq_i2c_host_start);
LISP_FUNC_DECL(useq_i2c_send_to);
LISP_FUNC_DECL(useq_send_sync_trigger_i2c);

#endif // USEQ_LISP_H_