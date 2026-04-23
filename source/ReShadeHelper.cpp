#include <reshade-imgui/imgui.h>
#include "RE/CreationRenderer.h"
#include "CComPtr.h"
#include "Plugin.h"
#include "ReShadeHelper.h"

extern "C" __declspec(dllexport) const char *NAME = BUILD_PROJECT_NAME;
extern "C" __declspec(dllexport) const char *DESCRIPTION = "ReShade helper integration via Starfield Shader Injector.";

namespace ReShadeHelper
{
	struct ID3D12ReShadeGraphicsCommandList : ID3D12GraphicsCommandList
	{
		using CallbackPre = std::move_only_function<void(reshade::api::command_list *)>;
		using CallbackPost = std::move_only_function<void(ID3D12CommandQueue *)>;

		inline static std::atomic_uint32_t SplitCommandListCount;

		reshade::api::command_list *GetReShadeInterface()
		{
			return GetImplData<reshade::api::command_list *>(this, IID_NativeToReShade);
		}

		template<typename F>
		void QueuePostSubmitCallback(F&& Callback)
		{
			SplitCommandListCount++;
			SetImplData(this, IID_CommandListSubmitCallback, new CallbackPost(Callback));
		}

		auto PopPendingPostSubmitCallback()
		{
			const auto callback = GetImplData<CallbackPost *>(this, IID_CommandListSubmitCallback);

			if (callback)
			{
				SetImplData(this, IID_CommandListSubmitCallback, nullptr);
				SplitCommandListCount--;
			}

			return callback;
		}

		void Init(reshade::api::command_list *ReShadeInterface)
		{
			SetImplData(this, IID_NativeToReShade, ReShadeInterface);
		}

		void Destroy()
		{
			delete PopPendingPostSubmitCallback();
			SetImplData(this, IID_NativeToReShade, nullptr);
		}
	};
	static_assert(sizeof(ID3D12ReShadeGraphicsCommandList) == sizeof(ID3D12CommandList));
	static_assert(alignof(ID3D12ReShadeGraphicsCommandList) == alignof(ID3D12CommandList));

	extern void(WINAPI *D3D12CommandQueueExecuteCommandLists)(ID3D12CommandQueue *, UINT, ID3D12CommandList *const *);
	void WINAPI HookedD3D12CommandQueueExecuteCommandLists(ID3D12CommandQueue *This, UINT NumCommandLists, ID3D12CommandList *const *ppCommandLists);

	EffectRuntimeConfiguration::EffectRuntimeConfiguration(reshade::api::effect_runtime *Runtime)
	{
		Load(Runtime);
	}

	void EffectRuntimeConfiguration::Load(reshade::api::effect_runtime *Runtime)
	{
		reshade::get_config_value(Runtime, NAME, "DrawEffectsBeforeUI", m_DrawEffectsBeforeUI);
		reshade::get_config_value(Runtime, NAME, "AutomaticDepthBufferSelection", m_AutomaticDepthBufferSelection);
	}

	void EffectRuntimeConfiguration::Save(reshade::api::effect_runtime *Runtime)
	{
		reshade::set_config_value(Runtime, NAME, "DrawEffectsBeforeUI", m_DrawEffectsBeforeUI);
		reshade::set_config_value(Runtime, NAME, "AutomaticDepthBufferSelection", m_AutomaticDepthBufferSelection);
	}

	void OnInitEffectRuntime(reshade::api::effect_runtime *Runtime)
	{
		if (auto type = Runtime->get_device()->get_api(); type != reshade::api::device_api::d3d12)
		{
			spdlog::error(
				"Unsupported configuration. Somehow a non-D3D12 device was passed to {}. Device type is: {:X}.",
				__FUNCTION__,
				static_cast<uint32_t>(type));

			return;
		}

		// Devices can have multiple swap chains which means multiple effect runtimes. I guess we'll have to
		// use the last one created.
		SetImplData(Runtime, __uuidof(EffectRuntimeConfiguration), new EffectRuntimeConfiguration(Runtime));
		SetImplData(Runtime->get_device(), IID_ReShadeEffectRuntime, Runtime);

		//
		// ReShade triggers execute_command_list events for each command list prior to calling ExecuteCommandLists.
		// These are okay enough when simply listening, but this plugin requires more control over the execution
		// order. Effects need to be rendered between or after command list submissions. ReShade doesn't offer such
		// functionality.
		//
		// That leaves two options:
		// 1. Hook the command queue's ExecuteCommandLists, split up submission groups, and inject our own lists.
		// 2. Hook game code to split up submission groups and then inject our own lists.
		//
		// Option 1 is implemented.
		//
		auto nativeD3D12Device = reinterpret_cast<ID3D12Device *>(Runtime->get_device()->get_native());
		auto wrappedD3D12Device = GetImplData<ID3D12Device *>(nativeD3D12Device, IID_ReShadeD3D12DevicePrivateData);

		// Create a graphics command queue using ReShade's wrapped device which should hopefully return a
		// vtable pointing to ReShade's implementation. If it doesn't, it means third party code likely added
		// their own hooks. We'll have to skip integrity checks and blindly hook its vtable.
		const static bool once = [&]
		{
			const D3D12_COMMAND_QUEUE_DESC queueDesc = {
				.Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
				.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
				.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
				.NodeMask = 0,
			};
			CComPtr<ID3D12CommandQueue> commandQueue;

			if (SUCCEEDED(wrappedD3D12Device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue))))
			{
				const auto vtableBase = *reinterpret_cast<uintptr_t *>(commandQueue.Get());

				return Hooks::WriteVirtualFunction(
					vtableBase,
					10,
					&HookedD3D12CommandQueueExecuteCommandLists,
					&D3D12CommandQueueExecuteCommandLists);
			}

			return false;
		}();

		if (!once)
			spdlog::error("ReShade initialized, but we failed to hook the D3D12 command queue. Crash imminent.");
	}

	void OnDestroyEffectRuntime(reshade::api::effect_runtime *Runtime)
	{
		SetImplData(Runtime->get_device(), IID_ReShadeEffectRuntime, nullptr);
		delete GetImplData<EffectRuntimeConfiguration *>(Runtime, __uuidof(EffectRuntimeConfiguration));
	}

	void OnInitCommandList(reshade::api::command_list *CommandList)
	{
		reinterpret_cast<ID3D12ReShadeGraphicsCommandList *>(CommandList->get_native())->Init(CommandList);
	}

	void OnDestroyCommandList(reshade::api::command_list *CommandList)
	{
		reinterpret_cast<ID3D12ReShadeGraphicsCommandList *>(CommandList->get_native())->Destroy();
	}

	void OnDrawSettingsOverlay(reshade::api::effect_runtime *Runtime)
	{
		auto effectConfig = GetImplData<EffectRuntimeConfiguration *>(Runtime, __uuidof(EffectRuntimeConfiguration));
		bool updated = false;

		updated |= ImGui::Checkbox("Draw ReShade effects before UI", &effectConfig->m_DrawEffectsBeforeUI);
		updated |= ImGui::Checkbox("Automatic depth buffer selection", &effectConfig->m_AutomaticDepthBufferSelection);

		if (effectConfig->m_AutomaticDepthBufferSelection)
			ImGui::TextColored({ 1.0f, 0.0f, 0.0f, 1.0f }, "Warning: Generic Depth must be disabled while automatic depth is in use.");

		if (updated)
			effectConfig->Save(Runtime);
	}

	void Initialize()
	{
		if (!reshade::register_addon(static_cast<HMODULE>(Plugin::GetThisModuleHandle())))
			return;

		reshade::register_overlay(nullptr, OnDrawSettingsOverlay);
		reshade::register_event<reshade::addon_event::init_effect_runtime>(OnInitEffectRuntime);
		reshade::register_event<reshade::addon_event::destroy_effect_runtime>(OnDestroyEffectRuntime);
		reshade::register_event<reshade::addon_event::init_command_list>(OnInitCommandList);
		reshade::register_event<reshade::addon_event::destroy_command_list>(OnDestroyCommandList);

		spdlog::info("Registered ReShade addon.");
	}

	void(WINAPI *D3D12CommandQueueExecuteCommandLists)(ID3D12CommandQueue *, UINT, ID3D12CommandList *const *);
	void WINAPI HookedD3D12CommandQueueExecuteCommandLists(ID3D12CommandQueue *This, UINT NumCommandLists, ID3D12CommandList *const *ppCommandLists)
	{
		// Zero matches => forward to original function by default
		uint32_t i = NumCommandLists;
		uint32_t start = 0;

		if (ID3D12ReShadeGraphicsCommandList::SplitCommandListCount != 0)
		{
			// Batch prior command list submissions together until a split is required
			for (i = 0; i < NumCommandLists; i++)
			{
				auto callback = static_cast<ID3D12ReShadeGraphicsCommandList *>(ppCommandLists[i])->PopPendingPostSubmitCallback();

				if (callback)
				{
					if (i > start)
						D3D12CommandQueueExecuteCommandLists(This, i - start, &ppCommandLists[start]);

					D3D12CommandQueueExecuteCommandLists(This, 1, &ppCommandLists[i]);
					start = i + 1;

					(*callback)(This);
					delete callback;
				}
			}
		}

		if (i > start)
			D3D12CommandQueueExecuteCommandLists(This, i - start, &ppCommandLists[start]);
	}

	void (*OriginalUpdatePreviousDepthBufferRenderPass)(void *, void *, void *);
	void HookedUpdatePreviousDepthBufferRenderPass(void *a1, void *a2, void *a3)
	{
		OriginalUpdatePreviousDepthBufferRenderPass(a1, a2, a3);

		auto commandList = static_cast<ID3D12ReShadeGraphicsCommandList *>(CreationRenderer::GetRenderGraphCommandList(a2));
		auto reshadeInterface = commandList->GetReShadeInterface();

		if (!reshadeInterface)
			return;

		auto sourceImage = CreationRenderer::AcquireRenderPassIO(a3, 1);

		// If automatic depth buffer selection is enabled, we need to copy the game's depth buffer to a
		// separate texture so that ReShade can use it in effects. ReShade's API can handle resource
		// creation, but the copy has to be done on the game's command list.
		auto effectRuntime = GetImplData<reshade::api::effect_runtime *>(reshadeInterface->get_device(), IID_ReShadeEffectRuntime);
		auto effectConfig = GetImplData<EffectRuntimeConfiguration *>(effectRuntime, __uuidof(EffectRuntimeConfiguration));

		if (effectConfig->m_AutomaticDepthBufferSelection)
		{
			auto device = reshadeInterface->get_device();

			// First determine if the depth buffer format changed between frames
			const auto depthResource = device->get_resource_from_view({ sourceImage->m_RTVCpuDescriptors[0].ptr });
			const auto depthResourceDesc = device->get_resource_desc(depthResource);

			auto copyInfo = [&]()
			{
				const EffectDepthCopy info = {
					.Format = depthResourceDesc.texture,
					.LastFrameIndex = effectConfig->DepthTrackingFrameIndex.fetch_add(1) + 1,
				};

				std::scoped_lock lock(effectConfig->DepthBufferListMutex);
				auto itr = std::find_if(
					effectConfig->DepthBufferCopies.begin(),
					effectConfig->DepthBufferCopies.end(),
					[&](const auto& Copy)
					{
						return Copy.Format.width == info.Format.width && Copy.Format.height == info.Format.height &&
							   Copy.Format.depth_or_layers == info.Format.depth_or_layers && Copy.Format.levels == info.Format.levels &&
							   Copy.Format.format == info.Format.format;
					});

				if (itr == effectConfig->DepthBufferCopies.end())
					return info;

				itr->LastFrameIndex = info.LastFrameIndex;
				return *itr;
			}();

			// A null handle means a suitable copy texture hasn't been created yet. Create one now.
			if (!copyInfo.Resource.handle)
			{
				auto copyResourceDesc = depthResourceDesc;
				copyResourceDesc.heap = reshade::api::memory_heap::gpu_only;
				copyResourceDesc.usage = reshade::api::resource_usage::depth_stencil | reshade::api::resource_usage::shader_resource |
										 reshade::api::resource_usage::copy_dest;

				auto copyViewDesc = device->get_resource_view_desc({ sourceImage->m_RTVCpuDescriptors[0].ptr });
				copyViewDesc.format = reshade::api::format_to_default_typed(copyViewDesc.format);

				// We only need a texture and its shader resource view; stencils don't matter
				device->create_resource(copyResourceDesc, nullptr, reshade::api::resource_usage::shader_resource, &copyInfo.Resource);
				device->create_resource_view(
					copyInfo.Resource,
					reshade::api::resource_usage::shader_resource,
					copyViewDesc,
					&copyInfo.ResourceView);

				std::scoped_lock lock(effectConfig->DepthBufferListMutex);
				effectConfig->DepthBufferCopies.emplace_back(copyInfo);
			}

			// Using the game's command list, copy the game's final depth buffer to our copy texture
			reshadeInterface->barrier(
				copyInfo.Resource,
				reshade::api::resource_usage::shader_resource,
				reshade::api::resource_usage::copy_dest);
			reshadeInterface->copy_resource(depthResource, copyInfo.Resource);
			reshadeInterface->barrier(
				copyInfo.Resource,
				reshade::api::resource_usage::copy_dest,
				reshade::api::resource_usage::shader_resource);

			commandList->QueuePostSubmitCallback(
				[device, effectRuntime, effectConfig, index = copyInfo.LastFrameIndex, view = copyInfo.ResourceView](ID3D12CommandQueue *Queue)
				{
					auto nativeDevice = reinterpret_cast<ID3D12Device *>(device->get_native());
					
					// Schedule a fence signal to release old copies
					if (!effectConfig->DepthTrackingFence)
						nativeDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&effectConfig->DepthTrackingFence));

					Queue->Signal(effectConfig->DepthTrackingFence.Get(), index);
					const auto currentFenceIndex = effectConfig->DepthTrackingFence->GetCompletedValue();

					// Tell ReShade about our new depth buffer
					if (std::exchange(effectConfig->UpdateHint, view) != view)
					{
						effectRuntime->update_texture_bindings("DEPTH", view);

						effectRuntime->enumerate_uniform_variables(
							nullptr,
							[](auto Runtime, auto Variable)
							{
								char source[32] = {};
								if (Runtime->get_annotation_string_from_uniform_variable(Variable, "sourceImage", source) &&
									std::strcmp(source, "bufready_depth") == 0)
									Runtime->set_uniform_value_bool(Variable, true);
							});
					}

					std::lock_guard lock(effectConfig->DepthBufferListMutex);
					std::erase_if(
						effectConfig->DepthBufferCopies,
						[&](const auto& Copy)
						{
							if (Copy.LastFrameIndex >= currentFenceIndex)
								return false;

							device->destroy_resource_view(Copy.ResourceView);
							device->destroy_resource(Copy.Resource);
							return true;
						});
				});
		}
	}

	void (*OriginalScaleformCompositeDrawPass)(void *, void *, void *);
	void HookedScaleformCompositeDrawPass(void *a1, void *a2, void *a3)
	{
		auto commandList = static_cast<ID3D12ReShadeGraphicsCommandList *>(CreationRenderer::GetRenderGraphCommandList(a2));
		auto reshadeInterface = commandList->GetReShadeInterface();

		if (reshadeInterface)
		{
			// Tell ReShade to render effects before this UI command list is submitted. A separate command
			// list is required because of state tracking reasons.
			auto effectRuntime = GetImplData<reshade::api::effect_runtime *>(reshadeInterface->get_device(), IID_ReShadeEffectRuntime);
			auto effectConfig = GetImplData<EffectRuntimeConfiguration *>(effectRuntime, __uuidof(EffectRuntimeConfiguration));

			if (effectConfig->m_DrawEffectsBeforeUI)
			{
				auto renderTarget = CreationRenderer::AcquireRenderPassIO(a3, 1);

				// Reshade's effect runtime clobbers command list state but the game resets everything when drawing the
				// scaleform pass anyway
				effectRuntime->render_effects(reshadeInterface, { renderTarget->m_RTVCpuDescriptors[0].ptr });
			}
		}

		OriginalScaleformCompositeDrawPass(a1, a2, a3);
	}

	DECLARE_HOOK_TRANSACTION(ReShadeHelper)
	{
		Hooks::WriteJump(
			Offsets::Signature(
				"48 89 5C 24 18 55 56 57 41 55 41 56 48 8D 6C 24 F0 48 81 EC 10 01 00 00"),
			&HookedScaleformCompositeDrawPass,
			&OriginalScaleformCompositeDrawPass);

		Hooks::WriteJump(
			Offsets::Signature("48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 50 48 8B CA 49 8B D8"),
			&HookedUpdatePreviousDepthBufferRenderPass,
			&OriginalUpdatePreviousDepthBufferRenderPass);
	};
}
