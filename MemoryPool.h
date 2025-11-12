#pragma once
#include <iostream>
#include <vector>

template <typename T>
class MemoryPool
{
private:
    struct Node
    {
        Node* next_;
    };

    std::vector<char> buffer_;
    Node* freeList_;
    size_t poolSize_ = 0;

public:
    MemoryPool(size_t poolSize) : poolSize_(poolSize), buffer_(sizeof(T)* poolSize), freeList_(nullptr)
    {
        for (size_t i = 0; i < poolSize_; ++i) {
            Node* node = reinterpret_cast<Node*>(&buffer_[i * sizeof(T)]);
            node->next_ = freeList_;
            freeList_ = node;
        }
    }

    template <typename... Args>
    T* allocate(Args&&... args)
    {
        if (!freeList_) throw std::bad_alloc();
        Node* node = freeList_;
        freeList_ = freeList_->next_;
        // placement new로 생성자 호출
        return new (node) T(std::forward<Args>(args)...);
    }

    void deallocate(T* ptr)
    {
        // 소멸자 호출
        ptr->~T();
        // free list에 반환
        Node* node = reinterpret_cast<Node*>(ptr);
        node->next_ = freeList_;
        freeList_ = node;
    }

    size_t Size() { return poolSize_; }
};
