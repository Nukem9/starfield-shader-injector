#include <execution>
#include <emmintrin.h>
#include <Windows.h>

namespace Offsets::detail
{
	static std::vector<SignatureStorageWrapper *>& GetInitializationEntries()
	{
		// Has to be a function-local static to avoid initialization order issues
		static std::vector<SignatureStorageWrapper *> entries;
		return entries;
	}

	SignatureStorageWrapper::SignatureStorageWrapper(const PatternSpan& Signature, const char *File, size_t Line)
		: m_Signature(Signature),
		  m_File(File),
		  m_Line(Line)
	{
		GetInitializationEntries().emplace_back(this);
	}

	__forceinline PatternSpan SignatureStorageWrapper::FindLongestNonWildcardRun() const noexcept
	{
		// Scan forwards until we hit the first non-wildcard byte, then scan backwards until
		// we hit the first non-wildcard byte.
		//
		// ? 86 ? 01 87 47 ? ? ? ? 48 ? ?
		//   ^F				       ^B
		for (size_t i = 0; i < m_Signature.size(); i++)
		{
			if (!m_Signature[i].NotWildcard)
				continue;

			for (size_t j = m_Signature.size(); j-- > i;)
			{
				if (m_Signature[j].NotWildcard)
					return m_Signature.subspan(i, j - i + 1);
			}

			break;
		}

		return m_Signature.subspan<0, 0>();
	}

	__forceinline bool SignatureStorageWrapper::IsCompleteMatch(ByteSpan::iterator Data) const noexcept
	{
		__assume(m_Signature.size() != 0);
		__assume(m_Signature.begin() != m_Signature.end());

		// Per-byte wildcard comparison loop designed to allow compiler uint16 load coalescing
		for (auto cur = m_Signature.begin(), end = m_Signature.end(); cur != end; ++cur, ++Data)
		{
			if (const auto e = *cur; ((*Data ^ e.Value) & e.NotWildcard) != 0)
				return false;
		}

		return true;
	}

	ByteSpan::iterator SignatureStorageWrapper::ScanRegion(const ByteSpan& Region) const noexcept
	{
		if (m_Signature.empty() || m_Signature.size() > Region.size())
			return Region.end();

		const auto nonWildcardSubrange = FindLongestNonWildcardRun();

		if (nonWildcardSubrange.empty()) // if (all wildcards)
			return Region.begin();

		constexpr size_t simdChunkSize = (sizeof(void *) == 8) ? 64 : 32;

		// Unrolled (via templates) version of http://0x80.pl/articles/simd-strfind.html#generic-sse-avx2
		auto runScanLoop = [&]<typename T>(T&& CandidateMaskLoadCallback) noexcept
		{
			const auto subrangeAdjustment = std::to_address(nonWildcardSubrange.begin()) - std::to_address(m_Signature.begin());
			const auto dataEnd = (Region.end() - m_Signature.size()) + subrangeAdjustment; // Seek backward to prevent overflow
			auto data = Region.begin() + subrangeAdjustment;							   // Seek forward to prevent underflow

			for (const auto chunkedEnd = data + (((dataEnd - data) / simdChunkSize) * simdChunkSize); data != chunkedEnd; data += simdChunkSize)
			{
				auto candidateMask = CandidateMaskLoadCallback(data);

				// The indices of 1-bits in candidateMask map to indices of byte matches in data. Each iteration finds the
				// lowest (LSB) index of a 1-bit in candidateMask, clears it, and tests the full signature at said index.
				while (candidateMask != 0)
				{
#if defined(_M_X64)
					const auto candidateIndex = _tzcnt_u64(candidateMask);
#else
					const auto candidateIndex = _tzcnt_u32(candidateMask);
#endif
					candidateMask &= (candidateMask - 1u);

					if (IsCompleteMatch(data + candidateIndex - subrangeAdjustment))
						return data + candidateIndex - subrangeAdjustment;
				}
			}

			for (; data != dataEnd; ++data)
			{
				if (IsCompleteMatch(data - subrangeAdjustment))
					return data - subrangeAdjustment;
			}

			return Region.end();
		};

		const auto firstBlockMask = _mm_set1_epi8(nonWildcardSubrange.front().Value);
		const auto lastBlockMask = _mm_set1_epi8(nonWildcardSubrange.back().Value);

		return runScanLoop([=](const ByteSpan::iterator Data) noexcept
		{
			auto internalLoad = [&](const size_t VectorIndex) noexcept
			{
				const auto offset = VectorIndex * sizeof(__m128i);
				const auto firstBlock = _mm_loadu_si128(reinterpret_cast<const __m128i *>(&Data[offset]));
				const auto lastBlock = _mm_loadu_si128(reinterpret_cast<const __m128i *>(&Data[offset + nonWildcardSubrange.size() - 1]));
				const auto mask = _mm_and_si128(_mm_cmpeq_epi8(firstBlockMask, firstBlock), _mm_cmpeq_epi8(lastBlockMask, lastBlock));

				return static_cast<size_t>(static_cast<uint32_t>(_mm_movemask_epi8(mask))) << offset;
			};

			return ([&internalLoad]<size_t... Is>(std::index_sequence<Is...>) noexcept
			{
				return (internalLoad(Is) | ...);
			})(std::make_index_sequence<simdChunkSize / sizeof(__m128i)> {});
		});
	}
}

namespace Offsets
{
	using namespace detail;

	bool Initialize()
	{
		spdlog::info("{}():", __FUNCTION__);

		auto dosHeader = reinterpret_cast<const PIMAGE_DOS_HEADER>(GetModuleHandleW(nullptr));
		auto ntHeaders = reinterpret_cast<const PIMAGE_NT_HEADERS>(reinterpret_cast<uintptr_t>(dosHeader) + dosHeader->e_lfanew);
		auto region = ByteSpan { reinterpret_cast<const uint8_t *>(dosHeader), ntHeaders->OptionalHeader.SizeOfImage };

		// Intialize() may be called from DllMain() which holds the loader lock. New threads can't be spawned as
		// long as the loader lock is held. Therefore parallelization is impossible.
		auto entries = std::move(GetInitializationEntries());

		std::for_each(
			std::execution::seq,
			entries.begin(),
			entries.end(),
			[&region](auto& P)
		{
			if (const auto itr = P->ScanRegion(region); itr != region.end())
			{
				P->m_Address = reinterpret_cast<uintptr_t>(std::to_address(itr));
				P->m_IsResolved = true;
			}
		});

		const auto failedSignatureCount = std::ranges::count_if(
			entries,
			[](const auto& P)
		{
			if (!P->m_IsResolved && P->m_File)
				spdlog::warn("Failed to resolve signature at {}:{}.", P->m_File, P->m_Line);

			return !P->m_IsResolved;
		});

		if (failedSignatureCount > 0)
		{
			spdlog::error("Failed to resolve {} out of {} signatures.", failedSignatureCount, entries.size());
			return false;
		}

		spdlog::info("Done!");
		return true;
	}

	Offset Relative(uintptr_t RelAddress)
	{
		return Offset(reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)) + RelAddress);
	}

	Offset Absolute(uintptr_t AbsAddress)
	{
		return Offset(AbsAddress);
	}
}
