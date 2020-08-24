
#define NOMINMAX
#include <windows.h>
#include <memoryapi.h>
#include "mecs.h"

#include "T0_functionality_queries.h"
#include "T1_functionality_simple.h"
#include "T2_functionality_spawner.h"
#include "T3_functionality_move.h"

//------------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------------
int main(void)
{
    xcore::Init("Engine Programming");

    if(0) mecs::unit_test::functionality::queries::Test();
    if(0) mecs::unit_test::functionality::simple::Test();
    if(0) mecs::unit_test::functionality::spawner::Test();
    if(1) mecs::unit_test::functionality::move::Test();

    xcore::Kill();
    return 0;
}