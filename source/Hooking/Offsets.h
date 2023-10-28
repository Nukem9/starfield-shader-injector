#pragma once

class Offsets
{
private:
	Offsets() = delete;

	struct PatternByte
	{
		uint8_t Value = 0;
		bool Wildcard = false;
	};

	class SignatureStorageWrapper
	{
		constexpr static uintptr_t SentinelValue = 0x1337C0DE1337C0DE;

	public:
		uintptr_t m_Address = SentinelValue;
		const std::span<const PatternByte> m_Signature;

		SignatureStorageWrapper(std::span<const PatternByte> Signature);

		bool IsValid() const
		{
			return m_Address != SentinelValue;
		}

		uintptr_t Address() const
		{
			return m_Address;
		}

		std::span<const PatternByte> FindLongestRun() const;
		std::span<const uint8_t>::iterator ScanRegion(std::span<const uint8_t> Region) const;

		static std::vector<Offsets::SignatureStorageWrapper *>& GetInitializationEntries();
	};

public:
	class ImplOffset
	{
	private:
		std::uintptr_t m_Address;

	public:
		ImplOffset(std::uintptr_t Address) : m_Address(Address) {}

		operator std::uintptr_t() const
		{
			return m_Address;
		}
	};

	template<size_t PatternLength>
	class ImplPatternLiteral
	{
		static_assert(PatternLength >= 3, "Signature must be at least 1 byte long");

	public:
		PatternByte m_Signature[(PatternLength / 2) + 1];
		size_t m_SignatureLength = 0;

		consteval ImplPatternLiteral(const char (&Pattern)[PatternLength])
		{
			for (size_t i = 0; i < PatternLength - 1;)
			{
				switch (Pattern[i])
				{
				case ' ':
					i++;
					continue;

				case '?':
					if ((i + 2) < PatternLength && Pattern[i + 1] != ' ')
						throw "Invalid wildcard";

					m_Signature[m_SignatureLength].Wildcard = true;
					break;

				default:
					m_Signature[m_SignatureLength].Value = AsciiHexToBytes<uint8_t>(Pattern + i);
					break;
				}

				i += 2;
				m_SignatureLength++;
			}
		}

		consteval std::span<const PatternByte> GetSignature() const
		{
			return { m_Signature, m_SignatureLength };
		}

	private:
		template<typename T, size_t Digits = sizeof(T) * 2>
		consteval static T AsciiHexToBytes(const char *Hex)
		{
			auto charToByte = [](char C) consteval -> T
			{
				if (C >= 'A' && C <= 'F')
					return C - 'A' + 10;
				else if (C >= 'a' && C <= 'f')
					return C - 'a' + 10;
				else if (C >= '0' && C <= '9')
					return C - '0';

				throw "Invalid hexadecimal digit";
			};

			T value = {};

			for (size_t i = 0; i < Digits; i++)
				value |= charToByte(Hex[i]) << (4 * (Digits - i - 1));

			return value;
		}
	};

	template<ImplPatternLiteral Literal>
	class Signature
	{
	private:
		const static inline SignatureStorageWrapper m_Storage { Literal.GetSignature() };

	public:
		static ImplOffset GetOffset()
		{
			return ImplOffset(m_Storage.Address());
		}
	};

	static bool Initialize();
	static ImplOffset Relative(std::uintptr_t Offset);
	static ImplOffset Absolute(std::uintptr_t Address);
#define Signature(X) Signature<Offsets::ImplPatternLiteral(X)>::GetOffset()
};
