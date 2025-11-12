#pragma once
#include "MemoryPool.h";

// 레이어 없는 버전
template<typename T>
class ObjectManager
{
protected:
    MemoryPool<T> pool;
    std::vector<T*> allObjects_;
    size_t poolSize_;
public:
    ObjectManager(size_t poolSize) : poolSize_(poolSize), pool(poolSize)
    {
        allObjects_.reserve(poolSize);
    }
    ~ObjectManager() { Clear(); }

    size_t Size() { return poolSize_; }
    std::vector<T*> GetAllObjects() { return allObjects_; }

    template<typename... Args>
    T* CreateObject(Args&&... args)
    {
        T* obj = pool.allocate(std::forward<Args>(args)...);
        allObjects_.push_back(obj);
        return obj;
    }

    void DestroyObject(T* obj)
    {
        allObjects_.erase(std::remove(allObjects_.begin(), allObjects_.end(), obj),
            allObjects_.end());
        pool.deallocate(obj);
    }

    void Clear()
    {
        for (auto& ob : allObjects_) {
            if (ob) {
                pool.deallocate(ob);
                ob = nullptr;
            }
        }
        allObjects_.clear();
    }
};

// 레이어 있는 버전
template<typename T, typename Layer>
class LayeredObjectManager
{
private:
    MemoryPool<T> pool;
    std::vector<T*> allObjects_;
    std::vector<T*> objectLayer_[(uint32_t)Layer::Count];
    size_t poolSize_;
public:
    LayeredObjectManager(size_t poolSize) : poolSize_(poolSize), pool(poolSize) {}
    ~LayeredObjectManager() { Clear(); }

    size_t Size() { return poolSize_; }
    std::vector<T*> GetAllObjects() { return allObjects_; }
    std::vector<T*> GetLayeredObjects(const Layer layer) { return objectLayer_[(uint32_t)layer]; }

    template<typename... Args>
    T* CreateObject(Args&&... args)
    {
        T* obj = pool.allocate(std::forward<Args>(args)...);
        allObjects_.push_back(obj);
        objectLayer_[(uint32_t)obj->GetType()].push_back(obj);
        return obj;
    }

    void DestroyObject(T* obj)
    {
        auto& layer = objectLayer_[(uint32_t)obj->GetType()];
        layer.erase(std::remove(layer.begin(), layer.end(), obj), layer.end());

        allObjects_.erase(std::remove(allObjects_.begin(), allObjects_.end(), obj),
            allObjects_.end());

        pool.deallocate(obj);
    }

    void Clear()
    {
        for (auto& ob : allObjects_) {
            if (ob) {
                pool.deallocate(ob);
                ob = nullptr;
            }
        }
        allObjects_.clear();
        for (auto& layer : objectLayer_)
            layer.clear();
    }
};


