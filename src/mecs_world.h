namespace mecs::world
{
    using guid = xcore::guid::unit<64, struct world_tag >;

    struct instance
    {
        xforceinline instance( mecs::universe::instance& Universe ) noexcept : m_Universe{ Universe }, m_GraphDB{ *this, Universe }
        {
            m_ArchetypeDB.Init();
        }

        void Play(bool bContinuousPlay = false)
        {
            //
            // First make sure the archetypes are ready to start
            // This allows archetypes to update their double buffer components and such
            //
            if( false == m_GraphDB.m_bGraphStarted ) m_ArchetypeDB.Start();

            //
            // Lets start the graph
            //
            m_GraphDB.Play(bContinuousPlay);
        }

        void Stop( void )
        {
            m_GraphDB.Stop();
        }

        guid                            m_Guid              {};
        mecs::universe::instance&       m_Universe;
        mecs::archetype::data_base      m_ArchetypeDB       {};
        mecs::graph::instance           m_GraphDB;
    };
}
