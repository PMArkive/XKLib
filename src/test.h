#ifndef ASURA_TEST_H
#define ASURA_TEST_H

#include "asura.h"

namespace Asura
{
    class Test
    {
      public:
        class APIPure
        {
          public:
            virtual void func1()                             = 0;
            virtual std::vector<int> func2(char const*, ...) = 0;
            virtual ~APIPure()                               = default;
        };

        class API : public APIPure
        {
          public:
            virtual void func1() override;
            virtual std::vector<int> func2(char const*, ...) override;
        };

      public:
        static auto run() -> void;
    };

    inline Test::API g_API;
    inline bool g_PassedTests = true;
}

#endif
