#include <Windows.h>
#include <execution>

std::vector<Offsets::SignatureStorageWrapper *>& Offsets::SignatureStorageWrapper::GetInitializationEntries()
{
	// Has to be a function-local static to avoid initialization order issues
	static std::vector<Offsets::SignatureStorageWrapper *> entries;
	return entries;
}

Offsets::SignatureStorageWrapper::SignatureStorageWrapper(std::span<const PatternByte> Signature) : m_Signature(Signature)
{
	GetInitializationEntries().push_back(this);
}

std::span<const Offsets::PatternByte> Offsets::SignatureStorageWrapper::FindLongestRun() const
{
	// Find the largest consecutive range of bytes without wildcards
	std::span<const PatternByte> largestRange = {};

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

std::span<const uint8_t>::iterator Offsets::SignatureStorageWrapper::ScanRegion(std::span<const uint8_t> Region) const
{
	if (m_Signature.empty() || m_Signature.size() > Region.size())
		return Region.end();

	const auto nonWildcardSubrange = FindLongestRun();
	const auto subrangeAdjustment = nonWildcardSubrange.data() - m_Signature.data();

	if (nonWildcardSubrange.empty()) // if (all wildcards)
		return Region.begin();

	auto testFullSignature = [this](std::span<const uint8_t>::iterator Iter)
	{
		for (const auto& entry : m_Signature)
		{
			if (!entry.Wildcard && entry.Value != *Iter)
				return false;

			Iter++;
		}

		return true;
	};

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

		if (testFullSignature(pos - subrangeAdjustment))
			return pos - subrangeAdjustment;

	nextIter:;
	}
#else
	// Linear vectorized search. Turns out CPUs are 2-3x faster at this than BMH.
	//
	// Unrolled version of http://0x80.pl/articles/simd-strfind.html#generic-sse-avx2 since AVX2 support
	// can't be assumed. iterCount is used to avoid three extra branches per loop instead of comparing pos.
	const ptrdiff_t perIterSize = sizeof(__m128i) * 2;
	ptrdiff_t iterCount = (scanEnd - scanStart) / perIterSize;

	const __m128i firstBlockMask = _mm_set1_epi8(nonWildcardSubrange.front().Value);
	const __m128i lastBlockMask = _mm_set1_epi8(nonWildcardSubrange.back().Value);

	for (; iterCount > 0; pos += perIterSize, iterCount--)
	{
		auto loadMask = [&](const size_t Offset)
		{
			const __m128i firstBlock = _mm_loadu_si128(reinterpret_cast<const __m128i *>(&pos[Offset]));
			const __m128i lastBlock = _mm_loadu_si128(reinterpret_cast<const __m128i *>(&pos[Offset + nonWildcardSubrange.size() - 1]));
			const __m128i mask128 = _mm_and_si128(_mm_cmpeq_epi8(firstBlockMask, firstBlock), _mm_cmpeq_epi8(lastBlockMask, lastBlock));

			return static_cast<uint32_t>(_mm_movemask_epi8(mask128)) << Offset;
		};

		auto mask = loadMask(0) | loadMask(sizeof(__m128i));

		while (mask != 0)
		{
			// Get the index of the first set (leftmost) bit and clear it
			unsigned long bitIndex;
			_BitScanForward(&bitIndex, mask);

			mask &= (mask - 1);

			if (testFullSignature(pos + bitIndex - subrangeAdjustment))
				return pos + bitIndex - subrangeAdjustment;
		}
	}

	for (; pos <= scanEnd; pos++)
	{
		if (testFullSignature(pos - subrangeAdjustment))
			return pos - subrangeAdjustment;
	}
#endif

	return Region.end();
}

bool Offsets::Initialize()
{
	spdlog::info("{}():", __FUNCTION__);

	auto dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER *>(GetModuleHandleA(nullptr));
	auto ntHeaders = reinterpret_cast<const IMAGE_NT_HEADERS *>((uintptr_t)dosHeader + dosHeader->e_lfanew);
	auto region = std::span { reinterpret_cast<const uint8_t *>(dosHeader), ntHeaders->OptionalHeader.SizeOfImage };

	auto& entries = SignatureStorageWrapper::GetInitializationEntries();

	auto start = std::chrono::high_resolution_clock::now();

	// Run all scans in parallel
	std::for_each(std::execution::par, entries.begin(), entries.end(),
	[&](auto& P)
	{
		auto itr = P->ScanRegion(region);

		if (itr != region.end())
			P->m_Address = reinterpret_cast<uintptr_t>(region.data() + std::distance(region.begin(), itr));
	});

	auto end = std::chrono::high_resolution_clock::now();
	auto delta = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

	spdlog::info("Resolved {} signatures in {} us.", entries.size(), delta);

	const auto failedSignatureCount = std::count_if(entries.begin(), entries.end(),
	[](const auto& P)
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

Offsets::ImplOffset Offsets::Relative(std::uintptr_t Offset)
{
	return ImplOffset(reinterpret_cast<std::uintptr_t>(GetModuleHandleA(nullptr)) + Offset);
}

Offsets::ImplOffset Offsets::Absolute(std::uintptr_t Address)
{
	return ImplOffset(Address);
}
