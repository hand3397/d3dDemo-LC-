#include "ContactFilter.h"

namespace spe {;

ContactFilter::ContactFilter(uint16_t category, uint16_t mask)
    : categoryBits(category), maskBits(mask)
{
}

void ContactFilter::Set(uint16_t category, uint16_t mask)
{
    categoryBits = category;
    maskBits = mask;
}

void ContactFilter::AddCollision(uint16_t targetCategory)
{
    maskBits |= targetCategory;
}

void ContactFilter::IgnoreCollision(uint16_t targetCategory)
{
    maskBits &= ~targetCategory;
}

void ContactFilter::AddCategory(uint16_t category)
{
    categoryBits |= category;
}

void ContactFilter::RemoveCategory(uint16_t category)
{
    categoryBits &= ~category;
}

// 전역 도우미 함수 구현
bool ShouldCollide(const ContactFilter& filterA, const ContactFilter& filterB)
{
    bool aCollidesWithB = (filterA.maskBits & filterB.categoryBits) != 0;
    bool bCollidesWithA = (filterB.maskBits & filterA.categoryBits) != 0;

    return aCollidesWithB && bCollidesWithA;
}

} // namespace spe