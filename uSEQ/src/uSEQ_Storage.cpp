#include "uSEQ_Storage.h"
#include "uSEQ.h"
#include "utils/log.h"
#include "uSEQ_Modules.h"

#if USE_NEW_MODULES && USE_STORAGE_MODULE
// Storage implementations
void uSEQ::write_flash_env() {}
void uSEQ::load_flash_env() {}
void uSEQ::clear_all_outputs() {}
void uSEQ::erase_info_flash() {}
void uSEQ::set_my_id(int num) {}
void uSEQ::load_flash_info() {}
void uSEQ::write_flash_info() {}
void uSEQ::reset_flash_env_var_info() {}
void uSEQ::reboot() {}
std::pair<size_t, size_t> uSEQ::num_bytes_def_strs() const { return std::make_pair(0, 0); }
void uSEQ::copy_def_strings_to_buffer(char* buffer) {}
bool uSEQ::flash_has_been_written_before() { return false; }
void uSEQ::autoload_flash() {}

#endif // USE_NEW_MODULES && USE_STORAGE_MODULE