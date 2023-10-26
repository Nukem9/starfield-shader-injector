#include <Windows.h>
#include <detours/detours.h>
#include "Memory.h"

namespace Hooks
{
	struct CallbackEntry
	{
		const char *Name = nullptr;
		std::variant<void(*)(), bool(*)()> Callback;
	};

	struct HookTransactionEntry
	{
		void *DetourStubFunction = nullptr;
		void *TargetFunction = nullptr;
		bool RequiresCallFixup = false;
	};

	struct IATEnumContext
	{
		const char *ModuleName = nullptr;
		std::variant<const char *, int> ImportName;
		const void *CallbackFunction = nullptr;
		void **OriginalFunction = nullptr;
		bool ModuleFound = false;
		bool Succeeded = false;
	};

	std::vector<CallbackEntry>& GetInitializationEntries()
	{
		// Has to be a function-local static to avoid initialization order issues
		static std::vector<CallbackEntry> entries;
		return entries;
	}

	std::vector<std::unique_ptr<HookTransactionEntry>>& GetTransactionEntries()
	{
		static std::vector<std::unique_ptr<HookTransactionEntry>> entries;
		return entries;
	}

	bool Initialize()
	{
		spdlog::info("{}():", __FUNCTION__);

		auto& initEntries = GetInitializationEntries();
		auto& transactionEntries = GetTransactionEntries();

		DetourSetIgnoreTooSmall(true);

		if (DetourTransactionBegin() != NO_ERROR)
			return false;

		DetourUpdateThread(GetCurrentThread());

		for (const auto& entry : initEntries)
		{
			spdlog::info("Setting up hooks for {}...", entry.Name);

			const bool status = std::visit([](auto&& F)
			{
				if constexpr (std::is_same_v<decltype(F()), void>)
					return F(), true;
				else if constexpr (std::is_same_v<decltype(F()), bool>)
					return F();
				else
					static_assert(!sizeof(decltype(F)), "Invalid callback type");
			}, entry.Callback);

			// Bail on the whole process when a callback returns false
			if (!status)
			{
				DetourTransactionAbort();

				spdlog::error("Transaction aborted.");
				return false;
			}
		}

		if (DetourTransactionCommit() != NO_ERROR)
			return false;

		// Apply call fixups
		for (const auto& entry : transactionEntries)
		{
			if (entry->RequiresCallFixup)
				Memory::Patch(reinterpret_cast<std::uintptr_t>(entry->TargetFunction), { 0xE8 });
		}

		initEntries.clear();
		transactionEntries.clear();

		spdlog::info("Done!");
		return true;
	}

	bool WriteJump(std::uintptr_t TargetAddress, const void *CallbackFunction, void **OriginalFunction)
	{
		if (!TargetAddress)
			return false;

		auto ptr = std::make_unique<HookTransactionEntry>();
		ptr->TargetFunction = reinterpret_cast<void *>(TargetAddress);

		// We need a temporary pointer until the transaction is committed
		if (!OriginalFunction)
			OriginalFunction = &ptr->DetourStubFunction;

		// Detours needs the real target function stored in said pointer
		*OriginalFunction = ptr->TargetFunction;

		if (DetourAttach(OriginalFunction, const_cast<void *>(CallbackFunction)) != NO_ERROR)
			return false;

		GetTransactionEntries().emplace_back(std::move(ptr));
		return true;
	}

	bool WriteCall(std::uintptr_t TargetAddress, const void *CallbackFunction, void **OriginalFunction)
	{
		// Identical to WriteJump, but with a call
		if (!TargetAddress)
			return false;

		auto ptr = std::make_unique<HookTransactionEntry>();
		ptr->TargetFunction = reinterpret_cast<void *>(TargetAddress);
		ptr->RequiresCallFixup = true;

		if (!OriginalFunction)
			OriginalFunction = &ptr->DetourStubFunction;

		*OriginalFunction = ptr->TargetFunction;

		if (DetourAttach(OriginalFunction, const_cast<void *>(CallbackFunction)) != NO_ERROR)
			return false;

		GetTransactionEntries().emplace_back(std::move(ptr));
		return true;
	}

	bool RedirectImport(void *ModuleHandle, const char *ImportModuleName, std::variant<const char *, int> ImportName, const void *CallbackFunction, void **OriginalFunction)
	{
		auto moduleCallback = [](PVOID Context, HMODULE, LPCSTR Name) -> BOOL
		{
			auto c = static_cast<IATEnumContext *>(Context);

			c->ModuleFound = Name && _stricmp(Name, c->ModuleName) == 0;
			return !c->Succeeded;
		};

		auto importCallback = [](PVOID Context, ULONG Ordinal, PCSTR Name, PVOID *Func) -> BOOL
		{
			auto c = static_cast<IATEnumContext *>(Context);

			if (!c->ModuleFound)
				return false;

			// If the import name matches...
			const bool matches = [&]()
			{
				if (!Func)
					return false;

				if (std::holds_alternative<const char *>(c->ImportName))
					return _stricmp(Name, std::get<const char *>(c->ImportName)) == 0;

				return std::cmp_equal(Ordinal, std::get<int>(c->ImportName));
			}();

			if (matches)
			{
				// ...swap out the IAT pointer
				if (c->OriginalFunction)
					*c->OriginalFunction = *Func;

				Memory::Patch(
					reinterpret_cast<std::uintptr_t>(Func),
					reinterpret_cast<const std::uint8_t *>(&c->CallbackFunction),
					sizeof(void *));

				c->Succeeded = true;
				return false;
			}

			return true;
		};

		IATEnumContext context
		{
			.ModuleName = ImportModuleName,
			.ImportName = ImportName,
			.CallbackFunction = CallbackFunction,
			.OriginalFunction = OriginalFunction,
		};

		if (!ModuleHandle)
			ModuleHandle = GetModuleHandleA(nullptr);

		DetourEnumerateImportsEx(reinterpret_cast<HMODULE>(ModuleHandle), &context, moduleCallback, importCallback);
		return context.Succeeded;
	}
}

namespace Hooks::Impl
{
	TxnBase::TxnBase(void(*Initializer)(), const char *Name)
	{
		GetInitializationEntries().emplace_back(CallbackEntry
		{
			.Name = Name,
			.Callback = Initializer,
		});
	}

	TxnBase::TxnBase(bool(*Initializer)(), const char *Name)
	{
		GetInitializationEntries().emplace_back(CallbackEntry
		{
			.Name = Name,
			.Callback = Initializer,
		});
	}
}
