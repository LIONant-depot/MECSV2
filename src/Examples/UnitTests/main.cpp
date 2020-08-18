
#define NOMINMAX
#include <windows.h>
#include <memoryapi.h>
#include "mecs.h"

#include "T0-functionality_queries.h"

//------------------------------------------------------------------------------------
// main
//------------------------------------------------------------------------------------
int main(void)
{
    xcore::Init("Engine Programming");

    if(1) mecs::unit_test::functionality::queries::Test();


    xcore::Kill();
    return 0;
}