<img src="https://i.imgur.com/NwahbNn.jpg" align="right" width="250px" />

# MECS (Version: 0.2.0 Alpha)
<!---
[      ![pipeline status](https://gitlab.com/LIONant/properties/badges/master/pipeline.svg)](https://gitlab.com/LIONant/properties/commits/master)
[            ![Docs](https://img.shields.io/badge/docs-ready-brightgreen.svg)](https://gitlab.com/LIONant/properties/blob/master/docs/Documentation.md)
<br>
[          ![Clang C++17](https://img.shields.io/badge/clang%20C%2B%2B17-compatible-brightgreen.svg)]()
[            ![GCC C++17](https://img.shields.io/badge/gcc%20C%2B%2B17-compatible-brightgreen.svg)]()
--->
[   ![Visual Studio 2019](https://img.shields.io/badge/Visual%20Studio%202019-compatible-brightgreen.svg)]()
<!---
<br>
[            ![Platforms](https://img.shields.io/badge/Platforms-All%20Supported-blue.svg)]()
<br>
--->
[             ![Feedback](https://img.shields.io/badge/feedback-welcome-brightgreen.svg)](https://gitlab.com/LIONant/properties/issues)
[![Contributions welcome](https://img.shields.io/badge/contributions-welcome-brightgreen.svg)](https://gitlab.com/LIONant/properties)
[              ![License](https://img.shields.io/badge/license-MIT-blue.svg)](https://opensource.org/licenses/MIT)

## (MECS) Mulicore Events Component and Systems
Simple yet powerful **ECS** system in **C++ 17**. It leverages many of the C++ features to provide a simple
to use API to the end user yet deliver near optimum code. It is also design to the user does not need to know
very much about multi-core and when the user makes a mistake the MECS should be able to detected.
It also allows advance users to take advantage of complex multi-core features easily and cleanly. 
<img src="https://i.imgur.com/9a5d2ee.png" align="right" width="150px" />

The project is meant to serve as a reference/bench mark for other ECS system out there.
The code should be able to be inserted in game engines and applications without a problem.

Any feedback is welcome. Please follow us and help support the project.

[              ![Twitter](https://img.shields.io/twitter/follow/nickreal03.svg?label=Follow&style=social)](https://twitter.com/nickreal03)
[                 ![chat](https://img.shields.io/discord/552344404258586644.svg?logo=discord)](https://discord.gg/fqaFSRE)

<!---
[<img src="https://i.imgur.com/4g2tHbP.png" width="170px" />](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=QPCQL53F8N73J&source=url)

[![Paypal](https://img.shields.io/badge/PayPal-Donate-blue.svg)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=QPCQL53F8N73J)
[        ![SubscriveStar](https://img.shields.io/badge/SubscriveStar-Donate-blue.svg)](https://www.subscribestar.com/LIONant)
[              ![Patreon](https://img.shields.io/badge/Patreon-Donate-blue.svg)](https://www.patreon.com/LIONant)
--->


# Features
The main concept of this ECS is to leverage all the features of the hardware to the maximum, and to provide a unify system for both CPU and GPU.

* **MIT license**. Not need to worry is all yours.
* **Lockless architecture**. Utilizes all the hardware threads, and remains completely safe for the end user.
* **Virtual memory support**. Provide super fast memory manipulations and minimizes wasted memory.
* **Archetypes support**. Groups entities into flat arrays to provide near zero cache misses.
* **Single CPP/H**. It is not a library is a module. Just include a single cpp/h into your project directly.
* **GPU support**. You can create systems that run entirely in the GPU, yet is able to play nice with the CPU systems.
* **Events/Delegates**. The events are lighting fast and use to extended systems to keep cache efficiency.
* **C++ 17 meta-programming**. Makes the compiler work hard so that the run time doesn't + keeps API simple.
* **Quantum components** Support direct multi-core access components, for advance users.
* **Share components** Factors out components which are share across multiple entities.
* **Tag components** Helps user group entities easily. 
* **GUID for entities** Helps identify entities across different levels, scripts, versions, etc.
* **Game Graph** Simple multi-core graph allows user to define order dependencies easily and flexibly. 
* **Universe/Worlds** Simple way to organize your project flexibly.
* **Data worlds** for blasting fast streaming of worlds. Think Playstation 5.
* **Unit-test** to make sure everything is working.
* **Command Line examples** Minimizes dependencies and keeps the project simple to follow.
* **Visual Examples** Shows how MECS can be used with a multi-core graphics APIs.
* **Step by step execution** So that it is easy to integrate with editor, or continuous execution for speed.

# Code Example

```c++
    struct position : mecs::component::data {
        xcore::vector2 m_Value;
    };

    struct velocity : mecs::component::data {
        xcore::vector2 m_Value;
    };

    struct move_system : mecs::system::instance {
        using instance::instance;
        void operator()( position& Position, const velocity& Velocity ) const {
            Position.m_Value += Velocity.m_Value * getTime().m_DeltaTime;
        }
    };
```

# Similar Projects

* Previous version (0.1) of MECS: https://gitlab.com/LIONant/MECS
* https://github.com/alecthomas/entityx
* https://github.com/sschmid/Entitas-CSharp
* https://github.com/skypjack/entt
* https://github.com/junkdog/entity-system-benchmarks
* https://github.com/SanderMertens/flecs

