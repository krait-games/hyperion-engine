#ifndef ENUM_OPTIONS_H
#define ENUM_OPTIONS_H

#include "../hash_code.h"
#include "../math/math_util.h"
#include "../util.h"

#include <array>
#include <initializer_list>

namespace hyperion {

template <typename EnumType, typename ValueType, size_t Sz>
class EnumOptions {
public:
    using EnumOption_t = EnumType;
    using Ordinal_t = uint64_t;
    using EnumValueArray_t = std::array<ValueType, Sz>;
    using EnumValuePair_t = std::pair<EnumType, ValueType>;

    // convert from attachment (2^x) into ordinal (0-5) for use as an array index
    static inline constexpr uint64_t EnumToOrdinal(uint64_t option)
        { return MathUtil::FastLog2_Pow2(option); }

    // convert from ordinal (0-5) into power-of-two for use as bit flags
    static inline constexpr uint64_t OrdinalToEnum(uint64_t ordinal)
        { return 1ULL << ordinal; }

    static_assert(Sz != 0, "EnumOptions cannot have size of zero");
    static_assert(
        OrdinalToEnum(Sz - 1) < MathUtil::MaxSafeValue<EnumType>(),
        "Size too large; enum conversion would cause overflow. "
        "Try changing the enum's underlying type to a larger sized data type?"
    );

    EnumOptions() : m_values{} {}
    EnumOptions(const EnumOptions &other) : m_values(other.m_values) {}

    EnumOptions &operator=(const EnumOptions &other)
    {
        m_values = other.m_values;

        return *this;
    }

    EnumOptions(EnumOptions &&other) : m_values(std::move(other.m_values)) {}

    EnumOptions &operator=(EnumOptions &&other) noexcept
    {
        m_values = std::move(other.m_values);

        return *this;
    }

    EnumOptions(const EnumValueArray_t &array) : m_values(array) {}

    EnumOptions(const std::vector<EnumValuePair_t> &pairs)
        : m_values{}
    {
        for (const auto &item : pairs) {
            Set(item.first, item.second);
        }
    }

    ~EnumOptions() = default;

    inline constexpr EnumValuePair_t KeyValueAt(size_t index) const
        { return std::make_pair(EnumOption_t(OrdinalToEnum(index)), m_values[index]); }

    inline constexpr EnumType KeyAt(size_t index) const
        { return OrdinalToEnum(index); }

    inline constexpr ValueType &ValueAt(size_t index)
        { return m_values[index]; }

    inline constexpr const ValueType &ValueAt(size_t index) const
        { return m_values[index]; }

    inline constexpr ValueType &Get(EnumOption_t enum_key)
        { return m_values[EnumToOrdinal(enum_key)]; }

    inline constexpr const ValueType &Get(EnumOption_t enum_key) const
        { return m_values[EnumToOrdinal(enum_key)]; }

    inline constexpr ValueType &operator[](EnumOption_t enum_key)
        { return m_values[EnumToOrdinal(enum_key)]; }

    inline constexpr const ValueType &operator[](EnumOption_t enum_key) const
        { return m_values[EnumToOrdinal(enum_key)]; }

    inline EnumOptions &Set(EnumOption_t enum_key, const ValueType &value)
    {
        auto ord = EnumToOrdinal(enum_key);

        AssertThrow(ord < m_values.size());

        m_values[ord] = value;

        return *this;
    }

    inline EnumOptions &Unset(EnumOption_t enum_key)
    {
        auto ord = EnumToOrdinal(enum_key);

        AssertThrow(ord < m_values.size());

        m_values[ord] = ValueType();

        return *this;
    }

    inline constexpr size_t Size() const
        { return m_values.size(); }

    inline HashCode GetHashCode() const
    {
        HashCode hc;

        for (const auto &it : m_values) {
            hc.Add(it.GetHashCode());
        }

        return hc;
    }

private:
    EnumValueArray_t m_values;
};

} // namespace hyperion

#endif