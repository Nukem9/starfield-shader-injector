#pragma once

#include <source_location>

namespace Offsets
{
	bool Initialize();

	namespace detail
	{
		struct PatternEntry
		{
			uint8_t Value;
			uint8_t NotWildcard;
		};

		using ByteSpan = std::span<const uint8_t>;
		using PatternSpan = std::span<const PatternEntry>;

		template<size_t PatternLength>
		class PatternLiteral
		{
			static_assert(PatternLength >= 3, "Signature must be at least 1 byte long");

		public:
			PatternEntry m_Signature[(PatternLength / 2) + 1] = {};
			size_t m_SignatureLength = 0;

#if defined(_DEBUG)
			char m_File[256] = {};
			size_t m_Line = 0;
#endif

			explicit consteval PatternLiteral(
#if defined(_DEBUG)
				const char (&Pattern)[PatternLength],
				const std::source_location& SourceLocation = std::source_location::current())
			{
				m_Line = SourceLocation.line();
				const auto fileName = SourceLocation.file_name();

				for (size_t i = 0; fileName[i] != '\0'; i++)
					m_File[i] = fileName[i];
#else
				const char (&Pattern)[PatternLength])
			{
#endif
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

						m_Signature[m_SignatureLength].Value = 0;
						m_Signature[m_SignatureLength].NotWildcard = 0;
						break;

					default:
						m_Signature[m_SignatureLength].Value = AsciiHexToBytes<uint8_t>(Pattern + i);
						m_Signature[m_SignatureLength].NotWildcard = 0xFF;
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

			consteval const char *GetFile() const
			{
#if defined(_DEBUG)
				return m_File;
#else
				return nullptr;
#endif
			}

			consteval size_t GetLine() const
			{
#if defined(_DEBUG)
				return m_Line;
#else
				return 0;
#endif
			}

		private:
			template<typename T, size_t Digits = sizeof(T) * 2>
			static consteval T AsciiHexToBytes(const char *Hex)
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
			template<PatternLiteral Literal>
			friend class Signature;

			friend bool Offsets::Initialize();

		private:
			const PatternSpan m_Signature;
			const char *m_File;
			const size_t m_Line;
			uintptr_t m_Address = 0;
			bool m_IsResolved = false;

			SignatureStorageWrapper(const PatternSpan& Signature, const char *File, size_t Line);
			PatternSpan FindLongestNonWildcardRun() const noexcept;
			bool IsCompleteMatch(ByteSpan::iterator Iterator) const noexcept;
			ByteSpan::iterator ScanRegion(const ByteSpan& Region) const noexcept;
		};

		class Offset
		{
		private:
			uintptr_t m_Address;

		public:
			Offset(uintptr_t Address) : m_Address(Address) {}

			Offset& AsAdjusted(ptrdiff_t Offset)
			{
				m_Address += Offset;
				return *this;
			}

			Offset& AsRipRelative(ptrdiff_t InstructionLength)
			{
				auto relativeAdjust = *reinterpret_cast<int32_t *>(m_Address + (InstructionLength - sizeof(int32_t)));
				m_Address += relativeAdjust + InstructionLength;

				return *this;
			}

			template<typename T>
			auto ToPointer() const
			{
				std::conditional_t<std::is_member_function_pointer_v<T>, T, T *> pointer = {};
				memcpy(&pointer, &m_Address, sizeof(uintptr_t));

				return pointer;
			}

			operator uintptr_t() const
			{
				return m_Address;
			}
		};

		template<PatternLiteral Literal>
		class Signature
		{
		private:
			const static inline SignatureStorageWrapper m_Storage { Literal.GetSignature(), Literal.GetFile(), Literal.GetLine() };

		public:
			Signature() = delete;

			static Offset GetOffset()
			{
				return Offset(m_Storage.m_Address);
			}
		};
	}

	detail::Offset Relative(uintptr_t RelAddress);
	detail::Offset Absolute(uintptr_t AbsAddress);
#define Signature(X) detail::Signature<Offsets::detail::PatternLiteral(X)>::GetOffset()
}
