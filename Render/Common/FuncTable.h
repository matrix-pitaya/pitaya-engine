#pragma once

#include<Core/PassKey/PassKey.h>
#include<Context/Context.h>
#include<Render/Bake-Input/IBLBakeInput.h>
#include<stdexcept>

namespace Pitaya::Engine
{
	template<>
	struct FuncTable<Pitaya::Render::Renderer>
	{
		friend class Pitaya::Engine::Engine;
	private:
		FuncTable() = default;
		~FuncTable() = default;

	public:
		FuncTable(const FuncTable&) = delete;
		FuncTable& operator=(const FuncTable&) = delete;
		FuncTable(FuncTable&&) = delete;
		FuncTable& operator=(FuncTable&&) = delete;

	public:
		inline bool Verify() const
		{
			if (!OnBakeIBL) { throw std::runtime_error("FuncTable miss [Render::OnBakeIBL] Function!"); }
			if (!OnWakeupAssetUpload) { throw std::runtime_error("FuncTable miss [Render::OnWakeupAssetUpload] Function!"); }
				return true;
		}
		inline void Nullify() noexcept
		{
			OnBakeIBL = nullptr;
			OnWakeupAssetUpload = nullptr;
		}

	public:
		inline bool InvokeOnBakeIBL(const Pitaya::Render::IBLBakeInput& input)
		{
			return OnBakeIBL(input);
		}
		inline void InvokeOnWakeupAssetUpload() noexcept
		{
			OnWakeupAssetUpload();
		}

	private:
		bool (ENGINE_CALL *OnBakeIBL)(const Pitaya::Render::IBLBakeInput&) = nullptr;
		void (ENGINE_CALL *OnWakeupAssetUpload)() noexcept = nullptr;
	};
}

namespace Pitaya::Render
{
	inline bool BakeIBL(const IBLBakeInput& input)
	{
		return Pitaya::Engine::Context::Instance().GetFuncTable<Pitaya::Render::Renderer>().InvokeOnBakeIBL(input);
	}
	inline void WakeupAssetUpload() noexcept
	{
		Pitaya::Engine::Context::Instance().GetFuncTable<Pitaya::Render::Renderer>().InvokeOnWakeupAssetUpload();
	}
}
