#include "test_framework.hpp"

int main()
{
    return smart::test::Registry::instance().run_all();
}
