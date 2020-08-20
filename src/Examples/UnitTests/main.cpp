
#define NOMINMAX
#include <windows.h>
#include <memoryapi.h>
#include "mecs.h"

#include "T0_functionality_queries.h"
#include "T1_functionality_simple.h"

//------------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------------
int main(void)
{
    xcore::Init("Engine Programming");

    if(0) mecs::unit_test::functionality::queries::Test();
    if(1) mecs::unit_test::functionality::simple::Test();

    xcore::Kill();
    return 0;
}