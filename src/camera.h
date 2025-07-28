#pragma once

#include "vk_types.h"

#include <SDL_events.h>


struct Camera
{
    void ProcessSDLEvent(SDL_Event& event);

    void Update();

    glm::mat4 GetViewMatrix() const;
    glm::mat4 GetRotationMatrix() const;

    glm::vec3 velocity;
    glm::vec3 position;
    float pitch = 0.f;
    float yaw = 0.f;
    float speed = 1.f;
};