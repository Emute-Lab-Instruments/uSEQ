/******************************************************************************
fast_any - A faster alternative to boost:any / std::any
Copyright (c) 2024, Marcus Tomlinson

BSD 2-Clause License

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
******************************************************************************/

#pragma once

#include <memory>

namespace fast_any
{

using type_info = unsigned int;
inline type_info type_info_seq = 1;
template <typename T>
inline const type_info type_id = type_info_seq++;

class any final
{
public:
    any();
    ~any();

    any( const any& other );
    any( any&& other );

    template <typename T>
    explicit any( const T& value );

    template <typename T>
    explicit any( T&& value );

    any& operator=( const any& other );
    any& operator=( any&& other );

    template <typename T>
    any& operator=( const T& value );

    template <typename T>
    any& operator=( T&& value );

    bool has_value() const;

    template <typename T>
    T* as() const;

    void emplace( const any& other );
    void emplace( any& other );
    void emplace( any&& other );

    template <typename T>
    void emplace( const T& value );

    template <typename T>
    void emplace( T&& value );

    void swap( any& other );

    void reset();

    type_info type() const;

private:
    struct value_holder_t
    {
        value_holder_t( const value_holder_t& ) = delete;
        value_holder_t& operator=( const value_holder_t& ) = delete;

        inline value_holder_t() = default;
        inline virtual ~value_holder_t() = default;

        inline virtual value_holder_t* clone() const = 0;
        inline virtual void emplace( const value_holder_t* value_holder ) = 0;
    };

    template <typename T>
    struct value_t final : value_holder_t
    {
        value_t() = delete;
        value_t( const value_t& ) = delete;
        value_t& operator=( const value_t& ) = delete;

        inline ~value_t() override = default;

        inline explicit value_t( const T& value )
            : value( value )
        {
        }

        inline explicit value_t( T&& value )
            : value( std::forward<T>( value ) )
        {
        }

        inline value_holder_t* clone() const override
        {
            return new value_t( value );
        }

        inline void emplace( const value_holder_t* value_holder ) override
        {
            value = reinterpret_cast<const value_t<T>*>( value_holder )->value;
        }

        T value;
    };

    value_holder_t* _value_holder = nullptr;
    bool _has_value = false;
    type_info _type = 0;
};

inline any::any() = default;

inline any::~any()
{
    delete _value_holder;
}

inline any::any( const any& other )
{
    emplace( other );
}

inline any::any( any&& other )
{
    emplace( std::forward<any>( other ) );
}

template <typename T>
inline any::any( const T& value )
{
    emplace( value );
}

template <typename T>
inline any::any( T&& value )
{
    emplace( std::forward<T>( value ) );
}

inline any& any::operator=( const any& other )
{
    emplace( other );
    return *this;
}

inline any& any::operator=( any&& other )
{
    emplace( std::forward<any>( other ) );
    return *this;
}

template <typename T>
inline any& any::operator=( const T& value )
{
    emplace( value );
    return *this;
}

template <typename T>
inline any& any::operator=( T&& value )
{
    emplace( std::forward<T>( value ) );
    return *this;
}

inline bool any::has_value() const
{
    return _has_value;
}

template <typename T>
inline T* any::as() const
{
    if ( _has_value && _type == type_id<T> )
    {
        return &reinterpret_cast<value_t<T>*>( _value_holder )->value;
    }
    else
    {
        return nullptr;
    }
}

inline void any::emplace( const any& other )
{
    _has_value = other._has_value;

    if ( _has_value )
    {
        if ( _type == other._type )
        {
            _value_holder->emplace( other._value_holder );
        }
        else
        {
            delete _value_holder;
            _value_holder = other._value_holder->clone();
            _type = other._type;
        }
    }
}

inline void any::emplace( any& other )
{
    _has_value = other._has_value;

    if ( _has_value )
    {
        if ( _type == other._type )
        {
            _value_holder->emplace( other._value_holder );
        }
        else
        {
            delete _value_holder;
            _value_holder = other._value_holder->clone();
            _type = other._type;
        }
    }
}

inline void any::emplace( any&& other )
{
    swap( other );
}

template <typename T>
inline void any::emplace( const T& value )
{
    _has_value = true;

    if ( _type == type_id<T> )
    {
        reinterpret_cast<value_t<T>*>( _value_holder )->value = value;
    }
    else
    {
        delete _value_holder;
        _value_holder = new value_t<T>( value );
        _type = type_id<T>;
    }
}

template <typename T>
inline void any::emplace( T&& value )
{
    _has_value = true;

    if ( _type == type_id<T> )
    {
        reinterpret_cast<value_t<T>*>( _value_holder )->value = std::forward<T>( value );
    }
    else
    {
        delete _value_holder;
        _value_holder = new value_t<T>( std::forward<T>( value ) );
        _type = type_id<T>;
    }
}

inline void any::swap( any& other )
{
    std::swap( other._value_holder, _value_holder );
    std::swap( other._has_value, _has_value );
    std::swap( other._type, _type );
}

inline void any::reset()
{
    _has_value = false;
}

inline type_info any::type() const
{
    if ( _has_value )
    {
        return _type;
    }
    else
    {
        return 0;
    }
}

}  // namespace fast_any
