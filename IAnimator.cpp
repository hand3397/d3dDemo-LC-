#include "IAnimator.h"

bool IAnimator::IsFinished() const
{
    return isFinished_;
}

void IAnimator::Pause()
{
    isPaused_ = true;
}

void IAnimator::Resume()
{
    isPaused_ = false;
}

void IAnimator::SetPlaySpeed(float speed) 
{ 
    playSpeed_ = speed; 
}

float IAnimator::GetPlaySpeed() const
{
    return playSpeed_;
}