#include "RigidBody.h"

void Rigidbody::UpdateBounds()
{
    XMMATRIX S = XMMatrixScaling(scale_.x, scale_.y, scale_.z);
    XMMATRIX R = XMMatrixRotationQuaternion(XMLoadFloat4(&orientation_));
    XMMATRIX T = XMMatrixTranslation(position_.x, position_.y, position_.z);

    XMMATRIX transform = S * R * T;

    localBoundingBox_.Transform(boundingBox_, transform);
    localBoundingSphere_.Transform(boundingSphere_, transform);
}
