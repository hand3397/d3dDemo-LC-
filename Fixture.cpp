#include "Fixture.h"
#include "Rigidbody.h"
#include "BroadPhase.h"

namespace spe {;

Fixture::Fixture(Shape* shape) :
    shape_(shape), proxy_(nullptr)
{
    shapeType_ = shape->GetType();
    proxy_ = new FixtureProxy();
}

Fixture::~Fixture()
{
    if (shape_ != nullptr) {
        delete shape_;
        shape_ = nullptr;
    }
    if (proxy_ != nullptr) {
        delete proxy_;
        proxy_ = nullptr;
    }
}

void Fixture::CreateProxy(BroadPhase* broadPhase)
{
    proxy_->aabb = shape_->GetAABB(rigidbody_->GetTransformMatrix());
    proxy_->proxyId = broadPhase->CreateProxy(proxy_->aabb, proxy_);
    proxy_->fixture = this;
}

void Fixture::DestroyProxy(BroadPhase* broadPhase)
{
    if (proxy_ == nullptr)
        return;
    broadPhase->DestroyProxy(proxy_->proxyId);
    delete proxy_;
    proxy_ = nullptr;
}

void Fixture::Synchronize(BroadPhase* broadPhase, const XMMATRIX& xf1, const XMMATRIX& xf2)
{
    if (proxy_ == nullptr)
        return;

    AABB aabb1 = shape_->GetAABB(xf1);
    AABB aabb2 = shape_->GetAABB(xf2);

    proxy_->aabb.Combine(aabb1, aabb2);

    XMVECTOR displacement = xf2.r[3] - xf1.r[3];
    broadPhase->MoveProxy(proxy_->proxyId, proxy_->aabb, displacement);
}

Rigidbody* Fixture::GetRigidbody() const
{
    return rigidbody_;
}

Shape* Fixture::GetShape()
{
    return shape_;
}

ShapeType Fixture::GetShapeType()const
{
    return shapeType_;
}

float Fixture::GetFriction()const
{
    return friction_;
}

float Fixture::GetRestitution()const
{
    return restitution_;
}

const FixtureProxy* Fixture::GetFixtureProxy() const
{
    return proxy_;
}

}