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

	// Use a Boyer-Moore-Horspool search for each signature.
	//
	// While BMH itself doesn't support wildcards, we can still use the largest contiguous signature
	// byte range that excludes wildcards, and then do a linear scan to match the rest.
	const auto bmhSubrange = FindLongestRun();
	const auto subrangeAdjustment = bmhSubrange.data() - m_Signature.data();

	if (bmhSubrange.empty()) // if (all wildcards)
		return Region.begin();

	const auto lastByteIndex = static_cast<ptrdiff_t>(bmhSubrange.size() - 1);
	const auto diff = std::max<ptrdiff_t>(lastByteIndex, 1);

	// Prime the skip lookup table
	std::array<uint8_t, 256> skipLUT;
	skipLUT.fill(static_cast<uint8_t>(diff));

	for (ptrdiff_t i = lastByteIndex - diff; i < lastByteIndex; i++)
		skipLUT[bmhSubrange[i].Value] = static_cast<uint8_t>(lastByteIndex - i);

	const auto scanStart = Region.begin() + subrangeAdjustment;						// Seek forward to prevent underflow
	const auto scanEnd = (Region.end() - m_Signature.size()) + subrangeAdjustment;	// Seek backward to prevent overflow

	for (auto pos = scanStart; pos <= scanEnd; pos += skipLUT[pos[lastByteIndex]])
	{
		// Match the BMH-only subrange first, then run the full check if it succeeds
		for (ptrdiff_t i = lastByteIndex; i >= 0; i--)
		{
			if (!bmhSubrange[i].Wildcard && bmhSubrange[i].Value != pos[i])
				goto nextIter;
		}

		for (ptrdiff_t j = 0; j < std::ssize(m_Signature); j++)
		{
			if (!m_Signature[j].Wildcard && m_Signature[j].Value != pos[j - subrangeAdjustment])
				goto nextIter;
		}

		return pos - subrangeAdjustment;

	nextIter:;
	}

	return Region.end();
}

bool Offsets::Initialize()
{
	spdlog::info("{}():", __FUNCTION__);

	auto dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER *>(GetModuleHandleA(nullptr));
	auto ntHeaders = reinterpret_cast<const IMAGE_NT_HEADERS *>((uintptr_t)dosHeader + dosHeader->e_lfanew);
	auto region = std::span{ reinterpret_cast<const uint8_t *>(dosHeader), ntHeaders->OptionalHeader.SizeOfImage };

	auto& entries = SignatureStorageWrapper::GetInitializationEntries();

	// Run all scans in parallel
	std::for_each(std::execution::par, entries.begin(), entries.end(), [&](auto& P)
	{
		auto itr = P->ScanRegion(region);

		if (itr != region.end())
			P->m_Address = reinterpret_cast<uintptr_t>(region.data() + std::distance(region.begin(), itr));
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

Offsets::ImplOffset Offsets::Relative(std::uintptr_t Offset)
{
	return ImplOffset(reinterpret_cast<std::uintptr_t>(GetModuleHandleA(nullptr)) + Offset);
}

Offsets::ImplOffset Offsets::Absolute(std::uintptr_t Address)
{
	return ImplOffset(Address);
}
