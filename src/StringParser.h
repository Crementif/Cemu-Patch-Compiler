#pragma once

class StringTokenParser
{
public:
	StringTokenParser() = default;

	StringTokenParser(const char* input, int32_t inputLen) : m_view(input, inputLen) {}
	explicit StringTokenParser(std::string_view input) : m_view(input) {}

	// skip whitespaces at current position
	void skipWhitespaces()
	{
		while (!m_view.empty() && (m_view.front() == ' ' || m_view.front() == '\t'))
		{
			m_view.remove_prefix(1);
		}
	}

	// decrease string length as long as there is a whitespace at the end
	void trimWhitespaces()
	{
		while (!m_view.empty() && (m_view.back() == ' ' || m_view.back() == '\t'))
		{
			m_view.remove_suffix(1);
		}
	}

	bool isEndOfString() const
	{
		return m_view.empty();
	}

	int32_t skipToCharacter(const char c)
	{
		size_t pos = m_view.find(c);
		if (pos == std::string_view::npos)
		{
			return -1;
		}
		m_view.remove_prefix(pos);
		return (int32_t)pos;
	}

	bool matchWordI(std::string_view word)
	{
		skipWhitespaces();
		if (m_view.size() < word.size())
		{
			return false;
		}

		for (size_t i = 0; i < word.size(); ++i)
		{
			if (toupper((unsigned char)m_view[i]) != toupper((unsigned char)word[i]))
			{
				return false;
			}
		}

		m_view.remove_prefix(word.size());
		return true;
	}

	bool compareCharacter(size_t relativeIndex, const char c) const
	{
		if (relativeIndex >= m_view.size())
		{
			return false;
		}
		return m_view[relativeIndex] == c;
	}

	bool compareCharacterI(size_t relativeIndex, const char c) const
	{
		if (relativeIndex >= m_view.size())
		{
			return false;
		}
		return toupper((unsigned char)m_view[relativeIndex]) == toupper((unsigned char)c);
	}

	void skipCharacters(size_t count)
	{
		if (count > m_view.size())
		{
			count = m_view.size();
		}
		m_view.remove_prefix(count);
	}

	bool parseU32(uint32_t& val)
	{
		skipWhitespaces();
		if (m_view.empty())
		{
			return false;
		}

		bool isHex = false;
		size_t index = 0;
		if (m_view.size() >= 2 && m_view[0] == '0' && (m_view[1] == 'x' || m_view[1] == 'X'))
		{
			isHex = true;
			index = 2;
		}
		else if (m_view[0] == '0')
		{
			isHex = true;
			index = 1;
		}

		if (m_view.size() <= index)
		{
			return false;
		}

		uint32_t value = 0;
		size_t firstDigitIndex = index;
		if (isHex)
		{
			for (; index < m_view.size(); ++index)
			{
				char c = m_view[index];
				if (c >= '0' && c <= '9')
				{
					value = value * 16 + (c - '0');
				}
				else if (c >= 'a' && c <= 'f')
				{
					value = value * 16 + (c - 'a' + 10);
				}
				else if (c >= 'A' && c <= 'F')
				{
					value = value * 16 + (c - 'A' + 10);
				}
				else
				{
					break;
				}
			}
		}
		else
		{
			for (; index < m_view.size(); ++index)
			{
				char c = m_view[index];
				if (c >= '0' && c <= '9')
				{
					value = value * 10 + (c - '0');
				}
				else
				{
					break;
				}
			}
		}

		if (index == firstDigitIndex)
		{
			return false;
		}

		m_view.remove_prefix(index);
		val = value;
		return true;
	}

	bool parseSymbolName(const char*& symbolStr, int32_t& symbolLength)
	{
		skipWhitespaces();
		if (m_view.empty())
		{
			return false;
		}

		char first = m_view[0];
		if (!(first >= 'a' && first <= 'z') &&
			!(first >= 'A' && first <= 'Z') &&
			!(first >= '0' && first <= '9') &&
			!(first == '_'))
		{
			return false;
		}

		size_t idx = 1;
		while (idx < m_view.size())
		{
			char c = m_view[idx];
			if (!(c >= 'a' && c <= 'z') &&
				!(c >= 'A' && c <= 'Z') &&
				!(c >= '0' && c <= '9') &&
				!(c == '_') && !(c == '.'))
			{
				break;
			}
			idx++;
		}

		symbolStr = m_view.data();
		symbolLength = (int32_t)idx;
		m_view.remove_prefix(idx);
		return true;
	}

	std::string_view getView() const
	{
		return m_view;
	}

private:
	std::string_view m_view;
};
