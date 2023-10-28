#pragma once

namespace Offsets
{
	namespace Impl
	{
		struct PatternEntry
		{
			uint8_t Value = 0;
			bool Wildcard = false;
		};

		using ByteSpan = std::span<const uint8_t>;
		using PatternSpan = std::span<const PatternEntry>;

		template<size_t PatternLength>
		class PatternLiteral
		{
			static_assert(PatternLength >= 3, "Signature must be at least 1 byte long");

		public:
			PatternEntry m_Signature[(PatternLength / 2) + 1];
			size_t m_SignatureLength = 0;

			consteval PatternLiteral(const char (&Pattern)[PatternLength])
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

			consteval PatternSpan GetSignature() const
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

		class SignatureStorageWrapper
		{
		public:
			const PatternSpan m_Signature;
			uintptr_t m_Address = 0;
			bool m_IsResolved = false;

			SignatureStorageWrapper(PatternSpan Signature);

			bool IsValid() const
			{
				return m_IsResolved;
			}

			uintptr_t Address() const
			{
				return m_Address;
			}

			ByteSpan::iterator ScanRegion(ByteSpan Region) const;

		private:
			PatternSpan FindLongestRun() const;
		};

		class Offset
		{
		private:
			const uintptr_t m_Address;

		public:
			Offset(uintptr_t Address) : m_Address(Address) {}

			operator uintptr_t() const
			{
				return m_Address;
			}
		};

		template<PatternLiteral Literal>
		class Signature
		{
		private:
			const static inline SignatureStorageWrapper m_Storage { Literal.GetSignature() };

		public:
			static Offset GetOffset()
			{
				return Offset(m_Storage.Address());
			}
		};
	}

	bool Initialize();
	Impl::Offset Relative(std::uintptr_t RelAddress);
	Impl::Offset Absolute(std::uintptr_t AbsAddress);
#define Signature(X) Impl::Signature<Offsets::Impl::PatternLiteral(X)>::GetOffset()
}
