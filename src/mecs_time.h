namespace mecs
{
    //---------------------------------------------------------------------------------
    // WORLD::time
    //---------------------------------------------------------------------------------
    struct time
    {
        float                   m_DeltaTime                     = 0;
        float                   m_FixDeltaTime                  = static_cast<float>(settings::fixed_deltatime_microseconds_v / 1000000.0);
        float                   m_FixFrameExtrapolation         = 0;
        std::uint32_t           m_FixSteps                      = 0;
        std::uint32_t           m_FixDeltaTimeMicroseconds      = mecs::settings::fixed_deltatime_microseconds_v;
        std::uint32_t           m_FixMicrosecondsAccumulation   = 0;

        std::uint64_t               m_iFrame                    { 0     };
        std::array<std::uint64_t,8> m_DeltaTimeMicroseconds     { 0     };

        std::chrono::time_point<std::chrono::high_resolution_clock> m_Start;
        void clear(void) noexcept 
        {
            m_DeltaTime                     = 0;
            m_FixMicrosecondsAccumulation   = 0;
            m_FixFrameExtrapolation         = 0;
            m_FixSteps                      = 0;
        }

        void Start(void) noexcept
        {
            m_Start                         = std::chrono::high_resolution_clock::now();
            m_DeltaTime                     = 0;
            m_FixMicrosecondsAccumulation   = 0;
            m_FixFrameExtrapolation         = 0;
            m_FixSteps                      = 0;
        }

        void ChangeFixDeltaTimeMicroseconds( std::uint32_t Microseconds ) noexcept
        {
            m_FixDeltaTimeMicroseconds      = Microseconds;
            m_FixDeltaTime                  = static_cast<float>(m_FixDeltaTimeMicroseconds / 1000000.0);
        }

        void UpdateDeltaTimeRoundTrip(void) noexcept
        {
            const auto End                          = std::chrono::high_resolution_clock::now();
            const auto DeltaTimeInMicroseconds      = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(End - m_Start).count());

            m_Start         = End;

            if( constexpr static std::uint64_t max_steps = 5; DeltaTimeInMicroseconds >= (max_steps * m_FixDeltaTimeMicroseconds) )
            {
                m_FixMicrosecondsAccumulation   = 0;
                m_FixSteps                      = max_steps;
                m_DeltaTime                     = static_cast<float>(m_FixSteps * m_FixDeltaTime);
                m_FixFrameExtrapolation         = 0;
            }
            else
            {
                m_FixMicrosecondsAccumulation  += static_cast<uint32_t>(DeltaTimeInMicroseconds);
                m_FixSteps                      = 0;
                m_DeltaTime                     = DeltaTimeInMicroseconds / 1000000.0f;     // Converts to seconds

                while( m_FixMicrosecondsAccumulation >= m_FixDeltaTimeMicroseconds )
                {
                    m_FixSteps++;
                    m_FixMicrosecondsAccumulation -= m_FixDeltaTimeMicroseconds;
                }

                m_FixFrameExtrapolation = std::max( 1.0f, m_FixMicrosecondsAccumulation / static_cast<float>(m_FixDeltaTimeMicroseconds) );
            }

            m_DeltaTimeMicroseconds[(m_iFrame++)&(m_DeltaTimeMicroseconds.size()-1)] = DeltaTimeInMicroseconds;
        }

        std::uint64_t getLastFrameID( void ) const noexcept
        {
            return m_iFrame;
        }

        std::uint64_t getLastFrameMicroseconds( void ) const noexcept
        {
            return m_DeltaTimeMicroseconds[(m_iFrame-1)&(m_DeltaTimeMicroseconds.size()-1)];
        }

        double getAvgMicroseconds( void ) const noexcept
        {
            std::uint64_t Sum=0;
            for( auto F : m_DeltaTimeMicroseconds ) Sum += F;
            return m_DeltaTimeMicroseconds.size() / static_cast<double>( Sum ); 
        }

        float getAvgFPS( void ) const noexcept
        {
            return static_cast<float>(getAvgMicroseconds() * 1000000.0);
        }
    };
}