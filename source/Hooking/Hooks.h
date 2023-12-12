#pragma once

namespace Hooks
{
	bool Initialize();
	bool WriteJump(std::uintptr_t TargetAddress, const void *CallbackFunction, void **OriginalFunction = nullptr);
	bool WriteCall(std::uintptr_t TargetAddress, const void *CallbackFunction, void **OriginalFunction = nullptr);
	bool WriteVirtualFunction(std::uintptr_t TableAddress, uint32_t Index, const void *CallbackFunction, void **OriginalFunction = nullptr);
	bool RedirectImport(void *ModuleHandle,
						const char *ImportModuleName,
						std::variant<const char *, int> ImportName,
						const void *CallbackFunction,
						void **OriginalFunction);

	template<typename U, typename... Args>
	bool WriteJump(std::uintptr_t TargetAddress, U (*CallbackFunction)(Args...), U (**OriginalFunction)(Args...) = nullptr)
	{
		return WriteJump(TargetAddress, *reinterpret_cast<void **>(&CallbackFunction), reinterpret_cast<void **>(OriginalFunction));
	}

	template<typename T, typename U, typename... Args>
	bool WriteJump(std::uintptr_t TargetAddress, U (T::*CallbackFunction)(Args...), U (T::**OriginalFunction)(Args...) = nullptr)
	{
		return WriteJump(TargetAddress, *reinterpret_cast<void **>(&CallbackFunction), reinterpret_cast<void **>(OriginalFunction));
	}

	template<typename U, typename... Args>
	bool WriteCall(std::uintptr_t TargetAddress, U (*CallbackFunction)(Args...), U (**OriginalFunction)(Args...) = nullptr)
	{
		return WriteCall(TargetAddress, *reinterpret_cast<void **>(&CallbackFunction), reinterpret_cast<void **>(OriginalFunction));
	}

	template<typename T, typename U, typename... Args>
	bool WriteCall(std::uintptr_t TargetAddress, U (T::*CallbackFunction)(Args...), U (T::**OriginalFunction)(Args...) = nullptr)
	{
		return WriteCall(TargetAddress, *reinterpret_cast<void **>(&CallbackFunction), reinterpret_cast<void **>(OriginalFunction));
	}

	template<typename U, typename... Args>
	bool WriteVirtualFunction(
		std::uintptr_t TableAddress,
		uint32_t Index,
		U (*CallbackFunction)(Args...),
		U (**OriginalFunction)(Args...) = nullptr)
	{
		return WriteVirtualFunction(
			TableAddress,
			Index,
			*reinterpret_cast<void **>(&CallbackFunction),
			reinterpret_cast<void **>(OriginalFunction));
	}

	template<typename T, typename U, typename... Args>
	bool WriteVirtualFunction(
		std::uintptr_t TableAddress,
		uint32_t Index,
		U (T::*CallbackFunction)(Args...),
		U (T::**OriginalFunction)(Args...) = nullptr)
	{
		return WriteVirtualFunction(
			TableAddress,
			Index,
			*reinterpret_cast<void **>(&CallbackFunction),
			reinterpret_cast<void **>(OriginalFunction));
	}

	namespace Impl
	{
		class TxnBase
		{
		protected:
			TxnBase(void (*Initializer)(), const char *Name);
			TxnBase(bool (*Initializer)(), const char *Name);
		};
	}
};

#define DECLARE_HOOK_TRANSACTION(Name)                                          \
	class ___HookTxnInit##Name : Hooks::Impl::TxnBase                           \
	{                                                                           \
	public:                                                                     \
		___HookTxnInit##Name(auto Initializer) : TxnBase(Initializer, #Name) {} \
	} const static ___HookTxnCallbackDoNotUse = +[]
