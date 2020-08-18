namespace mecs::tools
{
    //---------------------------------------------------------------------------------
    // TOOLS::bits
    //---------------------------------------------------------------------------------
    struct bits
    {
        std::array<std::uint64_t,2> m_Bits;

        constexpr bits()=default;
        constexpr bits(std::nullptr_t)    noexcept : m_Bits{ 0ull, 0ull}{}
        constexpr bits(xcore::not_null_t) noexcept : m_Bits{~0ull,~0ull}{}

        xforceinline constexpr int AllocBit( void ) noexcept
        {
            for( auto& E : m_Bits ) 
            {
                const auto Index = xcore::bits::ctz64(E);
                if(Index != 64 )
                {
                    E &= ~(1ull<<Index);
                    return Index;
                }
            }

            xassert(false);
            return -1;
        }

        xforceinline void AddBit( int Index ) noexcept
        {
            xassert(Index >=0);
            m_Bits[Index/64] |= (1ull<<(Index%64));
        }

        xforceinline void Add( bits Bits ) noexcept
        {
            for( int i=0; i<m_Bits.size(); ++i ) m_Bits[i] |= Bits.m_Bits[i];
        }
        xforceinline constexpr bool getBit( int Index ) const noexcept
        {
            return !!((m_Bits[Index/64] >> (Index%64))&1);
        }

        xforceinline constexpr bool isMatchingBits( const bits& Match ) const noexcept
        {
            for( int i=0; i<m_Bits.size(); ++i )
            {
                if( m_Bits[i] & Match.m_Bits[i] ) return true;
            }
            return false;
        }

        xforceinline void clear( void ) noexcept
        {
            for( int i=0; i<m_Bits.size(); ++i )
            {
                m_Bits[i] = 0;
            }
        }

        xforceinline constexpr bool Query( bits All, bits Any, bits None ) const noexcept
        {
            bool    bAny1 = false;
            bool    bAny2 = false;
            for( int i=0; i<m_Bits.size(); ++i )
            { 
                const auto B = m_Bits[i];
                if( None.m_Bits[i] & B ) return false;
                if( const auto A = All.m_Bits[i]; (A&B) != A ) return false;
                if( const auto A = Any.m_Bits[i]; (A&B) ) bAny1 = true;
                else bAny2 = A != 0;
            }
            return bAny1 || bAny2 == bAny1;
        }
    };
}
