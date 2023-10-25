#pragma once

#include <BAN/Span.h>

namespace BAN
{

	template<bool CONST>
	class ByteSpanGeneral
	{
	public:
		using value_type = maybe_const_t<CONST, uint8_t>;
		using size_type = size_t;

	public:
		ByteSpanGeneral() = default;
		ByteSpanGeneral(value_type* data, size_type size)
			: m_data(data)
			, m_size(size)
		{ }

		ByteSpanGeneral(ByteSpanGeneral& other)
			: m_data(other.data())
			, m_size(other.size())
		{ }
		template<bool C2>
		ByteSpanGeneral(const ByteSpanGeneral<C2>& other) requires(CONST)
			: m_data(other.data())
			, m_size(other.size())
		{ }
		ByteSpanGeneral(Span<uint8_t> other)
			: m_data(other.data())
			, m_size(other.size())
		{ }
		ByteSpanGeneral(const Span<const uint8_t>& other) requires(CONST)
			: m_data(other.data())
			, m_size(other.size())
		{ }

		ByteSpanGeneral& operator=(ByteSpanGeneral other)
		{
			m_data = other.data();
			m_size = other.size();
			return *this;
		}
		template<bool C2>
		ByteSpanGeneral& operator=(const ByteSpanGeneral<C2>& other) requires(CONST)
		{
			m_data = other.data();
			m_size = other.size();
			return *this;
		}
		ByteSpanGeneral& operator=(Span<uint8_t> other)
		{
			m_data = other.data();
			m_size = other.size();
			return *this;
		}
		ByteSpanGeneral& operator=(const Span<const uint8_t>& other) requires(CONST)
		{
			m_data = other.data();
			m_size = other.size();
			return *this;
		}

		template<typename S>
		requires(CONST || !is_const_v<S>)
		static ByteSpanGeneral from(S& value)
		{
			return ByteSpanGeneral(reinterpret_cast<value_type*>(&value), sizeof(S));
		}

		template<typename S>
		requires(!CONST && !is_const_v<S>)
		S& as()
		{
			ASSERT(m_data);
			ASSERT(m_size >= sizeof(S));
			return *reinterpret_cast<S*>(m_data);
		}

		template<typename S>
		const S& as() const
		{
			ASSERT(m_data);
			ASSERT(m_size >= sizeof(S));
			return *reinterpret_cast<S*>(m_data);
		}

		ByteSpanGeneral slice(size_type offset, size_type length = size_type(-1))
		{
			ASSERT(m_data);
			ASSERT(m_size >= offset);
			if (length == size_type(-1))
				length = m_size - offset;
			ASSERT(m_size >= offset + length);
			return ByteSpanGeneral(m_data + offset, length);
		}

		value_type& operator[](size_type offset)
		{
			ASSERT(offset < m_size);
			return m_data[offset];
		}
		const value_type& operator[](size_type offset) const
		{
			ASSERT(offset < m_size);
			return m_data[offset];
		}

		value_type* data() { return m_data; }
		const value_type* data() const { return m_data; } 

		size_type size() const { return m_size; }

	private:
		value_type* m_data { nullptr };
		size_type m_size { 0 };
	};

	using ByteSpan = ByteSpanGeneral<false>;
	using ConstByteSpan = ByteSpanGeneral<true>;

}