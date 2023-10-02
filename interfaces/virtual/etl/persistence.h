///\file

/******************************************************************************
The MIT License(MIT)

Embedded Template Library.
https://github.com/ETLCPP/etl
https://www.etlcpp.com

Copyright(c) 2021 jwellbelove

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

#ifndef ETL_PERSISTENCE_INCLUDED
#define ETL_PERSISTENCE_INCLUDED

#include "platform.h"
#include "memory.h"

namespace etl
{
  //***************************************************************************
  /// Persistence interface.
  //***************************************************************************
  class ipersistence
  {
  public:

    virtual void start() = 0;

    virtual void save(const char* data, size_t length) = 0;
    virtual void restore(char* data, size_t length) = 0;
  };

  //***************************************************************************
  /// Generic Save Persistent.
  //***************************************************************************
  template <typename T>
#if ETL_CPP11_SUPPORTED
  void save_persistent(T&& value, etl::ipersistence& persistence)
#else
  void save_persistent(const T& value, etl::ipersistence& persistence)
#endif  
  {
    const char* pvalue = reinterpret_cast<const char*>(&value);
    size_t      length = sizeof(T);
    persistence.save(pvalue, length);
  }

  //***************************************************************************
  /// Generic Restore Persistent.
  //***************************************************************************
  template <typename T>
#if ETL_CPP11_SUPPORTED
  T&& restore_persistent(etl::ipersistence& persistence)
#else
  T restore_persistent(etl::ipersistence& persistence)
#endif    
  {
    etl::uninitialized_buffer_of<T, 1U> buffer;
    size_t length = sizeof(T);
    persistence.restore(reinterpret_cast<char*>(buffer.begin()), length);

    return static_cast<T>(buffer);
  }
}

#endif
