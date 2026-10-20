#pragma once

#include<Core/Utils/StateFlags.h>

#include<cstdint>
#include<type_traits>

namespace Pitaya::Core
{
    template <typename TEnum>
    class StateFlags
    {
        static_assert(std::is_enum_v<TEnum>, "TEnum must be an enum type!");
        static_assert(std::is_same_v<std::underlying_type_t<TEnum>, uint8_t> || std::is_same_v<std::underlying_type_t<TEnum>, uint16_t> ||
            std::is_same_v<std::underlying_type_t<TEnum>, uint32_t> || std::is_same_v<std::underlying_type_t<TEnum>, uint64_t>,
            "Only support uint8_t/uint16_t/uint32_t/uint64_t as underlying type!");

    public:
        constexpr StateFlags() noexcept = default;
        constexpr StateFlags(TEnum initial) noexcept
            :state(static_cast<std::underlying_type_t<TEnum>>(initial)) {}
        ~StateFlags() noexcept = default;

    public:
        StateFlags(const StateFlags&) noexcept = default;
        StateFlags& operator=(const StateFlags&) noexcept = default;
        StateFlags(StateFlags&&) noexcept = default;
        StateFlags& operator=(StateFlags&&) noexcept = default;

    public:
        StateFlags& operator=(TEnum value) noexcept
        {
            state = static_cast<std::underlying_type_t<TEnum>>(value);
            return *this;
        }

    public:
        bool operator==(TEnum value) noexcept
        {
            return state == static_cast<std::underlying_type_t<TEnum>>(value);
        }
        bool operator!=(TEnum value) noexcept
        {
            return state != static_cast<std::underlying_type_t<TEnum>>(value);
        }
        bool operator==(TEnum value) const noexcept
        {
            return state == static_cast<std::underlying_type_t<TEnum>>(value);
        }
        bool operator!=(TEnum value) const noexcept
        {
            return state != static_cast<std::underlying_type_t<TEnum>>(value);
        }

    public:
        inline void SetBits(TEnum mask) noexcept
        {
            state |= static_cast<std::underlying_type_t<TEnum>>(mask);
        }
        template <typename... Args>
        inline void SetBits(Args&&... args) noexcept
        {
            static_assert(sizeof...(Args) > 0, "SetBits requires at least one argument!");
            static_assert((std::is_same_v<std::decay_t<Args>, TEnum> && ...),
                "All arguments of SetBits must be of type TEnum!");

            state |= Pitaya::Core::CombineMask<TEnum>(std::forward<Args>(args)...);
        }
        inline void ClearBits(TEnum mask) noexcept
        {
            state &= ~static_cast<std::underlying_type_t<TEnum>>(mask);
        }
        inline void ModifyBits(TEnum setMask, TEnum clearMask) noexcept
        {
            state &= ~static_cast<std::underlying_type_t<TEnum>>(clearMask);
            state |= static_cast<std::underlying_type_t<TEnum>>(setMask);
        }
        template <typename... Args>
        inline void ModifyBits(TEnum setMask, Args&&... clearMasks) noexcept
        {
            static_assert(sizeof...(Args) > 0, "ModifyBits requires at least one clear mask!");
            static_assert((std::is_same_v<std::decay_t<Args>, TEnum> && ...),
                "All clear masks of ModifyBits must be of type TEnum!");

            const std::underlying_type_t<TEnum> clearMask =
                Pitaya::Core::CombineMask<TEnum>(std::forward<Args>(clearMasks)...);
            state = (state & ~clearMask) | static_cast<std::underlying_type_t<TEnum>>(setMask);
        }
        inline void OverwriteBits(TEnum value) noexcept
        {
            state = static_cast<std::underlying_type_t<TEnum>>(value);
        }
        inline bool HasBits(TEnum mask) const noexcept
        {
            return (state & static_cast<std::underlying_type_t<TEnum>>(mask)) != 0;
        }
        template <typename... Args>
        inline bool HasAnyBits(Args&&... args) const noexcept
        {
            static_assert(sizeof...(Args) > 0, "HasAnyBits requires at least one argument!");
            static_assert((std::is_same_v<std::decay_t<Args>, TEnum> && ...),
                "All arguments of HasAnyBits must be of type TEnum!");

            const std::underlying_type_t<TEnum> mask =
                Pitaya::Core::CombineMask<TEnum>(std::forward<Args>(args)...);
            return (state & mask) != 0;
        }
        inline bool HasAllBits(TEnum mask) const noexcept
        {
            return (state & static_cast<std::underlying_type_t<TEnum>>(mask)) == static_cast<std::underlying_type_t<TEnum>>(mask);
        }
        template <typename... Args>
        inline bool HasAllBits(Args&&... args) const noexcept
        {
            static_assert(sizeof...(Args) > 0, "HasAllBits requires at least one argument!");
            static_assert((std::is_same_v<std::decay_t<Args>, TEnum> && ...),
                "All arguments of HasAllBits must be of type TEnum!");

            std::underlying_type_t<TEnum> combinedMask = Pitaya::Core::CombineMask<TEnum>(std::forward<Args>(args)...);
            return (state & combinedMask) == combinedMask;
        }
        inline bool HasAllBitsAndNone(TEnum requiredMask, TEnum forbiddenMask) const noexcept
        {
            return (state & static_cast<std::underlying_type_t<TEnum>>(requiredMask))
                == static_cast<std::underlying_type_t<TEnum>>(requiredMask)
                && (state & static_cast<std::underlying_type_t<TEnum>>(forbiddenMask)) == 0;
        }
        inline TEnum GetEnum() const noexcept
        {
            return static_cast<TEnum>(state);
        }
        inline std::underlying_type_t<TEnum> GetRawValue() const noexcept
        {
            return state;
        }

    private:
        std::underlying_type_t<TEnum> state = 0;
    };
}
