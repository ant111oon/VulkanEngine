#include "pch.h"

#include "camera.h"

#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>


void Camera::ProcessSDLEvent(SDL_Event& e)
{
    constexpr float RAW_SPEED_VALUE = 0.5f;

    if (e.type == SDL_KEYDOWN) {
        if (e.key.keysym.sym == SDLK_w) { 
            velocity.z = -RAW_SPEED_VALUE;
        }
        if (e.key.keysym.sym == SDLK_s) {
            velocity.z = RAW_SPEED_VALUE;
        }
        if (e.key.keysym.sym == SDLK_a) {
            velocity.x = -RAW_SPEED_VALUE;
        }
        if (e.key.keysym.sym == SDLK_d) {
            velocity.x = RAW_SPEED_VALUE;
        }
        if (e.key.keysym.sym == SDLK_e) {
            velocity.y = RAW_SPEED_VALUE;
        }
        if (e.key.keysym.sym == SDLK_q) {
            velocity.y = -RAW_SPEED_VALUE;
        }
    }

    if (e.type == SDL_KEYUP) {
        if (e.key.keysym.sym == SDLK_w) {
            velocity.z = 0;
        }
        if (e.key.keysym.sym == SDLK_s) {
            velocity.z = 0;
        }
        if (e.key.keysym.sym == SDLK_a) {
            velocity.x = 0;
        }
        if (e.key.keysym.sym == SDLK_d) {
            velocity.x = 0;
        }
        if (e.key.keysym.sym == SDLK_e) {
            velocity.y = 0;
        }
        if (e.key.keysym.sym == SDLK_q) {
            velocity.y = 0;
        }
    }

    if (e.type == SDL_MOUSEMOTION) {
        yaw += (float)e.motion.xrel / 200.f;
        pitch -= (float)e.motion.yrel / 200.f;
    }
}


void Camera::Update()
{
    glm::mat4 cameraRotation = GetRotationMatrix();
    position += glm::vec3(cameraRotation * glm::vec4(velocity * 0.5f, 0.f));
}


glm::mat4 Camera::GetViewMatrix() const
{
    const glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.f), position);
    const glm::mat4 cameraRotation = GetRotationMatrix();
    
    return glm::inverse(cameraTranslation * cameraRotation);
}


glm::mat4 Camera::GetRotationMatrix() const
{
    const glm::quat pitchRotation = glm::angleAxis(pitch, glm::vec3 { 1.f, 0.f, 0.f });
    const glm::quat yawRotation = glm::angleAxis(yaw, glm::vec3 { 0.f, -1.f, 0.f });

    return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
}