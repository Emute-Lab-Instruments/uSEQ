///\file

/******************************************************************************
The MIT License(MIT)

Copyright (c) 2020 Dr. Alexander Bulovyatov
https://github.com/mrshurik/poly_span

Modifications:
Copyright(c) 2022 John Wellbelove

Embedded Template Library.
https://github.com/ETLCPP/etl
https://www.etlcpp.com

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

#ifndef ETL_POLY_SPAN_INCLUDED
#define ETL_POLY_SPAN_INCLUDED

#include "platform.h"
#include "type_traits.h"
#include "iterator.h"
#include "exception.h"
#include "error_handler.h"

#include "private/dynamic_extent.h"

#if ETL_CPP11_SUPPORTED
namespace etl
{
  //***************************************************************************
  /// The base class for poly_span exceptions.
  //***************************************************************************
  class poly_span_exception : public exception
  {
  public:

    poly_span_exception(string_type reason_, string_type file_name_, numeric_type line_number_)
      : exception(reason_, file_name_, line_number_)
    {
    }
  };

  //***************************************************************************
  /// The out of range exceptions.
  //***************************************************************************
  class poly_span_out_of_range : public poly_span_exception
  {
  public:

    poly_span_out_of_range(string_type file_name_, numeric_type line_number_)
      : poly_span_exception("poly_span:out of range", file_name_, line_number_)
    {
    }
  };

  namespace private_poly_span
  {
    //***************************************************************************
    template <typename TType>
    using has_size = etl::is_integral<decltype(etl::declval<TType&>().size())>;

    //***************************************************************************
    template <typename TType>
    using data_type = decltype(etl::declval<TType&>().data());

    //***************************************************************************
    template <typename TType>
    using has_data = etl::is_pointer<data_type<TType>>;

    //***************************************************************************
    template <typename TFrom, typename TType>
    using has_convertible_data = etl::is_convertible<data_type<TFrom>, TType*>;

    //***************************************************************************
    template <typename TFrom, typename TTo>
    using transfer_const = typename etl::conditional<etl::is_const<TFrom>::value,
                                                     typename etl::add_const<TTo>::type,
                                                     TTo>::type;

    //***************************************************************************
    template <typename TType>
    class poly_span_iterator;

    //***************************************************************************
    template <typename TIterator>
    struct is_poly_span_itr_impl : etl::false_type {};

    //***************************************************************************
    template <typename TType>
    struct is_poly_span_itr_impl<poly_span_iterator<TType>> : etl::true_type {};

    //***************************************************************************
    template <typename TIterator>
    using is_poly_span_iterator = is_poly_span_itr_impl<TIterator>;

    //***************************************************************************
    /// Iterator implementation
    //***************************************************************************
    template <typename TType>
    class poly_span_iterator : public etl::iterator<ETL_OR_STD::random_access_iterator_tag, TType> 
    {
    private:

      template <typename TOther>
      using enable_if_conversion_to_const = etl::enable_if_t<etl::is_const<TType>::value && etl::is_same<TType, const TOther>::value>;

    public:

      //***************************************************************************
      poly_span_iterator() = default;
      poly_span_iterator(const poly_span_iterator&) = default;
      poly_span_iterator& operator=(const poly_span_iterator&) = default;

      //***************************************************************************
      poly_span_iterator(TType* ptr, size_t element_size) noexcept
        : m_ptr(ptr), m_element_size(element_size) 
      {
      }

      //***************************************************************************
      template <typename TMutableType, typename = enable_if_conversion_to_const<TMutableType>>
      poly_span_iterator(const poly_span_iterator<TMutableType>& other) noexcept
        : m_ptr(other.m_ptr)
        , m_element_size(other.m_element_size) 
      {
      }

      //***************************************************************************
      TType* operator ->() const noexcept 
      { 
        return m_ptr; 
      }
      
      //***************************************************************************
      TType& operator *() const noexcept 
      { 
        return *m_ptr; 
      }

      //***************************************************************************
      poly_span_iterator& operator ++() noexcept 
      {
        from_char_ptr(to_char_ptr() + m_element_size);
        return *this;
      }

      //***************************************************************************
      poly_span_iterator& operator --() noexcept 
      {
        from_char_ptr(to_char_ptr() - m_element_size);
        return *this;
      }

      //***************************************************************************
      poly_span_iterator& operator +=(ptrdiff_t n) noexcept 
      {
        from_char_ptr(to_char_ptr() + (n * m_element_size));
        return *this;
      }

      //***************************************************************************
      friend ptrdiff_t operator -(poly_span_iterator left, poly_span_iterator right) noexcept 
      {
        return (left.to_char_ptr() - right.to_char_ptr()) / left.m_element_size;
      }

      //***************************************************************************
      poly_span_iterator operator +(ptrdiff_t n) const noexcept 
      {
        poly_span_iterator tmp = *this;
        tmp += n;
        return tmp;
      }

      //***************************************************************************
      poly_span_iterator operator ++(int) noexcept 
      {
        poly_span_iterator tmp = *this;
        ++(*this);
        return tmp;
      }

      //***************************************************************************
      poly_span_iterator operator --(int) noexcept 
      {
        poly_span_iterator tmp = *this;
        --(*this);
        return tmp;
      }

      //***************************************************************************
      poly_span_iterator operator-(ptrdiff_t n) const noexcept 
      { 
        return *this + (-n);
      }

      //***************************************************************************
      poly_span_iterator& operator-=(ptrdiff_t n) noexcept 
      { 
        return *this += -n; 
      }

      //***************************************************************************
      TType& operator[](ptrdiff_t n) const noexcept 
      { 
        return *(*this + n); 
      }

      //***************************************************************************
      friend poly_span_iterator operator+(ptrdiff_t n, poly_span_iterator other) noexcept 
      {
        return other + n;
      }

      //***************************************************************************
      friend bool operator==(poly_span_iterator left, poly_span_iterator right) noexcept 
      { 
        return left.m_ptr == right.m_ptr; 
      }
      
      //***************************************************************************
      friend bool operator!=(poly_span_iterator left, poly_span_iterator right) noexcept 
      { 
        return left.m_ptr != right.m_ptr; 
      }

      //***************************************************************************
      friend bool operator >(poly_span_iterator left, poly_span_iterator right) noexcept 
      { 
        return left.m_ptr > right.m_ptr; 
      }
      
      //***************************************************************************
      friend bool operator <(poly_span_iterator left, poly_span_iterator right) noexcept 
      { 
        return left.m_ptr < right.m_ptr; 
      }
      
      //***************************************************************************
      friend bool operator>=(poly_span_iterator left, poly_span_iterator right) noexcept 
      { 
        return left.m_ptr >= right.m_ptr; 
      }

      //***************************************************************************
      friend bool operator<=(poly_span_iterator left, poly_span_iterator right) noexcept 
      { 
        return left.m_ptr <= right.m_ptr; 
      }

    private:

      template <typename>
      friend class poly_span_iterator;

      using void_ptr_t = transfer_const<TType, void>*;
      using char_ptr_t = transfer_const<TType, char>*;

      //***************************************************************************
      char_ptr_t to_char_ptr() const noexcept 
      { 
        return static_cast<char_ptr_t>(static_cast<void_ptr_t>(m_ptr)); 
      }
      
      //***************************************************************************
      void from_char_ptr(char_ptr_t ptr) noexcept 
      { 
        m_ptr = static_cast<TType*>(static_cast<void_ptr_t>(ptr)); 
      }

      TType* m_ptr;
      size_t m_element_size;
    };
  }

  //***************************************************************************
  /// C++11 etl::span-like view with focus on polymorphic aspect of types.
  /// Provides a base class / interface view. Type can be const or non-const.
  /// The class behaves like etl::span, has O(1) index access and provides RandomAccess iterators.
  /// Hints:
  /// 1. NEVER assign to elements or apply mutable algorithms to a mutable view to a base class. Object slicing!
  /// 2. Pay attention to lifetime of the source buffer. poly_span should mostly be used as an argument of functions.
  /// 3. The view has no data() for reason. Pointer to the first byte of the first object is unknown in general.
  //***************************************************************************
  template <typename TType, size_t Extent = etl::dynamic_extent>
  class poly_span 
  {
  private:

    //***************************************************************************
    template <typename Object>
    using EnableIfConvertibleFromObject = etl::enable_if_t<private_poly_span::has_size<Object>::value && 
                                                           private_poly_span::has_data<Object>::value &&
                                                           private_poly_span::has_convertible_data<Object, TType>::value>;

    //***************************************************************************
    template <typename OtherType>
    using EnableIfTConvertibleType = etl::enable_if_t<etl::is_convertible<OtherType*, TType*>::value>;

    //***************************************************************************
    template <typename Iterator>
    using is_contiguous_iterator = etl::integral_constant<bool,
                                                          etl::is_base_of<ETL_OR_STD::random_access_iterator_tag, typename ETL_OR_STD::iterator_traits<Iterator>::iterator_category>::value &&
                                                          !private_poly_span::is_poly_span_iterator<Iterator>::value>;

  public:

    using element_type           = TType;
    using value_type             = typename etl::remove_cv<TType>::type;
    using size_type              = size_t;
    using difference_type        = ptrdiff_t;
    using pointer                = TType*;
    using reference              = TType&;
    using const_pointer          = const TType*;
    using const_reference        = const TType&;
    using iterator               = private_poly_span::poly_span_iterator<TType>;
    using const_iterator         = private_poly_span::poly_span_iterator<const TType>;
    using reverse_iterator       = etl::reverse_iterator<iterator>;
    using const_reverse_iterator = etl::reverse_iterator<const_iterator>;

    static constexpr size_t npos = ~size_t(0);

    //***************************************************************************
    /// Default constructor
    //***************************************************************************
    poly_span() = default;

    //***************************************************************************
    /// Copy constructor
    //***************************************************************************
    poly_span(const poly_span&) = default;
    
    //***************************************************************************
    /// Assignment operator
    //***************************************************************************
    poly_span& operator=(const poly_span&) = default;

    //***************************************************************************
    /// Construct from a compatible poly_span: convert mutable to const, derived to base type.
    //***************************************************************************
    template <typename TConvertibleType>
    poly_span(const poly_span<TConvertibleType>& span) noexcept
      : poly_span(span.m_dataPtr, span.m_size, span.m_element_size) 
    {
    }

    //***************************************************************************
    /// Construct from a range of iterators or pointers. 
    /// Iterators must Contiguous.
    //***************************************************************************
    template <typename TContiguousIterator>
    poly_span(TContiguousIterator begin, TContiguousIterator end) noexcept
      : poly_span(&*begin, end - begin, ETL_NULLPTR) 
    {
      static_assert(is_contiguous_iterator<TContiguousIterator>::value, "Not a contiguous iterator");
    }

    //***************************************************************************
    /// Construct from a range defined as iterator/pointer to begin and size. 
    /// Iterators must Contiguous.
    //***************************************************************************
    template <typename TContiguousIterator>
    poly_span(TContiguousIterator begin, size_t size) noexcept
      : poly_span(&*begin, size, ETL_NULLPTR) 
    {
      static_assert(is_contiguous_iterator<TContiguousIterator>::value, "Not a contiguous iterator");
    }

    //***************************************************************************
    /// Construct from a C-style array.
    //***************************************************************************
    template <typename TConvertibleType, size_t N>
    poly_span(TConvertibleType(&array)[N]) noexcept
      : poly_span(array, N, ETL_NULLPTR) 
    {
    }

    //***************************************************************************
    /// Construct from a buffer objects: etl::vector, etl::array, etl::span or any class with data() and size().
    //***************************************************************************
    template <typename TBufferLikeObj, typename = EnableIfConvertibleFromObject<TBufferLikeObj>>
    poly_span(TBufferLikeObj& buffer) noexcept
      : poly_span(buffer.data()
      , buffer.size()
      , ETL_NULLPTR) 
    {
    }

    //***************************************************************************
    /// Get the beginning of the span
    //***************************************************************************
    iterator begin() const noexcept 
    { 
      return { m_dataPtr, m_element_size }; 
    }
    
    //***************************************************************************
    /// Get the end of the span
    //***************************************************************************
    iterator end() const noexcept 
    { 
      return { element_at(m_size), m_element_size }; 
    }
    
    //***************************************************************************
    /// Get the beginning of the span
    //***************************************************************************
    const_iterator cbegin() const noexcept 
    { 
      return begin(); 
    }
    
    //***************************************************************************
    /// Get the end of the span
    //***************************************************************************
    const_iterator cend() const noexcept 
    { 
      return end(); 
    }
    
    //***************************************************************************
    /// Get the reverse beginning of the span
    //***************************************************************************
    reverse_iterator rbegin() const noexcept 
    { 
      return reverse_iterator(end()); 
    }

    //***************************************************************************
    /// Get the end of the span
    //***************************************************************************
    reverse_iterator rend() const noexcept 
    { 
      return reverse_iterator(begin()); 
    }

    //***************************************************************************
    /// Get the reverse beginning of the span
    //***************************************************************************
    const_reverse_iterator crbegin() const noexcept 
    { 
      return rbegin(); 
    }
    
    //***************************************************************************
    /// Get the end of the span
    //***************************************************************************
    const_reverse_iterator crend() const noexcept 
    { 
      return rend(); 
    }

    //***************************************************************************
    /// Returns true if the view has no elements.
    //***************************************************************************
    bool empty() const noexcept 
    { 
      return m_size == 0; 
    }

    //***************************************************************************
    /// Number of elements in the view.
    //***************************************************************************
    size_t size() const noexcept 
    { 
      return m_size; 
    }

    //***************************************************************************
    /// sizeof(OriginalType) where OriginalType is type of the source buffer.
    /// It's not sizeof(TType) for poly_span<TType>!
    //***************************************************************************
    size_t element_size() const noexcept 
    { 
      return m_element_size; 
    }

    //***************************************************************************
    /// Checks that other poly_span provides view to the same source buffer.
    /// It's not like etl::span operator==() which checks for elements equality.
    //***************************************************************************
    bool equal_view(const poly_span<const TType>& other) const noexcept 
    {
      return (m_dataPtr == other.m_dataPtr) &&
             (m_size == other.m_size) && 
             (m_element_size == other.m_element_size);
    }

    //***************************************************************************
    /// Reference to the element with given index. No out of bounds check.
    //***************************************************************************
    reference operator[](size_t index) const noexcept
    { 
      return *element_at(index); 
    }

    //***************************************************************************
    /// Index access with out of bounds checks.
    /// \param index position of element in the view.
    /// Asserts etl::poly_span_out_of_range if index >= size().
    /// \return reference to the element.
    //***************************************************************************
    reference at(size_t index) const 
    {
      ETL_ASSERT(index < m_size, ETL_ERROR(etl::poly_span_out_of_range));

      return *element_at(index);
    }

    //***************************************************************************
    /// Reference to the first element. The view must be non-empty.
    //***************************************************************************
    reference front() const noexcept 
    { 
      return *m_dataPtr; 
    }

    //***************************************************************************
    /// Reference to the last element. The view must be non-empty.
    //***************************************************************************
    reference back() const noexcept 
    { 
      return *element_at(m_size - 1); 
    }

    //***************************************************************************
    /// Creates a new view as a subrange of this.
    /// \param pos Starting offset for the new view.
    /// \param count Number of elements to take into the new view. Default npos is to take all until the end.
    /// Asserts etl::poly_span_out_of_range if pos > size().
    /// \return New sub-view. If count > number of elements before the end it's the same as npos, no error.
    //***************************************************************************
    poly_span subspan(size_t pos, size_t count = npos) const 
    {
      ETL_ASSERT(pos <= m_size, ETL_ERROR(etl::poly_span_out_of_range));

      return { element_at(pos), min_impl(m_size - pos, count), m_element_size };
    }

    //***************************************************************************
    /// 
    //***************************************************************************
    template <const size_t OFFSET, size_t COUNT = etl::dynamic_extent>
    constexpr etl::poly_span<element_type, (COUNT != etl::dynamic_extent ? COUNT : (Extent != etl::dynamic_extent ? Extent - OFFSET : etl::dynamic_extent))> subspan() const
    {
      if (COUNT == etl::dynamic_extent)
      {
        return etl::poly_span<element_type, (COUNT != etl::dynamic_extent ? COUNT : (Extent != etl::dynamic_extent ? Extent - OFFSET : etl::dynamic_extent))>(m_dataPtr + (OFFSET * element_size), mend);
      }
      else
      {
        return etl::poly_span<element_type, (COUNT != etl::dynamic_extent ? COUNT : (Extent != etl::dynamic_extent ? Extent - OFFSET : etl::dynamic_extent))>(m_dataPtr + (OFFSET * element_size), m_dataPtr + ((OFFSET + COUNT) * element_size) + COUNT);
      }
    }

    //***************************************************************************
    /// Creates a new view from exactly 'CCount' first elements.
    /// \param count Exact number of elements to take.
    /// \return New sub-view.
    //***************************************************************************
    template <size_t CCount>
    constexpr etl::poly_span<element_type, CCount> first() const
    {
      return etl::poly_span<element_type, CCount>(m_dataPtr, CCount, m_element_size);
    }

    //***************************************************************************
    /// Creates a new view from exactly 'count' first elements.
    /// \param count Exact number of elements to take.
    /// \return New sub-view.
    //***************************************************************************
    constexpr etl::poly_span<element_type, etl::dynamic_extent> first(size_t count) const
    {
      return etl::poly_span<element_type, etl::dynamic_extent>(m_dataPtr, count, m_element_size);
    }

    //***************************************************************************
    /// Creates a new view from exactly 'CCount' last elements.
    /// \param count Exact number of elements to take.
    /// \return New sub-view.
    //***************************************************************************
    template <size_t CCount>
    constexpr etl::poly_span<element_type, CCount> last() const
    {
      return etl::poly_span<element_type, CCount>(element_at(m_size - CCount), CCount, m_element_size);
    }

    //***************************************************************************
    /// Creates a new view from exactly 'count' last elements.
    /// \param count Exact number of elements to take.
    /// \return New sub-view.
    //***************************************************************************
    constexpr poly_span last(size_t count) const
    {
      return etl::poly_span<element_type, etl::dynamic_extent>(element_at(m_size - count), count, m_element_size);
    }

  private:
      
    //***************************************************************************
    poly_span(TType* dataPtr, size_t size, size_t element_size) noexcept
      : m_dataPtr(dataPtr)
      , m_size(size)
      , m_element_size(element_size) 
    {
    }

#if ETL_USING_STL
    //***************************************************************************
    template <typename TConvertibleType>
    poly_span(TConvertibleType* dataPtr, size_t size, std::nullptr_t) noexcept
      : m_dataPtr(dataPtr)
      , m_size(size)
      , m_element_size(sizeof(TConvertibleType))
    {
    }
#else
    //***************************************************************************
    template <typename TConvertibleType>
    poly_span(TConvertibleType* dataPtr, size_t size, void*) noexcept
      : m_dataPtr(dataPtr)
      , m_size(size)
      , m_element_size(sizeof(TConvertibleType))
    {
    }
#endif

    template <typename TType, size_t Extent>
    friend class poly_span;

    using void_ptr_t = private_poly_span::transfer_const<TType, void>*;
    using char_ptr_t = private_poly_span::transfer_const<TType, char>*;

    //***************************************************************************
    TType* element_at(size_t ind) const noexcept 
    {
      const auto base = static_cast<char_ptr_t>(static_cast<void_ptr_t>(m_dataPtr));
      return static_cast<TType*>(static_cast<void_ptr_t>(base + (ind * m_element_size)));
    }

    //***************************************************************************
    template <typename Value>
    static Value min_impl(Value a, Value b) noexcept 
    { 
      return (a < b) ? a : b; 
    }

    TType* m_dataPtr      = ETL_NULLPTR;
    size_t m_size         = 0;
    size_t m_element_size = 0;
  };

  //***************************************************************************
  /// Specialisation for etl::dynamic_extent
  //***************************************************************************
  template <typename TType>
  class poly_span< TType, etl::dynamic_extent>
  {
  private:

    //***************************************************************************
    template <typename Object>
    using EnableIfConvertibleFromObject = etl::enable_if_t<private_poly_span::has_size<Object>::value&&
                                                           private_poly_span::has_data<Object>::value&&
                                                           private_poly_span::has_convertible_data<Object, TType>::value>;

    //***************************************************************************
    template <typename OtherType>
    using EnableIfTConvertibleType = etl::enable_if_t<etl::is_convertible<OtherType*, TType*>::value>;

    //***************************************************************************
    template <typename Iterator>
    using is_contiguous_iterator = etl::integral_constant<bool,
                                                         etl::is_base_of<ETL_OR_STD::random_access_iterator_tag, typename ETL_OR_STD::iterator_traits<Iterator>::iterator_category>::value &&
                                                         !private_poly_span::is_poly_span_iterator<Iterator>::value>;

  public:

    using element_type           = TType;
    using value_type             = typename etl::remove_cv<TType>::type;
    using size_type              = size_t;
    using difference_type        = ptrdiff_t;
    using pointer                = TType*;
    using reference              = TType&;
    using const_pointer          = const TType*;
    using const_reference        = const TType&;
    using iterator               = private_poly_span::poly_span_iterator<TType>;
    using const_iterator         = private_poly_span::poly_span_iterator<const TType>;
    using reverse_iterator       = etl::reverse_iterator<iterator>;
    using const_reverse_iterator = etl::reverse_iterator<const_iterator>;

    static constexpr size_t npos = ~size_t(0);

    //***************************************************************************
    /// Default constructor
    //***************************************************************************
    poly_span() = default;

    //***************************************************************************
    /// Copy constructor
    //***************************************************************************
    poly_span(const poly_span&) = default;

    //***************************************************************************
    /// Assignment operator
    //***************************************************************************
    poly_span& operator=(const poly_span&) = default;

    //***************************************************************************
    /// Construct from a compatible poly_span: convert mutable to const, derived to base type.
    //***************************************************************************
    template <typename TConvertibleType>
    poly_span(const poly_span<TConvertibleType>& span) noexcept
      : poly_span(span.m_dataPtr, span.m_size, span.m_element_size)
    {
    }

    //***************************************************************************
    /// Construct from a range of iterators or pointers. 
    /// Iterators must Contiguous.
    //***************************************************************************
    template <typename TContiguousIterator>
    poly_span(TContiguousIterator begin, TContiguousIterator end) noexcept
      : poly_span(&*begin, end - begin, ETL_NULLPTR)
    {
      static_assert(is_contiguous_iterator<TContiguousIterator>::value, "Not a contiguous iterator");
    }

    //***************************************************************************
    /// Construct from a range defined as iterator/pointer to begin and size. 
    /// Iterators must Contiguous.
    //***************************************************************************
    template <typename TContiguousIterator>
    poly_span(TContiguousIterator begin, size_t size) noexcept
      : poly_span(&*begin, size, ETL_NULLPTR)
    {
      static_assert(is_contiguous_iterator<TContiguousIterator>::value, "Not a contiguous iterator");
    }

    //***************************************************************************
    /// Construct from a C-style array.
    //***************************************************************************
    template <typename TConvertibleType, size_t N>
    poly_span(TConvertibleType(&array)[N]) noexcept
      : poly_span(array, N, ETL_NULLPTR)
    {
    }

    //***************************************************************************
    /// Construct from a buffer objects: etl::vector, etl::array, etl::span or any class with data() and size().
    //***************************************************************************
    template <typename TBufferLikeObj, typename = EnableIfConvertibleFromObject<TBufferLikeObj>>
    poly_span(TBufferLikeObj& buffer) noexcept
      : poly_span(buffer.data()
        , buffer.size()
        , ETL_NULLPTR)
    {
    }

    //***************************************************************************
    /// Get the beginning of the span
    //***************************************************************************
    iterator begin() const noexcept
    {
      return { m_dataPtr, m_element_size };
    }

    //***************************************************************************
    /// Get the end of the span
    //***************************************************************************
    iterator end() const noexcept
    {
      return { element_at(m_size), m_element_size };
    }

    //***************************************************************************
    /// Get the beginning of the span
    //***************************************************************************
    const_iterator cbegin() const noexcept
    {
      return begin();
    }

    //***************************************************************************
    /// Get the end of the span
    //***************************************************************************
    const_iterator cend() const noexcept
    {
      return end();
    }

    //***************************************************************************
    /// Get the reverse beginning of the span
    //***************************************************************************
    reverse_iterator rbegin() const noexcept
    {
      return reverse_iterator(end());
    }

    //***************************************************************************
    /// Get the end of the span
    //***************************************************************************
    reverse_iterator rend() const noexcept
    {
      return reverse_iterator(begin());
    }

    //***************************************************************************
    /// Get the reverse beginning of the span
    //***************************************************************************
    const_reverse_iterator crbegin() const noexcept
    {
      return rbegin();
    }

    //***************************************************************************
    /// Get the end of the span
    //***************************************************************************
    const_reverse_iterator crend() const noexcept
    {
      return rend();
    }

    //***************************************************************************
    /// Returns true if the view has no elements.
    //***************************************************************************
    bool empty() const noexcept
    {
      return m_size == 0;
    }

    //***************************************************************************
    /// Number of elements in the view.
    //***************************************************************************
    size_t size() const noexcept
    {
      return m_size;
    }

    //***************************************************************************
    /// sizeof(OriginalType) where OriginalType is type of the source buffer.
    /// It's not sizeof(TType) for poly_span<TType>!
    //***************************************************************************
    size_t element_size() const noexcept
    {
      return m_element_size;
    }

    //***************************************************************************
    /// Checks that other poly_span provides view to the same source buffer.
    /// It's not like etl::span operator==() which checks for elements equality.
    //***************************************************************************
    bool equal_view(const poly_span<const TType>& other) const noexcept
    {
      return (m_dataPtr == other.m_dataPtr) &&
             (m_size == other.m_size) &&
             (m_element_size == other.m_element_size);
    }

    //***************************************************************************
    /// Reference to the element with given index. No out of bounds check.
    //***************************************************************************
    reference operator[](size_t index) const noexcept
    {
      return *element_at(index);
    }

    //***************************************************************************
    /// Index access with out of bounds checks.
    /// \param index position of element in the view.
    /// Asserts etl::poly_span_out_of_range if index >= size().
    /// \return reference to the element.
    //***************************************************************************
    reference at(size_t index) const
    {
      ETL_ASSERT(index < m_size, ETL_ERROR(etl::poly_span_out_of_range));

      return *element_at(index);
    }

    //***************************************************************************
    /// Reference to the first element. The view must be non-empty.
    //***************************************************************************
    reference front() const noexcept
    {
      return *m_dataPtr;
    }

    //***************************************************************************
    /// Reference to the last element. The view must be non-empty.
    //***************************************************************************
    reference back() const noexcept
    {
      return *element_at(m_size - 1);
    }

    //***************************************************************************
    /// Creates a new view as a subrange of this.
    /// \param pos Starting offset for the new view.
    /// \param count Number of elements to take into the new view. Default npos is to take all until the end.
    /// Asserts etl::poly_span_out_of_range if pos > size().
    /// \return New sub-view. If count > number of elements before the end it's the same as npos, no error.
    //***************************************************************************
    poly_span subspan(size_t pos, size_t count = npos) const
    {
      ETL_ASSERT(pos <= m_size, ETL_ERROR(etl::poly_span_out_of_range));

      return { element_at(pos), min_impl(m_size - pos, count), m_element_size };
    }

    //***************************************************************************
    /// Creates a new view from exactly N first elements.
    /// \param count Exact number of elements to take.
    /// Asserts etl::poly_span_out_of_range if count > size().
    /// \return New sub-view.
    //***************************************************************************
    poly_span first(size_t count) const
    {
      ETL_ASSERT(count <= m_size, ETL_ERROR(etl::poly_span_out_of_range));

      return { m_dataPtr, count, m_element_size };
    }

    //***************************************************************************
    /// Creates a new view from exactly N last elements.
    /// \param count Exact number of elements to take.
    /// Asserts etl::poly_span_out_of_range if count > size().
    /// \return New sub-view.
    //***************************************************************************
    poly_span last(size_t count) const
    {
      ETL_ASSERT(count <= m_size, ETL_ERROR(etl::poly_span_out_of_range));

      return { element_at(m_size - count), count, m_element_size };
    }

  private:

    //***************************************************************************
    poly_span(TType* dataPtr, size_t size, size_t element_size) noexcept
      : m_dataPtr(dataPtr)
      , m_size(size)
      , m_element_size(element_size)
    {
    }

#if ETL_USING_STL
    //***************************************************************************
    template <typename TConvertibleType>
    poly_span(TConvertibleType* dataPtr, size_t size, std::nullptr_t) noexcept
      : m_dataPtr(dataPtr)
      , m_size(size)
      , m_element_size(sizeof(TConvertibleType))
    {
    }
#else
    //***************************************************************************
    template <typename TConvertibleType>
    poly_span(TConvertibleType* dataPtr, size_t size, void*) noexcept
      : m_dataPtr(dataPtr)
      , m_size(size)
      , m_element_size(sizeof(TConvertibleType))
    {
    }
#endif

    template <typename TType, size_t Extent>
    friend class poly_span;

    using void_ptr_t = private_poly_span::transfer_const<TType, void>*;
    using char_ptr_t = private_poly_span::transfer_const<TType, char>*;

    //***************************************************************************
    TType* element_at(size_t ind) const noexcept
    {
      const auto base = static_cast<char_ptr_t>(static_cast<void_ptr_t>(m_dataPtr));
      return static_cast<TType*>(static_cast<void_ptr_t>(base + (ind * m_element_size)));
    }

    //***************************************************************************
    template <typename Value>
    static Value min_impl(Value a, Value b) noexcept
    {
      return (a < b) ? a : b;
    }

    TType* m_dataPtr      = ETL_NULLPTR;
    size_t m_size         = 0;
    size_t m_element_size = 0;
  };
}

#endif

#endif
