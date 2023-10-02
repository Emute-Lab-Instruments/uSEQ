/******************************************************************************
The MIT License(MIT)

Embedded Template Library.
https://github.com/ETLCPP/etl
https://www.etlcpp.com

Copyright(c) 2022 John Wellbelove, Aron P. Müller

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files(the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
******************************************************************************/

#ifndef ETL_MUTEX_CMSIS_RTOS1_RTX_INCLUDED
#define ETL_MUTEX_CMSIS_RTOS1_RTX_INCLUDED

#include "../platform.h"

#include <cmsis_os.h>

namespace etl
{
  //***************************************************************************
  ///\ingroup mutex
  ///\brief This mutex class is implemented using CMSIS's RTOS1 (RTX) mutexes
  //***************************************************************************
  class mutex
  {
  public:

    mutex()
      : os_mutex_cb_etl()
      , id(NULL)
    {
      const osMutexDef_t os_mutex_def_etl = { os_mutex_cb_etl };
      id = osMutexCreate(osMutex(etl));
    }

    ~mutex()
    {
      osMutexDelete(id);
    }

    void lock()
    {
      osMutexWait(id, osWaitForever);
    }

    bool try_lock()
    {
      return osMutexWait(id, 0) == osOK;
    }

    void unlock()
    {
      osMutexRelease(id);
    }

  private:

    mutex(const mutex&) ETL_DELETE;
    mutex& operator=(const mutex&) ETL_DELETE;

    /// RTX specific
    uint32_t os_mutex_cb_etl[4];
    osMutexId id;
  };
}

#endif
