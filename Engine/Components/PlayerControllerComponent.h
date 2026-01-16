#pragma once
#include "CameraFollowComponent.h"

//enum class CameraMode
//{
//    ThirdPerson,
//    FirstPerson
//};


struct PlayerControllerComponent
{
    float moveSpeed = 5.0f;
    float lookSpeed = 0.1f;

    CameraMode defaultCameraMode = CameraMode::ThirdPerson;
    CameraMode cameraMode = CameraMode::ThirdPerson;

    bool allowCameraSwitch = true;
};