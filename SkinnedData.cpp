#include "SkinnedData.h"

void SkinnedData::SetNode(vector<uint32_t>& parentNode, vector<vector<uint32_t>>& childrenNode,
	unordered_map<string, uint32_t> nodeToIdx, vector<XMFLOAT4X4>& nodeTransforms)
{
	parentNode_ = parentNode;
	childrenNode_ = childrenNode;
	nodeToIdx_ = nodeToIdx;
	nodeTransforms_ = nodeTransforms;
}

void SkinnedData::SetBone(unordered_map<uint32_t, uint32_t> nodeToBone, vector<XMFLOAT4X4>& boneOffset)
{
	nodeToBone_ = nodeToBone;
	boneOffsets_ = boneOffset;
}

void SkinnedData::AddAnimaiton(const string& clipName, const SkinnedAnimationClip& clip)
{
    animations_[clipName] = clip;
}

UINT SkinnedData::NodeCount()const
{
	return nodeToIdx_.size();
}

UINT SkinnedData::BoneCount()const
{
	return nodeToBone_.size();
}
 
void SkinnedData::GetFinalTransforms(const string& clipName, float timePos,  vector<XMFLOAT4X4>& finalTransforms)const
{
	//uint32_t numNodes = NodeCount();
	//uint32_t numBones = BoneCount();
	//
	//auto it = animations_.find(clipName);
	//fill(finalTransforms.begin(), finalTransforms.begin() + numBones, MathHelper::Identity4x4());
	//if (it == animations_.end()) {
	//	return;
	//}
	//vector<XMFLOAT4X4> transforms(numNodes, MathHelper::Identity4x4());
	//// 이 클립의 모든 뼈대를 주어진 시간에 맞게 보간한다.
	//it->second.InterpolateAll(timePos, transforms);
	//
	//// rootNode = 0
	//for (uint32_t ni = 1; ni < numNodes; ni++) {
	//	transforms[ni] = transforms[ni] * transforms[parentNode_[ni]];
	//}

	////XMMatrixTranspose
	//for (uint32_t ni = 0; ni < numNodes; ++ni) {
	//	int32_t bi = NodeToBone(ni);
	//	if (bi == -1)
	//		continue;
	//	XMMATRIX finalTransform = XMLoadFloat4x4(&boneOffsets_[bi]) * transforms[ni];
	//	//XMMATRIX finalTransform = XMLoadFloat4x4(&boneOffsets_[bi]);
	//	XMStoreFloat4x4(&finalTransforms[bi], XMMatrixTranspose(finalTransform));
	//}
}

void SkinnedData::GetFinalTransforms(const string& clipName1, float timePos, const string& clipName2, float alpha, vector<XMFLOAT4X4>& finalTransforms)const
{
	alpha = clamp(alpha, 0.0f, 1.0f);

	uint32_t numNodes = NodeCount();
	uint32_t numBones = BoneCount();

	auto it1 = animations_.find(clipName1), it2 = animations_.find(clipName2);
	fill(finalTransforms.begin(), finalTransforms.begin() + numBones, MathHelper::Identity4x4());
	if (it1 == animations_.end() || it2 == animations_.end()) {
		return;
	}
	vector<XMMATRIX> transforms(numNodes, XMMatrixIdentity());
	// 이 클립의 모든 뼈대를 주어진 시간에 맞게 보간한다.
	BlendInterpolate(it1->second, it2->second, timePos, transforms, alpha);
	
	// rootNode = 0
	for (uint32_t ni = 1; ni < numNodes; ni++) {
		transforms[ni] = transforms[ni] * transforms[parentNode_[ni]];
	}

	//XMMatrixTranspose
	for (uint32_t ni = 0; ni < numNodes; ++ni) {
		int32_t bi = NodeToBone(ni);
		if (bi == -1)
			continue;
		XMMATRIX finalTransform = XMLoadFloat4x4(&boneOffsets_[bi]) * transforms[ni];
		//XMMATRIX finalTransform = XMLoadFloat4x4(&boneOffsets_[bi]);
		XMStoreFloat4x4(&finalTransforms[bi], XMMatrixTranspose(finalTransform));
	}
}

void SkinnedData::AddBlendingAnimation(const string& name, const string& clip1, const string& clip2, float alpha)
{
	//if (name.empty() ||
	//	animations_.find(clip1) == animations_.end() ||
	//	animations_.find(clip2) == animations_.end())
	//	return;

	//if (alpha > 1.0 || alpha < 0.0f)
	//	return;

	//const SkinnedAnimationClip& anim1 = animations_[clip1];
	//const SkinnedAnimationClip& anim2 = animations_[clip2];
	//
	//SkinnedAnimationClip blendedAnimation;
	//// 애니메이션 길이가 다를 경우 길이가 짧은 애니메이션을 더 길게 재생함.
	//if (anim1.GetDuration() > anim2.GetDuration())
	//	blendedAnimation.SetDuration(anim1.GetDuration(), anim1.GetTicksPerSecond());
	//else 
	//	blendedAnimation.SetDuration(anim2.GetDuration(), anim2.GetTicksPerSecond());

	//blendedAnimation.boneAnimations_.resize(NodeCount());
	//for (uint32_t ni = 0; ni < NodeCount(); ni++) {
	//	vector<double> times;
	//	const BoneAnimation &boneAnim1 = anim1.boneAnimations_[ni];
	//	const BoneAnimation &boneAnim2 = anim2.boneAnimations_[ni];
	//	times.reserve(boneAnim1.keyframes_.size() + boneAnim2.keyframes_.size());

	//	for (auto& kf : boneAnim1.keyframes_)
	//		times.push_back(kf.timePos_);
	//	for (auto& kf : boneAnim2.keyframes_)
	//		times.push_back(kf.timePos_);

	//	sort(times.begin(), times.end());

	//	auto it = std::unique(times.begin(), times.end(), MathHelper::FloatEqual());
	//	times.erase(it, times.end());

	//	uint32_t numTimes = times.size();
	//	vector<Keyframe>& keyFrames = blendedAnimation.boneAnimations_[ni].keyframes_;
	//	keyFrames.resize(numTimes);

	//	for (uint32_t ti = 0; ti < numTimes; ti++) {
	//		double time = times[ti];
	//		double alphaTime = time / times.back();
	//		Keyframe key1 = boneAnim1.InterpolateKeyframeAlpha(alphaTime);
	//		Keyframe key2 = boneAnim2.InterpolateKeyframeAlpha(alphaTime);

	//		keyFrames[ti].timePos_ = time;
	//		keyFrames[ti].scale_;
	//		keyFrames[ti].rotationQuat_;
	//		keyFrames[ti].translation_;

	//		XMStoreFloat3(& keyFrames[ti].scale_, XMVectorLerp(XMLoadFloat3(&key1.scale_), XMLoadFloat3(&key2.scale_), alpha));
	//		XMStoreFloat4(& keyFrames[ti].rotationQuat_, XMQuaternionSlerp(XMLoadFloat4(&key1.rotationQuat_), XMLoadFloat4(&key2.rotationQuat_), alpha));
	//		XMStoreFloat3(& keyFrames[ti].translation_, XMVectorLerp(XMLoadFloat3(&key1.translation_), XMLoadFloat3(&key2.translation_), alpha));
	//	}
	//}

	//AddAnimaiton(name, blendedAnimation);
}

void SkinnedData::BlendInterpolate(const SkinnedAnimationClip& clip1, const SkinnedAnimationClip& clip2, 
	float timePos, vector<XMMATRIX>& transforms, float alpha)const
{
	//// alpha = (sub) 0.0f ~ 1.0f (main)
	//uint32_t numNodes = NodeCount();
	//for (uint32_t ni = 0; ni < numNodes; ni++) {
	//	Keyframe mainKey = clip1.boneAnimations_[ni].keyframes_.empty() ? Keyframe() :
	//		mainClip.boneAnimations_[ni].InterpolateKeyframe(timePos);
	//	Keyframe subKey = clip2.boneAnimations_[ni].keyframes_.empty() ? Keyframe() :
	//		subClip.boneAnimations_[ni].InterpolateKeyframe(timePos);

	//	XMVECTOR S = XMVectorLerp(XMLoadFloat3(&subKey.scale_), XMLoadFloat3(&mainKey.scale_), alpha);
	//	XMVECTOR P = XMVectorLerp(XMLoadFloat3(&subKey.translation_), XMLoadFloat3(&mainKey.translation_), alpha);
	//	XMVECTOR Q = XMQuaternionSlerp(XMLoadFloat4(&subKey.rotationQuat_), XMLoadFloat4(&mainKey.rotationQuat_), alpha);

	//	transforms[ni] = XMMatrixAffineTransformation(S, XMVectorZero(), Q, P);
	//}
}

int32_t SkinnedData::NodeToIdx(const string& name) const
{
	auto it = nodeToIdx_.find(name);
	if (it == nodeToIdx_.end())
		return -1;
	return it->second;
}

int32_t SkinnedData::NodeToBone(const uint32_t node) const
{
	auto it = nodeToBone_.find(node);
	if (it == nodeToBone_.end())
		return -1;
	return it->second;
}
