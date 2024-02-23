#include <Windows.h>
#include <bit>
#include <execution>

namespace Offsets::Impl
{
	std::vector<SignatureStorageWrapper *>& GetInitializationEntries()
	{
		// Has to be a function-local static to avoid initialization order issues
		static std::vector<SignatureStorageWrapper *> entries;
		return entries;
	}

	SignatureStorageWrapper::SignatureStorageWrapper(PatternSpan Signature) : m_Signature(Signature)
	{
		GetInitializationEntries().emplace_back(this);
	}

	ByteSpan::iterator SignatureStorageWrapper::ScanRegion(ByteSpan Region) const
	{
		if (m_Signature.empty() || m_Signature.size() > Region.size())
			return Region.end();

		const auto nonWildcardSubrange = FindLongestNonWildcardRun();
		const auto subrangeAdjustment = nonWildcardSubrange.data() - m_Signature.data();

		if (nonWildcardSubrange.empty()) // if (all wildcards)
			return Region.begin();

		const auto scanStart = Region.begin() + subrangeAdjustment;					   // Seek forward to prevent underflow
		const auto scanEnd = (Region.end() - m_Signature.size()) + subrangeAdjustment; // Seek backward to prevent overflow
		auto pos = scanStart;

#if 0
		// Use a Boyer-Moore-Horspool search for each signature.
		//
		// While BMH itself doesn't support wildcards, we can still use the largest contiguous signature
		// byte range that excludes wildcards, and then do a linear scan to match the rest.
		const auto lastByteIndex = static_cast<ptrdiff_t>(nonWildcardSubrange.size() - 1);
		const auto diff = std::max<ptrdiff_t>(lastByteIndex, 1);

		// Prime the skip lookup table
		std::array<uint8_t, 256> skipLUT;
		skipLUT.fill(static_cast<uint8_t>(diff));

		for (ptrdiff_t i = lastByteIndex - diff; i < lastByteIndex; i++)
			skipLUT[nonWildcardSubrange[i].Value] = static_cast<uint8_t>(lastByteIndex - i);

		for (; pos <= scanEnd; pos += skipLUT[pos[lastByteIndex]])
		{
			// Match the BMH-only subrange first, then run the full check if it succeeds
			for (ptrdiff_t i = lastByteIndex; i >= 0; i--)
			{
				if (nonWildcardSubrange[i].Value != pos[i])
					goto nextIter;
			}

			if (MatchPattern(pos - subrangeAdjustment))
				return pos - subrangeAdjustment;

		nextIter:;
		}
#else
		// Linear vectorized search. Turns out CPUs are 2-3x faster at this than BMH.
		//
		// Unrolled version of http://0x80.pl/articles/simd-strfind.html#generic-sse-avx2 since AVX2 support
		// can't be assumed. iterCount is used to avoid three extra branches per loop instead of comparing pos.
		const __m128i firstBlockMask = _mm_set1_epi8(nonWildcardSubrange.front().Value);
		const __m128i lastBlockMask = _mm_set1_epi8(nonWildcardSubrange.back().Value);

		auto loadMask = [&](const size_t Offset)
		{
			const __m128i firstBlock = _mm_loadu_si128(reinterpret_cast<const __m128i *>(&pos[Offset]));
			const __m128i lastBlock = _mm_loadu_si128(reinterpret_cast<const __m128i *>(&pos[Offset + nonWildcardSubrange.size() - 1]));
			const __m128i mask = _mm_and_si128(_mm_cmpeq_epi8(firstBlockMask, firstBlock), _mm_cmpeq_epi8(lastBlockMask, lastBlock));

			return static_cast<uint32_t>(_mm_movemask_epi8(mask)) << Offset;
		};

		const ptrdiff_t perIterSize = sizeof(__m128i) * 2;
		ptrdiff_t iterCount = (scanEnd - scanStart) / perIterSize;

		for (; iterCount > 0; iterCount--, pos += perIterSize)
		{
			auto mask = loadMask(0) | loadMask(sizeof(__m128i));

			// The indices of 1-bits in mask map to indices of byte matches in pos. Each iteration finds the
			// lowest (LSB) index of a 1-bit in mask, clears it, and tests the full signature at that index.
			while (mask != 0)
			{
				auto bitIndex = std::countr_zero(mask);
				mask &= (mask - 1);

				if (MatchPattern(pos + bitIndex - subrangeAdjustment))
					return pos + bitIndex - subrangeAdjustment;
			}
		}

		for (; pos <= scanEnd; pos++)
		{
			if (MatchPattern(pos - subrangeAdjustment))
				return pos - subrangeAdjustment;
		}
#endif

		return Region.end();
	}

	bool SignatureStorageWrapper::MatchPattern(ByteSpan::iterator Iterator) const
	{
		return std::equal(m_Signature.begin(), m_Signature.end(), Iterator, [](const auto& A, const auto& B)
		{
			return A.Wildcard || A.Value == B;
		});
	}

	PatternSpan SignatureStorageWrapper::FindLongestNonWildcardRun() const
	{
		PatternSpan largestRange = {};

		for (size_t i = 0; i < m_Signature.size(); i++)
		{
			if (m_Signature[i].Wildcard)
				continue;

			auto getEnd = [this](size_t j)
			{
				for (j += 1; (j < m_Signature.size()) && !m_Signature[j].Wildcard; j++)
					/**/;

				return j;
			};

			if (auto len = getEnd(i) - i; len > largestRange.size())
				largestRange = m_Signature.subspan(i, len);
		}

		return largestRange;
	}
}

namespace Offsets
{
	using namespace Impl;

	bool Initialize()
	{
		spdlog::info("{}():", __FUNCTION__);

		auto dosHeader = reinterpret_cast<const PIMAGE_DOS_HEADER>(GetModuleHandleW(nullptr));
		auto ntHeaders = reinterpret_cast<const PIMAGE_NT_HEADERS>(reinterpret_cast<uintptr_t>(dosHeader) + dosHeader->e_lfanew);
		auto region = std::span { reinterpret_cast<const uint8_t *>(dosHeader), ntHeaders->OptionalHeader.SizeOfImage };

		auto& entries = GetInitializationEntries();

		// Run all scans in parallel
		std::for_each(std::execution::par, entries.begin(), entries.end(), [&](auto& P)
		{
			auto itr = P->ScanRegion(region);

			if (itr != region.end())
			{
				P->m_Address = reinterpret_cast<uintptr_t>(std::to_address(itr));
				P->m_IsResolved = true;
			}
		});

		const auto failedSignatureCount = std::count_if(entries.begin(), entries.end(), [](const auto& P)
		{
			return !P->IsValid();
		});

		if (failedSignatureCount > 0)
		{
			spdlog::info("Failed to resolve {} out of {} signatures.", failedSignatureCount, entries.size());
			return false;
		}

		entries.clear();
		spdlog::info("Done!");
		return true;
	}

	Offset Relative(std::uintptr_t RelAddress)
	{
		return Offset(reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)) + RelAddress);
	}

	Offset Absolute(std::uintptr_t AbsAddress)
	{
		return Offset(AbsAddress);
	}
}
