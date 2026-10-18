#pragma once

namespace Pitaya::Render
{
    class Renderer;
}

#include<Asset/Common/AssetOperate.h>

#include<Core/Container/ThreadSafe/ThreadSafeQueue.h>
#include<Core/PassKey/PassKey.h>
#include<Core/Utils/Check.h>
#include<Core/Utils/Time.h>

#include<Thread/Common/FuncTable.h>
#include<Thread/Common/ThreadType.h>

#include<Render/Common/FuncTable.h>

#include<chrono>
#include<queue>
#include<utility>
#include<variant>

namespace Pitaya::Asset
{
    class AssetHub;

    class GpuAssetBridge final
    {
        friend class AssetHub;

    private:
        GpuAssetBridge() = default;
        ~GpuAssetBridge() = default;

    public:
        GpuAssetBridge(const GpuAssetBridge&) = delete;
        GpuAssetBridge& operator=(const GpuAssetBridge&) = delete;
        GpuAssetBridge(GpuAssetBridge&&) = delete;
        GpuAssetBridge& operator=(GpuAssetBridge&&) = delete;

    private:
        inline void Enqueue(const Pitaya::Asset::AssetOperate& operate)
        {
            pendingOperations.Push(operate);
            Pitaya::Render::WakeupAssetUpload();
        }

        inline void Enqueue(Pitaya::Asset::AssetOperate&& operate)
        {
            pendingOperations.Push(std::move(operate));
            Pitaya::Render::WakeupAssetUpload();
        }

        template<typename Handler>
        inline void Process(Pitaya::Core::PassKey<Pitaya::Render::Renderer> passkey, Handler&& handler)
        {
            (void)passkey;
            PITAYA_CHECK(Pitaya::Thread::GetIsInThread(Pitaya::Thread::ThreadType::Render));
            if (renderOperations.empty()) { renderOperations = pendingOperations.PopN(5); }

            Pitaya::Core::InvokeWithTimeBudget(
                [this]() ->bool { return renderOperations.empty(); },
                [this, &handler]()
                {
                    std::visit(handler, renderOperations.front().Data);
                    renderOperations.pop();
                },
                std::chrono::milliseconds(2));
        }

        inline bool HasPending(Pitaya::Core::PassKey<Pitaya::Render::Renderer> passkey) const
        {
            (void)passkey;
            return !renderOperations.empty() || !pendingOperations.Empty();
        }

    private:
        Pitaya::Core::ThreadSafeQueue<Pitaya::Asset::AssetOperate> pendingOperations;
        std::queue<Pitaya::Asset::AssetOperate> renderOperations;
    };
}
