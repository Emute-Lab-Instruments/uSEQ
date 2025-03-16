#include "uSEQ.h"
#include "hardware/flash.h"


#include "pico/stdlib.h"

constexpr uintptr_t PICO_FLASH_START_ADDR = reinterpret_cast<uintptr_t>(XIP_BASE);
constexpr uintptr_t NUM_SECTORS = PICO_FLASH_SIZE_BYTES / FLASH_SECTOR_SIZE;

constexpr uintptr_t FLASH_INFO_SECTOR_SIZE = FLASH_SECTOR_SIZE;
constexpr uintptr_t FLASH_ENV_SECTOR_SIZE  = FLASH_SECTOR_SIZE;

constexpr uintptr_t FLASH_INFO_SECTOR_OFFSET_START =
    (NUM_SECTORS - 1) * FLASH_SECTOR_SIZE;
constexpr uintptr_t FLASH_INFO_SECTOR_OFFSET_END =
    FLASH_INFO_SECTOR_OFFSET_START + FLASH_SECTOR_SIZE - 1;

constexpr uintptr_t FLASH_ENV_SECTOR_OFFSET_START =
    (NUM_SECTORS - 2) * FLASH_SECTOR_SIZE;
constexpr uintptr_t FLASH_ENV_SECTOR_OFFSET_END =
    FLASH_ENV_SECTOR_OFFSET_START + FLASH_SECTOR_SIZE - 1;

// Write an arbitrary byte buffer to an offset position in flash storage
void write_to_flash(u_int8_t* buffer, size_t OFFSET_START, size_t SECTOR_SIZE,
                    size_t WRITE_SIZE)
{
    // 1. Save and temporarily disable interrupts
    uint32_t interrupts = save_and_disable_interrupts();
    // 2. Clear the entire sector
    // NOTE: this is mandatory for the write operation to work properly
    flash_range_erase(OFFSET_START, SECTOR_SIZE);
    // 3. Write the actual pages (which may be smaller than
    // the sector size we cleared)
    flash_range_program(OFFSET_START, buffer, WRITE_SIZE);
    // 4. Restore and resume interrupts
    restore_interrupts(interrupts);
}

void print_flash_vars()
{
    println("FLASH_START_ADDR: " + String((uint)PICO_FLASH_START_ADDR));
    println("PICO_FLASH_SIZE_BYTES: " + String(PICO_FLASH_SIZE_BYTES));
    println("FLASH_INFO_SECTOR_SIZE: " + String(FLASH_INFO_SECTOR_SIZE));
    println("FLASH_INFO_SECTOR_OFFSET_START: " +
            String(FLASH_INFO_SECTOR_OFFSET_START));
}

// Utils
void uSEQ::copy_def_strings_to_buffer(char* buffer)
{
    char* write_pos = buffer;

    for (auto& map : { m_defs, m_def_exprs })
    {
        for (auto& pair : map)
        {
            // NOTE: Unless we cast to c_str first, it seems
            // that there is an issue with multi-byte UTF-8 chars
            // confusing the String() constructor when reading...
            String name_ascii = pair.first.c_str();
            String def_ascii  = pair.second.to_lisp_src().c_str();

            // Write the name
            size_t name_length = name_ascii.length() + 1;
            memcpy(write_pos, name_ascii.c_str(), name_length);
            write_pos += name_length;

            // Write the definition
            size_t def_length = def_ascii.length() + 1;
            memcpy(write_pos, def_ascii.c_str(), def_length);
            write_pos += def_length;
        }
    }
}

size_t padding_to_nearest_multiple_of(size_t in, size_t quant)
{
    return (quant - (in % quant)) % quant;
}

size_t pad_to_nearest_multiple_of(size_t in, size_t quant)
{
    return in + padding_to_nearest_multiple_of(in, quant);
}

void uSEQ::erase_info_flash()
{
    flash_range_erase(FLASH_INFO_SECTOR_OFFSET_START, FLASH_INFO_SECTOR_SIZE);
    println("Erased info flash.");
}

bool uSEQ::flash_has_been_written_before()
{
    // Get a char* to the start of the info block and check to see whether
    // that points to the start of a c-style string (spelling "uSEQ" as
    // of 1.0 release)
    const char* const info_start =
        (char*)(PICO_FLASH_START_ADDR + FLASH_INFO_SECTOR_OFFSET_START);
    return std::strcmp(info_start, m_flash_stamp_str) == 0;
}

void uSEQ::write_flash_info()
{
    // 4k buffer
    u_int8_t buffer[FLASH_INFO_SECTOR_SIZE];

    // This will increment as we're writing
    u_int8_t* write_ptr = &buffer[0];

    // Write a specific type's worth of bytes to the buffer and
    // increment the write pointer
#define WRITE_SEQUENTIALLY_TO_BUFFER_SIZE(__item__, __size__)                       \
    std::memcpy(write_ptr, (u_int8_t*)&__item__, __size__);                         \
    write_ptr += __size__;

#define WRITE_SEQUENTIALLY_TO_BUFFER(__item__)                                      \
    WRITE_SEQUENTIALLY_TO_BUFFER_SIZE(__item__, sizeof(__item__));

    // This should always be at the top so that we can know if we've written
    // to the flash at least once
    std::strcpy((char*)write_ptr, m_flash_stamp_str);
    write_ptr += m_flash_stamp_size_bytes;

    // Writing starting from 0
    // NOTE: The order here should match the order in load_flash_info
    // NOTE: The order here should match the order in load_flash_info
    WRITE_SEQUENTIALLY_TO_BUFFER(m_my_id);
    WRITE_SEQUENTIALLY_TO_BUFFER(m_FLASH_ENV_SECTOR_SIZE);
    WRITE_SEQUENTIALLY_TO_BUFFER(m_FLASH_ENV_SECTOR_OFFSET_START);
    WRITE_SEQUENTIALLY_TO_BUFFER(m_FLASH_ENV_DEFS_SIZE);
    WRITE_SEQUENTIALLY_TO_BUFFER(m_FLASH_ENV_EXPRS_SIZE);
    WRITE_SEQUENTIALLY_TO_BUFFER(m_FLASH_ENV_STRING_BUFFER_SIZE);

    // Write buffer to flash
    write_to_flash(&buffer[0], FLASH_INFO_SECTOR_OFFSET_START,
                   FLASH_INFO_SECTOR_SIZE, FLASH_INFO_SECTOR_SIZE);

    println("Wrote module info to flash successfully.");
}

void uSEQ::load_flash_info()
{
    const u_int8_t* const info_sector_addr_ptr = reinterpret_cast<u_int8_t*>(
        PICO_FLASH_START_ADDR + FLASH_INFO_SECTOR_OFFSET_START);

    const u_int8_t* read_ptr = info_sector_addr_ptr;

#define READ_SEQUENTIALLY_FROM_FLASH_SIZE(__dest__, __size__)                       \
    std::memcpy(&__dest__, read_ptr, __size__);                                     \
    read_ptr += __size__;

#define READ_SEQUENTIALLY_FROM_FLASH(__item__)                                      \
    READ_SEQUENTIALLY_FROM_FLASH_SIZE(__item__, sizeof(__item__));

    if (flash_has_been_written_before())
    {
        // Skip the flash marker
        read_ptr += m_flash_stamp_size_bytes;

        // NOTE: The order here should match the order in write_flash_info
        READ_SEQUENTIALLY_FROM_FLASH(m_my_id);
        READ_SEQUENTIALLY_FROM_FLASH(m_FLASH_ENV_SECTOR_SIZE);
        READ_SEQUENTIALLY_FROM_FLASH(m_FLASH_ENV_SECTOR_OFFSET_START);
        READ_SEQUENTIALLY_FROM_FLASH(m_FLASH_ENV_DEFS_SIZE);
        READ_SEQUENTIALLY_FROM_FLASH(m_FLASH_ENV_EXPRS_SIZE);
        READ_SEQUENTIALLY_FROM_FLASH(m_FLASH_ENV_STRING_BUFFER_SIZE);
    }
    else
    {
        println("Warning: Attempted to read info from flash but it seems that "
                "nothing has been written there yet. Try running "
                "`(write-flash-info)` first. You will only need to do this "
                "once.");
    }
}

std::pair<size_t, size_t> uSEQ::num_bytes_def_strs() const
{
    size_t defs_size  = 0;
    size_t exprs_size = 0;

    int i = 0;
    for (const auto& map : { m_defs, m_def_exprs })
    {
        size_t size = 0;

        for (const auto& pair : map)
        {
            size += pair.first.length() + pair.second.to_lisp_src().length() + 2;
        }

        if (i == 0)
        {
            defs_size = size;
        }
        else if (i == 1)
        {
            exprs_size = size;
        }

        i++;
    }

    return { defs_size, exprs_size };
}

void uSEQ::write_flash_env()
{
    // 1. Collect all env strings and figure out their total size
    std::pair<size_t, size_t> pair = num_bytes_def_strs();
    m_FLASH_ENV_DEFS_SIZE          = pair.first;
    m_FLASH_ENV_EXPRS_SIZE         = pair.second;
    m_FLASH_ENV_STRING_BUFFER_SIZE = pair.first + pair.second;

    size_t buffer_size =
        pad_to_nearest_multiple_of(m_FLASH_ENV_STRING_BUFFER_SIZE, FLASH_PAGE_SIZE);

    if (buffer_size % FLASH_PAGE_SIZE != 0)
    {
        println("Error: Flash env buffer size is not a multiple of the page "
                "size: " +
                String(buffer_size));
        return;
    }

    m_FLASH_ENV_SECTOR_SIZE =
        pad_to_nearest_multiple_of(buffer_size, FLASH_SECTOR_SIZE);

    // The env sector ends where the info sector begins
    m_FLASH_ENV_SECTOR_OFFSET_END = FLASH_INFO_SECTOR_OFFSET_START;

    m_FLASH_ENV_SECTOR_OFFSET_START =
        m_FLASH_ENV_SECTOR_OFFSET_END - m_FLASH_ENV_SECTOR_SIZE;

    if (m_FLASH_ENV_SECTOR_SIZE % FLASH_SECTOR_SIZE != 0)
    {
        println("Error: Flash env sector size is not a multiple of the "
                "sector size: " +
                String(buffer_size));
        return;
    }

    if (m_FLASH_ENV_SECTOR_OFFSET_START % FLASH_SECTOR_SIZE != 0)
    {
        println("Error: Flash env sector start position does not allign "
                "with flash "
                "sector boundaries: " +
                String(buffer_size));
        println(String((uint)m_FLASH_ENV_SECTOR_OFFSET_START));
        return;
    }

    // 2. Allocate a padded buffer to hold packed strings
    char* buffer = new char[buffer_size];
    copy_def_strings_to_buffer(buffer);

    // Flush the in-memory buffer to the flash
    write_to_flash((u_int8_t*)buffer, m_FLASH_ENV_SECTOR_OFFSET_START,
                   m_FLASH_ENV_SECTOR_SIZE, buffer_size);

    delete[] buffer;

    println("Wrote current variable definitions to flash.");

    // 5. Update the info sector with the new locations/sizes
    write_flash_info();
}

void uSEQ::load_flash_env()
{
    println("Loading previously saved state...");

    char* strings_start =
        (char*)(PICO_FLASH_START_ADDR + m_FLASH_ENV_SECTOR_OFFSET_START);

    char* read_ptr    = strings_start;
    size_t bytes_read = 0;
    // We read defs first, then swap to def_exprs
    ValueMap* map_ptr = &m_defs;

    while (bytes_read < m_FLASH_ENV_STRING_BUFFER_SIZE)
    {
        String name_str = String(read_ptr);
        read_ptr += name_str.length() + 1;

        String def_str = String(read_ptr);
        read_ptr += def_str.length() + 1;

        // Update bytes read (to see if we need to move
        // on to loading def exprs instead)
        bytes_read = (size_t)(read_ptr - strings_start);

        if (bytes_read > m_FLASH_ENV_DEFS_SIZE)
        {
            // Swap pointers
            map_ptr = &m_def_exprs;
        }

        Value val = parse(def_str);

        if (val.is_error())
        {
            println("Warning: Expression for " + name_str +
                    " could not be parsed (ignoring):\n" + "    " + def_str);
        }
        else
        {
            (*map_ptr)[name_str] = val;
        }
    }

    for (int i = 0; i < m_continuous_ASTs.size(); i++)
    {
        String name               = "a" + String(i + 1);
        std::optional<Value> expr = get_expr(name);
        if (expr)
        {
            m_continuous_ASTs[i] = *expr;
        }
        else
        {
            // println("Warning: Expression for output " + name +
            //         " was not found, ignoring...");
        }
    }

    for (int i = 0; i < m_binary_ASTs.size(); i++)
    {
        String name               = "d" + String(i + 1);
        std::optional<Value> expr = get_expr(name);
        if (expr)
        {
            m_binary_ASTs[i] = *expr;
        }
        else
        {
            // println("Warning: Expression for output " + name +
            //         " was not found, ignoring...");
        }
    }

    for (int i = 0; i < m_serial_ASTs.size(); i++)
    {
        String name               = "s" + String(i + 1);
        std::optional<Value> expr = get_expr(name);
        if (expr)
        {
            m_serial_ASTs[i] = *expr;
        }
        else
        {
            // println("Warning: Expression for output " + name +
            //         " was not found, ignoring...");
        }
    }

    println("Previously saved state loaded successfully.");
}

void uSEQ::autoload_flash()
{
    if (flash_has_been_written_before())
    {
        load_flash_info();

        if (m_FLASH_ENV_SECTOR_SIZE > 0 && m_FLASH_ENV_SECTOR_OFFSET_START > 0)
        {
            // println("All good, reading env...");
            load_flash_env();
        }
        else
        {
            println("Not ready to read env:");

            String s = "m_FLASH_ENV_SECTOR_SIZE: ";
            s += String((size_t)m_FLASH_ENV_SECTOR_SIZE);
            println(s);

            s = "m_FLASH_ENV_SECTOR_OFFSET_START: ";
            s += String((size_t)m_FLASH_ENV_SECTOR_OFFSET_START);
            println(s);
        }
    }
    else
    {
        // println("Flash NOT written before - ignoring.");
    }
}

// FIXME hangs
// void uSEQ::clear_non_program_flash()
// {
//     // NOTE: this was taken from the Arduino's IDE printout
//     // during compilation
//     constexpr uint32_t max_code_bytes = 1044480;

//     for (uint32_t addr = max_code_bytes; addr < PICO_FLASH_SIZE_BYTES;
//          addr += FLASH_SECTOR_SIZE)
//     {
//         if (addr % FLASH_SECTOR_SIZE != 0)
//         {
//             println("Address " + String(addr) +
//                     " is NOT aligned on sector borders.");
//         }
//         else
//         {
//             flash_range_erase(addr, FLASH_SECTOR_SIZE);
//         }
//     }

//     println("Cleared flash memory.");
// }

void uSEQ::reboot()
{
#define AIRCR_Register (*((volatile uint32_t*)(PPB_BASE + 0x0ED0C)))
    AIRCR_Register = 0x5FA0004;
}

void uSEQ::reset_flash_env_var_info()
{
    m_FLASH_ENV_SECTOR_SIZE         = 0;
    m_FLASH_ENV_DEFS_SIZE           = 0;
    m_FLASH_ENV_EXPRS_SIZE          = 0;
    m_FLASH_ENV_SECTOR_OFFSET_START = 0;
    m_FLASH_ENV_SECTOR_OFFSET_END   = 0;
    m_FLASH_ENV_STRING_BUFFER_SIZE  = 0;
    write_flash_info();
}