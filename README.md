<img src="https://i.imgur.com/NwahbNn.jpg" align="right" width="250px" />

# MECS (Version: 2.0-Alpha)
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
to use API to the end user yet deliver near optimum code. It is also designed so the user does not need to know
very much about multi-core and when the user makes a mistake MECS should be able to detected.
It also allows advance users to take advantage of complex multi-core features easily and cleanly. 
<img src="https://i.imgur.com/9a5d2ee.png" align="right" width="150px" />

The project is meant to serve as a reference/bench mark for other ECS system out there.
The code should be able to be inserted in game engines and applications without a problem.

Any feedback is more than welcome. Please follow us and help support the project.

[              ![Twitter](https://img.shields.io/twitter/follow/nickreal03.svg?label=Follow&style=social)](https://twitter.com/nickreal03)
[                 ![chat](https://img.shields.io/discord/552344404258586644.svg?logo=discord)](https://discord.gg/fqaFSRE)

<!---
[<img src="https://i.imgur.com/4g2tHbP.png" width="170px" />](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=QPCQL53F8N73J&source=url)

[![Paypal](https://img.shields.io/badge/PayPal-Donate-blue.svg)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=QPCQL53F8N73J)
[        ![SubscriveStar](https://img.shields.io/badge/SubscriveStar-Donate-blue.svg)](https://www.subscribestar.com/LIONant)
[              ![Patreon](https://img.shields.io/badge/Patreon-Donate-blue.svg)](https://www.patreon.com/LIONant)
--->

# Features
The main concept of MECS is to leverage all the features of the hardware to the maximum, and to provide a unify system for both CPU and GPU.

### Legal
* **MIT license**. Not need to worry is all yours.

### Architecture
* **Lockless architecture**. Utilizes all the hardware threads, and remains completely safe for the end user.
* **Virtual memory support**. Provide super fast memory manipulations and minimizes wasted memory.
* **Archetypes support**. Groups entities into flat arrays to provide near zero cache misses.
* **Single CPP/H**. It is not a library is a module. Just include a single cpp/h into your project directly.
* **[WIP :construction:] GPU support**. You can create systems that run entirely in the GPU, yet is able to play nice with the CPU systems.
* **System Events/Delegates**. Simple generic way to extended systems, and lighting fast with hot cache efficiency.
* **Archetype Events/Delegates**. Add functionality on key events from Archetypes, adding, updating, move-in, etc.
* **C++ 17 meta-programming**. Makes the compiler work hard so that the run time doesn't + keeps API simple.
* **GUID for entities**. Helps identify entities across different levels, scripts, versions, etc.
* **Game Graph**. Simple multi-core graph allows user to define order dependencies easily and flexibly. 
* **Universe/Worlds**. Simple way to organize your project flexibly. Can run multiple worlds at the same time.
* **Data worlds**. for blasting fast streaming of worlds. Think Playstation 5.
* **Step by step execution**. So that it is easy to integrate with editor, or continuous execution for speed.
* **Procedure + Data Oriented APIs**. Procedure API for speed and simplicity + Data Oriented APIs for editors.

### Component Types
* **Linear**. Simple and easy to use components with plenty of debug features.
* **Quantum**. Support direct multi-core access components, for advance users.
* **Double Buffer**. To allow simultaneous Read/Write operation for linear and quantum components.
* **Share**. Factors out components which are share across multiple entities.
* **Tags**. Helps user group entities easily. 
* **Singleton**. Allows to build supporting systems, complex components, and still plays nice with MECS.
* **[WIP :construction:] Reference**. references to other entities in a safe, simple and elegant way. Great for assets/resources.

### Pipeline / Documentation
* **Unit-test**. to make sure everything is working.
* **Command Line examples**. Minimizes dependencies and keeps the project simple to follow.
* **Visual Examples**. Shows how MECS can be used with a multi-core graphics APIs.
* **Follows Unity3D**. Name of concepts follows unity3D when possible.

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
<br>

# Dependencies

* XCORE - https://gitlab.com/LIONant/xcore (low level API, hard dependency)
* DiligentEngine - https://github.com/DiligentGraphics/DiligentEngine (graphics API, Soft dependency for the Graphical Examples)

# Similar Projects

* Previous version (1.0-Alpha) of MECS: https://gitlab.com/LIONant/MECS
* https://github.com/alecthomas/entityx
* https://github.com/sschmid/Entitas-CSharp
* https://github.com/skypjack/entt
* https://github.com/junkdog/entity-system-benchmarks
* https://github.com/SanderMertens/flecs

