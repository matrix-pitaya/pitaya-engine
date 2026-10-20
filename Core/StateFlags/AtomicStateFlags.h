#pragma once

#include<Core/Utils/StateFlags.h>

#include<atomic>
#include<type_traits>
#include<cstdint>

namespace Pitaya::Core
{
	template <typename TEnum>
	class AtomicStateFlags
	{
        static_assert(std::is_enum_v<TEnum>, "TEnum must be an enum type!");
        static_assert(std::is_same_v<std::underlying_type_t<TEnum>, uint8_t> || std::is_same_v<std::underlying_type_t<TEnum>, uint16_t> ||
            std::is_same_v<std::underlying_type_t<TEnum>, uint32_t> || std::is_same_v<std::underlying_type_t<TEnum>, uint64_t>,
            "Only support uint8_t/uint16_t/uint32_t/uint64_t as underlying type!");
	
    public:
        AtomicStateFlags() noexcept = default;
        AtomicStateFlags(TEnum initial) noexcept
            :atomicState(static_cast<std::underlying_type_t<TEnum>>(initial)) {}
        ~AtomicStateFlags() noexcept = default;

    public:
		AtomicStateFlags(const AtomicStateFlags&) = delete;
		AtomicStateFlags& operator=(const AtomicStateFlags&) = delete;
		AtomicStateFlags(AtomicStateFlags&&) = delete;
        AtomicStateFlags& operator=(AtomicStateFlags&&) = delete;

    public:
        AtomicStateFlags& operator=(TEnum value) noexcept
        {
            atomicState.store(static_cast<std::underlying_type_t<TEnum>>(value), std::memory_order_release);
            return *this;
        }

    public:
        bool operator==(TEnum value) noexcept
        {
            return atomicState.load(std::memory_order_acquire) == static_cast<std::underlying_type_t<TEnum>>(value);
        }
        bool operator!=(TEnum value) noexcept
        {
            return atomicState.load(std::memory_order_acquire) != static_cast<std::underlying_type_t<TEnum>>(value);
        }
        bool operator==(TEnum value) const noexcept
        {
            return atomicState.load(std::memory_order_acquire) == static_cast<std::underlying_type_t<TEnum>>(value);
        }
        bool operator!=(TEnum value) const noexcept
        {
            return atomicState.load(std::memory_order_acquire) != static_cast<std::underlying_type_t<TEnum>>(value);
        }

	public:
        inline void SetBits(TEnum mask) noexcept
        {
            atomicState.fetch_or(static_cast<std::underlying_type_t<TEnum>>(mask), std::memory_order_acq_rel);
        }
        template <typename... Args>
        inline void SetBits(Args&&... args) noexcept
        {
            static_assert(sizeof...(Args) > 0, "SetBits requires at least one argument!");
            static_assert((std::is_same_v<std::decay_t<Args>, TEnum> && ...),
                "All arguments of SetBits must be of type TEnum!");

            atomicState.fetch_or(Pitaya::Core::CombineMask<TEnum>(std::forward<Args>(args)...), std::memory_order_acq_rel);
        }
        inline void ClearBits(TEnum mask) noexcept
        {
            atomicState.fetch_and(~static_cast<std::underlying_type_t<TEnum>>(mask), std::memory_order_acq_rel);
        }
        inline void ModifyBits(TEnum setMask, TEnum clearMask) noexcept
        {
            std::underlying_type_t<TEnum> expectedState = atomicState.load(std::memory_order_relaxed);
            while (!atomicState.compare_exchange_weak(expectedState, (expectedState & ~static_cast<std::underlying_type_t<TEnum>>(clearMask)) | static_cast<std::underlying_type_t<TEnum>>(setMask),
                std::memory_order_acq_rel, std::memory_order_relaxed));
        }
        template <typename... Args>
        inline void ModifyBits(TEnum setMask, Args&&... clearMasks) noexcept
        {
            static_assert(sizeof...(Args) > 0, "ModifyBits requires at least one clear mask!");
            static_assert((std::is_same_v<std::decay_t<Args>, TEnum> && ...),
                "All clear masks of ModifyBits must be of type TEnum!");

            const std::underlying_type_t<TEnum> clearMask =
                Pitaya::Core::CombineMask<TEnum>(std::forward<Args>(clearMasks)...);
            std::underlying_type_t<TEnum> expectedState = atomicState.load(std::memory_order_relaxed);
            while (!atomicState.compare_exchange_weak(expectedState,
                (expectedState & ~clearMask) | static_cast<std::underlying_type_t<TEnum>>(setMask),
                std::memory_order_acq_rel, std::memory_order_relaxed));
        }
        inline void OverwriteBits(TEnum targetValue) noexcept
        {
            atomicState.store(static_cast<std::underlying_type_t<TEnum>>(targetValue), std::memory_order_release);
        }
        inline bool HasBits(TEnum mask) const noexcept
        {
            return (atomicState.load(std::memory_order_acquire) & static_cast<std::underlying_type_t<TEnum>>(mask)) != 0;
        }
        template <typename... Args>
        inline bool HasAnyBits(Args&&... args) const noexcept
        {
            static_assert(sizeof...(Args) > 0, "HasAnyBits requires at least one argument!");
            static_assert((std::is_same_v<std::decay_t<Args>, TEnum> && ...),
                "All arguments of HasAnyBits must be of type TEnum!");

            const std::underlying_type_t<TEnum> mask =
                Pitaya::Core::CombineMask<TEnum>(std::forward<Args>(args)...);
            return (atomicState.load(std::memory_order_acquire) & mask) != 0;
        }
        inline bool HasAllBits(TEnum mask) const noexcept
        {
            return (atomicState.load(std::memory_order_acquire) & static_cast<std::underlying_type_t<TEnum>>(mask)) == static_cast<std::underlying_type_t<TEnum>>(mask);
        }
        template <typename... Args>
        inline bool HasAllBits(Args&&... args) const noexcept
        {
            static_assert(sizeof...(Args) > 0, "HasAllBits requires at least one argument!");
            static_assert((std::is_same_v<std::decay_t<Args>, TEnum> && ...),
                "All arguments of HasAllBits must be of type TEnum!");

            std::underlying_type_t<TEnum> combinedMask = Pitaya::Core::CombineMask<TEnum>(std::forward<Args>(args)...);
            return (atomicState.load(std::memory_order_acquire) & combinedMask) == combinedMask;
        }
        inline bool HasAllBitsAndNone(TEnum requiredMask, TEnum forbiddenMask) const noexcept
        {
            const std::underlying_type_t<TEnum> state = atomicState.load(std::memory_order_acquire);
            return (state & static_cast<std::underlying_type_t<TEnum>>(requiredMask))
                == static_cast<std::underlying_type_t<TEnum>>(requiredMask)
                && (state & static_cast<std::underlying_type_t<TEnum>>(forbiddenMask)) == 0;
        }
        inline TEnum GetEnum() const noexcept
        {
            return static_cast<TEnum>(atomicState.load(std::memory_order_acquire));
        }
        inline std::underlying_type_t<TEnum> GetRawValue() const noexcept
        {
            return atomicState.load(std::memory_order_acquire);
        }

	private:
		std::atomic<std::underlying_type_t<TEnum>> atomicState = 0;
	};
}
