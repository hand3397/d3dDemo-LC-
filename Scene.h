#pragma once
#include "GameObject.h"

class Scene
{
public:
    void AddObject(std::shared_ptr<GameObject> obj)
    {
        objects_.push_back(obj);
    }

    // 렌더링 준비
    void CollectRenderItems(std::vector<RenderItem*>& renderItems)
    {
        renderItems.clear();
        for (auto& obj : objects_) {
            obj->UpdateRenderItem();          // 월드 행렬 갱신
            renderItems.push_back(obj->GetRenderItem());
        }
    }

private:
    std::vector<std::shared_ptr<GameObject>> objects_;
};

