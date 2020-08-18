
#include "T01_simple.h"
#include "T02_spawner.h"
#include "T03_simple_v2.h"
#include "T04_tutorial.h"
#include "T05_memory.h"

namespace mecs::tests
{
    void Test( void )
    {
        mecs::tests::T01_simple::Test();
        mecs::tests::T02_spawner::Test();
        mecs::tests::T03_simple_v2::Test();
        mecs::tests::T04_tutorial::Test();
        mecs::tests::T05_memory::Test();
    }
}