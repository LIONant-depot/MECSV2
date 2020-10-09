
namespace mecs::tools
{
    namespace hash
    {
        template<typename...T_ARGS >
        xforceinline constexpr
        std::uint64_t combine( std::uint64_t Hash = 0u )
        {
            return Hash;
        }

        template<typename...T_ARGS >
        xforceinline constexpr
        std::uint64_t combine( std::uint64_t Hash1, const std::uint64_t Hash2, T_ARGS... Args )
        {
            Hash1 ^= Hash2 + 0x9e3779b9u + (Hash1 << 6) + (Hash1 >> 2);
            if constexpr (!!sizeof...(T_ARGS)) return combine( Hash1, Args... );
            else                               return Hash1;
        }

        template<typename T, typename...T_ARGS >
        xforceinline constexpr
        std::uint64_t hash_and_combine( std::uint64_t Seed, const T Value, T_ARGS... Args )
        {
            std::hash<T> Hasher;
            return combine( Seed, Hasher(Value), Args... );
        }
    }
}