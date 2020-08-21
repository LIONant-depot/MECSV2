namespace mecs::system
{
    xforceinline
    instance::instance(const construct&& Settings) noexcept
        : overrites{ Settings.m_JobDefinition | xcore::scheduler::triggers::DONT_CLEAN_COUNT }
        , m_World{ Settings.m_World }
        , m_Guid{ Settings.m_Guid }
    { setupName(Settings.m_Name); }


    template< typename T_CALLBACK > constexpr xforceinline
    void instance::ForEach(mecs::archetype::query::instance& QueryI, T_CALLBACK&& Functor, int nEntitiesPerJob) noexcept
    {
        // If there is nothing to do then lets bail out
        if (QueryI.m_lResults.size() == 0)
            return;

        //
        // Check for graph errors by Locking the relevant components in the archetypes
        //
        if (mecs::archetype::details::safety::LockQueryComponents(*this, QueryI))
        {
            // we got an error here
            xassert(false);
        }

        //
        // Loop through all the results and lets the system deal with the components
        //
        xcore::scheduler::channel       Channel(getName());
        using                           params_t = details::process_resuts_params<T_CALLBACK>;
        params_t                        Params
        {
            Channel
            , std::forward<T_CALLBACK>(Functor)
            , nEntitiesPerJob
        };

        for (auto& R : QueryI.m_lResults)
        {
            auto& MainPool = R.m_pArchetype->m_MainPool;
            xassert(MainPool.size());
            for (int end = static_cast<int>(MainPool.size()), i = 0; i < end; ++i)
            {
                ProcessResult(Params, R, MainPool, i);
            }
        }

        Channel.join();
    }

    template< typename...T_SHARE_COMPONENTS > xforceinline
    mecs::archetype::entity_creation instance::createEntities(mecs::archetype::instance::guid gArchetype, int nEntities, std::span<entity::guid> gEntitySpan, T_SHARE_COMPONENTS&&... ShareComponents) noexcept
    {
        auto& Archetype = m_World.m_ArchetypeDB.getArchetype( gArchetype );
        return Archetype.CreateEntities( *this, nEntities, gEntitySpan, std::forward<T_SHARE_COMPONENTS>(ShareComponents)...);
    }

    inline
    void instance::deleteEntity(mecs::component::entity& Entity )
    {
        XCORE_PERF_ZONE_SCOPED()
        if (Entity.isZombie()) return;
        Entity.getReference().m_pPool->m_pArchetypeInstance->deleteEntity(*this, Entity);
    }

}