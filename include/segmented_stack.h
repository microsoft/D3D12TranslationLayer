// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#pragma once

struct noop_unary
{
    void* operator()(bool) noexcept { return nullptr; }
};

//==================================================================================================================================
//
// segmented_stack
//
//==================================================================================================================================
template< class T, size_t segment_size = 256, typename unary = noop_unary >
class segmented_stack
{
public:
    typedef T value_type;
    typedef T* pointer;
    typedef const T* const_pointer;
    typedef T& reference;
    typedef const T& const_reference;
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;

    explicit segmented_stack(const std::nothrow_t&, const unary& notifier = unary());
    explicit segmented_stack(const unary& notifier = unary()); // throw( bad_alloc )
    void free();
    segmented_stack& operator=(segmented_stack&&);
    segmented_stack(segmented_stack&&);
    ~segmented_stack();
    void set_notifier(const unary&);
    size_type max_size() const;
    size_type size() const;
    size_type capacity() const;
    void reserve(size_type); // throw( bad_alloc )
    void push(const value_type&); // throw( ... )
    bool reserve_contiguous(size_type) noexcept;
    pointer append_contiguous_manually() noexcept;
    pointer append_contiguous_manually(size_type) noexcept;
    void swap(segmented_stack< T, segment_size, unary >&);
    void clear();
    bool empty();

    class segment_range
    {
    protected:
        friend class segmented_stack;
        explicit segment_range(void* p) :
            m_begin(static_cast< pointer >(p)),
            m_end(static_cast< pointer >(p))
        { }

        pointer m_begin;
        pointer m_end;

    public:
        typedef pointer iterator;
        iterator begin();
        iterator end();
        typedef const_pointer const_iterator;
        const_iterator begin() const;
        const_iterator end() const;
    };

    typedef std::vector< segment_range > segment_vector;
    typedef typename segment_vector::iterator segment_iterator;
    segment_iterator segments_begin();
    segment_iterator segments_end();
    typedef typename segment_vector::const_iterator const_segment_iterator;
    const_segment_iterator segments_begin() const;
    const_segment_iterator segments_end() const;

    void reserve_additional_segment(); // throw( bad_alloc )
    bool reserve_contiguous_alloc(size_type) noexcept;

    segment_vector m_segments;
    typename segment_vector::iterator m_last_segment;
    unary m_notifier;
};

//----------------------------------------------------------------------------------------------------------------------------------
template< class T, size_t segment_size, typename unary >
inline segmented_stack< T, segment_size, unary >::segmented_stack(const std::nothrow_t&, const unary& notifier) :
    m_notifier(notifier)
{
    // Usage of this constructor is dangerous, as m_last_segment doesn't point to anything allocated, while push assumes it does.
    m_last_segment = m_segments.begin();
}

//----------------------------------------------------------------------------------------------------------------------------------
template< class T, size_t segment_size, typename unary >
inline segmented_stack< T, segment_size, unary >::segmented_stack(const unary& notifier) : // throw( bad_alloc )
    m_notifier(notifier)
{
    m_segments.reserve(8); // throw( bad_alloc )
    m_last_segment = m_segments.begin();
    reserve_additional_segment(); // throw( bad_alloc )
    m_last_segment = m_segments.begin();
}

//----------------------------------------------------------------------------------------------------------------------------------
template< class T, size_t segment_size, typename unary >
inline void segmented_stack< T, segment_size, unary >::free()
{
    for (segment_vector::iterator segment = m_segments.begin(); segment != m_segments.end(); ++segment)
    {
        for (segment_range::iterator it = segment->m_begin; it != segment->m_end; ++it)
        {
            it->~T();
        }
        operator delete(segment->m_begin);
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
template< class T, size_t segment_size, typename unary >
inline auto segmented_stack< T, segment_size, unary >::operator=(segmented_stack&& o) -> segmented_stack&
{
    free();
    size_t index = std::distance(o.m_segments.begin(), o.m_last_segment);
    m_segments = std::move(o.m_segments);
    m_last_segment = m_segments.begin() + index;
    m_notifier = std::move(o.m_notifier);
    return *this;
}

//----------------------------------------------------------------------------------------------------------------------------------
template< class T, size_t segment_size, typename unary >
inline segmented_stack< T, segment_size, unary >::segmented_stack(segmented_stack&& o) :
    m_notifier(std::move(o.m_notifier))
{
    size_t index = std::distance(o.m_segments.begin(), o.m_last_segment);
    m_segments = std::move(o.m_segments);
    m_last_segment = m_segments.begin() + index;
}

//----------------------------------------------------------------------------------------------------------------------------------
template< class T, size_t segment_size, typename unary >
inline segmented_stack< T, segment_size, unary >::~segmented_stack()
{
    free();
}

//----------------------------------------------------------------------------------------------------------------------------------
template< class T, size_t segment_size, typename unary >
inline void segmented_stack< T, segment_size, unary >::set_notifier(const unary& notifier)
{
    m_notifier = notifier;
}

//----------------------------------------------------------------------------------------------------------------------------------
template< class T, size_t segment_size, typename unary >
inline typename segmented_stack< T, segment_size, unary >::size_type
segmented_stack< T, segment_size, unary >::max_size() const
{
    return size_t(-1) / sizeof(T);
}

//----------------------------------------------------------------------------------------------------------------------------------
template< class T, size_t segment_size, typename unary >
inline typename segmented_stack< T, segment_size, unary >::size_type
segmented_stack< T, segment_size, unary >::size() const
{
    return (m_last_segment - m_segments.begin()) * segment_size +
        (m_last_segment != m_segments.end() ? m_last_segment->m_end - m_last_segment->m_begin : 0);
}

//----------------------------------------------------------------------------------------------------------------------------------
template< class T, size_t segment_size, typename unary >
inline typename segmented_stack< T, segment_size, unary >::size_type
segmented_stack< T, segment_size, unary >::capacity() const
{
    return m_segments.size() * segment_size;
}

//----------------------------------------------------------------------------------------------------------------------------------
template< class T, size_t segment_size, typename unary >
inline void segmented_stack< T, segment_size, unary >::reserve_additional_segment() // throw( bad_alloc )
{
    if (m_segments.size() >= m_segments.capacity())
    {
        const size_t cur_capacity = m_segments.capacity();
        const size_t max_capacity = m_segments.max_size();
        if (capacity() > max_size() - segment_size ||
            cur_capacity >= max_capacity)
        {
            throw std::bad_alloc();
        }

        const size_t last_segment = m_last_segment - m_segments.begin(); // Record relative offset before re-allocation.
        size_t add_capacity = min(max(cur_capacity / 2, size_t(1)), max_capacity - cur_capacity); // STL default policy
        while (add_capacity)
        {
            if (1 == add_capacity)
            {
                m_segments.reserve(cur_capacity + add_capacity); // throw( bad_alloc )
                break;
            }
            else
            {
                try
                {
                    m_segments.reserve(cur_capacity + add_capacity); // throw( bad_alloc )
                    break;
                }
                catch (std::bad_alloc&)
                {
                    // Try less.
                }

                add_capacity /= 2;
            }
        }
        m_last_segment = m_segments.begin() + last_segment;
    }

    void* p = m_notifier(true);
    if (!p)
    {
        p = operator new(segment_size * sizeof(T)); // throw( bad_alloc )
    }
    m_segments.push_back(segment_range(p));
}

//----------------------------------------------------------------------------------------------------------------------------------
template< class T, size_t segment_size, typename unary >
inline void segmented_stack< T, segment_size, unary >::reserve(size_type s) // throw( bad_alloc )
{
    while (s > capacity())
    {
        reserve_additional_segment(); // throw( bad_alloc )
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
template< class T, size_t segment_size, typename unary >
inline void segmented_stack< T, segment_size, unary >::push(const value_type& v) // throw( ... )
{
    if (m_last_segment->m_end >= m_last_segment->m_begin + segment_size)
    {
        if (m_last_segment + 1 == m_segments.end())
        {
            reserve_additional_segment(); // throw( bad_alloc )
        }
        ++m_last_segment;
    }

    T*& pEnd = m_last_segment->m_end;
    ASSUME(pEnd);
    new (pEnd) value_type(v); // throw( ... )
    ++(pEnd);
}

//----------------------------------------------------------------------------------------------------------------------------------
template< class T, size_t segment_size, typename unary >
inline bool segmented_stack< T, segment_size, unary >::reserve_contiguous(size_type s) noexcept
{
    assert(s <= segment_size);
    if (size_t(m_last_segment->m_begin + segment_size - m_last_segment->m_end) < s)
    {
        return reserve_contiguous_alloc(s);
    }

    return true;
}

//----------------------------------------------------------------------------------------------------------------------------------
template< class T, size_t segment_size, typename unary >
inline typename segmented_stack< T, segment_size, unary >::pointer
segmented_stack< T, segment_size, unary >::append_contiguous_manually() noexcept
{
    return m_last_segment->m_end;
}

//----------------------------------------------------------------------------------------------------------------------------------
template< class T, size_t segment_size, typename unary >
inline typename segmented_stack< T, segment_size, unary >::pointer
segmented_stack< T, segment_size, unary >::append_contiguous_manually(size_type s) noexcept
{
    // Check that reserve_contiguous was called before and was successful:
    assert(size_t(m_last_segment->m_begin + segment_size - m_last_segment->m_end) >= s);

    T*& pEnd = m_last_segment->m_end;
    pointer p = pEnd;
    pEnd += s;
    return p;
}

//----------------------------------------------------------------------------------------------------------------------------------
template< class T, size_t segment_size, typename unary >
__declspec(noinline) inline bool segmented_stack< T, segment_size, unary >::reserve_contiguous_alloc(size_type /*s*/) noexcept
{
    try
    {
        if (m_last_segment + 1 == m_segments.end())
        {
            reserve_additional_segment(); // throw( bad_alloc )
        }
        // This effectively pushes undefined values, until the segment transitions to a new empty one.
        // However, the segment's end pointer is untouched, so that those undefined values can be skipped over
        // with the appropriate iterators.
        ++m_last_segment;
        return true;
    }
    catch (std::bad_alloc&)
    {
        m_notifier(false);
        return false;
    }
}

//----------------------------------------------------------------------------------------------------------------------------------
template< class T, size_t segment_size, typename unary >
inline void segmented_stack< T, segment_size, unary >::swap(segmented_stack< T, segment_size, unary >& o)
{
    m_segments.swap(o.m_segments);
    std::swap(m_last_segment, o.m_last_segment);
    std::swap(m_notifier, o.m_notifier);
}

//----------------------------------------------------------------------------------------------------------------------------------
template< class T, size_t segment_size, typename unary >
inline void segmented_stack< T, segment_size, unary >::clear()
{
    for (segment_vector::iterator segment = m_segments.begin(); segment != m_segments.end(); ++segment)
    {
        for (segment_range::iterator it = segment->m_begin; it != segment->m_end; ++it)
        {
            it->~T();
        }
        segment->m_end = segment->m_begin;
    }
    m_last_segment = m_segments.begin();
}

//----------------------------------------------------------------------------------------------------------------------------------
template< class T, size_t segment_size, typename unary >
inline bool segmented_stack< T, segment_size, unary >::empty()
{
    return m_last_segment == m_segments.begin() && 
        (m_last_segment == m_segments.end() || m_last_segment->m_end == m_last_segment->m_begin);
}

//----------------------------------------------------------------------------------------------------------------------------------
template< class T, size_t segment_size, typename unary >
inline typename segmented_stack< T, segment_size, unary >::segment_iterator
segmented_stack< T, segment_size, unary >::segments_begin()
{
    return m_segments.begin();
}

//----------------------------------------------------------------------------------------------------------------------------------
template< class T, size_t segment_size, typename unary >
inline typename segmented_stack< T, segment_size, unary >::segment_iterator
segmented_stack< T, segment_size, unary >::segments_end()
{
    return m_last_segment == m_segments.end() ? m_last_segment : m_last_segment + 1;
}

//----------------------------------------------------------------------------------------------------------------------------------
template< class T, size_t segment_size, typename unary >
inline typename segmented_stack< T, segment_size, unary >::const_segment_iterator
segmented_stack< T, segment_size, unary >::segments_begin() const
{
    return m_segments.begin();
}

//----------------------------------------------------------------------------------------------------------------------------------
template< class T, size_t segment_size, typename unary >
inline typename segmented_stack< T, segment_size, unary >::const_segment_iterator
segmented_stack< T, segment_size, unary >::segments_end() const
{
    return m_last_segment == m_segments.end() ? m_last_segment : m_last_segment + 1;
}

//----------------------------------------------------------------------------------------------------------------------------------
template< class T, size_t segment_size, typename unary >
inline typename segmented_stack< T, segment_size, unary >::segment_range::iterator
segmented_stack< T, segment_size, unary >::segment_range::begin()
{
    return m_begin;
}

//----------------------------------------------------------------------------------------------------------------------------------
template< class T, size_t segment_size, typename unary >
inline typename segmented_stack< T, segment_size, unary >::segment_range::iterator
segmented_stack< T, segment_size, unary >::segment_range::end()
{
    return m_end;
}

//----------------------------------------------------------------------------------------------------------------------------------
template< class T, size_t segment_size, typename unary >
inline typename segmented_stack< T, segment_size, unary >::segment_range::const_iterator
segmented_stack< T, segment_size, unary >::segment_range::begin() const
{
    return m_begin;
}

//----------------------------------------------------------------------------------------------------------------------------------
template< class T, size_t segment_size, typename unary >
inline typename segmented_stack< T, segment_size, unary >::segment_range::const_iterator
segmented_stack< T, segment_size, unary >::segment_range::end() const
{
    return m_end;
}