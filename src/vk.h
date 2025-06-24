#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vk_mem_alloc.h>

#include "core.h"


#ifdef ENG_DEBUG
    #define ENG_VK_CHECK(RESULT)                                                                        \
        do {                                                                                            \
            const VkResult _vkCheckRes = RESULT;                                                        \
            ENG_ASSERT_MSG(_vkCheckRes == VK_SUCCESS, "[VK ERROR]: {}", string_VkResult(_vkCheckRes));  \
        } while (0)
#else
    #define ENG_VK_CHECK(RESULT) (RESULT)
#endif