#pragma once

#include <BAN/Formatter.h>
#include <BAN/ForwardList.h>
#include <BAN/Iterators.h>
#include <BAN/Optional.h>

namespace BAN
{

	class StringView
	{
	public:
		using size_type = size_t;
		using const_iterator = ConstIteratorSimple<char, StringView>;

	public:
		StringView();
		StringView(const String&);
		StringView(const char*, size_type = -1);

		const_iterator begin() const { return const_iterator(m_data); }
		const_iterator end() const { return const_iterator(m_data + m_size); }

		char operator[](size_type) const;

		bool operator==(const String&) const;
		bool operator==(StringView) const;
		bool operator==(const char*) const;

		StringView substring(size_type, size_type = -1) const;

		ErrorOr<Vector<StringView>> split(char, bool = false) const;
		ErrorOr<Vector<StringView>> split(bool(*comp)(char), bool = false) const;

		char back() const;
		char front() const;

		BAN::Optional<size_type> find(char) const;
		BAN::Optional<size_type> find(bool(*comp)(char)) const;

		bool contains(char) const;
		size_type count(char) const;

		bool empty() const;
		size_type size() const;

		const char* data() const;

	private:
		const char*	m_data = nullptr;
		size_type	m_size = 0;
	};

}

inline BAN::StringView operator""sv(const char* str, BAN::StringView::size_type len) { return BAN::StringView(str, len); }

namespace BAN::Formatter
{

	template<typename F>
	void print_argument(F putc, const StringView& sv, const ValueFormat&)
	{
		for (StringView::size_type i = 0; i < sv.size(); i++)
			putc(sv[i]);
	}

}
