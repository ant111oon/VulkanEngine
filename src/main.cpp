#include "vk_engine.h"


int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    VulkanEngine& engine = VulkanEngine::GetInstance();
    
    engine.Init();
    engine.Run();
    engine.Terminate();
    
    return 0;
}