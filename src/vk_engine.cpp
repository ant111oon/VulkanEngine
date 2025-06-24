#include "core.h"

#include "vk_engine.h"
#include "vk_initializers.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>


#define ENG_CHECK_SDL_ERROR(COND, ...)                      \
    if (!(COND)) {                                          \
        ENG_ASSERT_FAIL("[SDL ERROR]: {}", SDL_GetError()); \
        return __VA_ARGS__;                                 \
    }


constexpr bool cfg_UseValidationLayers = false;


VulkanEngine& VulkanEngine::GetInstance() noexcept
{
    static VulkanEngine engine;
    return engine;
}


void VulkanEngine::Init() noexcept
{
    if (m_isInitialized) {
        return;
    }

    ENG_CHECK_SDL_ERROR(SDL_Init(SDL_INIT_VIDEO) == 0);

    const SDL_WindowFlags windowFlags = static_cast<SDL_WindowFlags>(SDL_WINDOW_VULKAN);

    m_pWindow = SDL_CreateWindow("Vulkan Engine", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        m_windowExtent.width, m_windowExtent.height, windowFlags);

    ENG_CHECK_SDL_ERROR(m_pWindow);

    m_isInitialized = true;
}


void VulkanEngine::Terminate() noexcept
{
    if (!m_isInitialized) {
        return;
    }

    SDL_DestroyWindow(m_pWindow);

    m_isInitialized = false;
}


void VulkanEngine::Run() noexcept
{
    ENG_ASSERT(IsInitialized());

    bool isQuit = false;
    
    SDL_Event event;
    while (!isQuit) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    isQuit = true;
                    break;

                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_MINIMIZED) {
                        m_needRender = false;
                    } else if (event.window.event == SDL_WINDOWEVENT_RESTORED) {
                        m_needRender = true;
                    }
                    break;

                default:
                    break;
            }
        }

        if (m_needRender) {
            Render();
        } else {
            // do not draw if window is minimized
            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
    }
}


VulkanEngine::~VulkanEngine()
{
    Terminate();
}


void VulkanEngine::Render() noexcept
{

}
