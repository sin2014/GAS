// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chaos/TriangleMeshImplicitObject.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "UObject/UObjectIterator.h"

class FOutputDevice;
class UWorld;

namespace LyraEditorUtilities
{

//////////////////////////////////////////////////////////////////////////

// 检测 Chaos 三角网格是否至少包含一个顶点重复的退化三角形。
// returns true if the mesh has one or more degenerate triangles
bool CheckMeshDataForProblem(const Chaos::FTriangleMeshImplicitObject::ParticlesType& Particles, const Chaos::FTrimeshIndexBuffer& Elements)
{
	// 索引缓冲类型是模板参数，因此用泛型 Lambda 统一遍历不同索引格式。
	// Internal helper because the index buffer type is templated
	auto CheckTris = [&](const auto& Elements, int32 NumTriangles)
	{
		using VecType = Chaos::FTriangleMeshImplicitObject::ParticleVecType;

		for (int32 FaceIdx = 0; FaceIdx < NumTriangles; ++FaceIdx)
		{
			const VecType& A = Particles.GetX(Elements[FaceIdx][0]);
			const VecType& B = Particles.GetX(Elements[FaceIdx][1]);
			const VecType& C = Particles.GetX(Elements[FaceIdx][2]);

			const VecType AB = B - A;
			const VecType AC = C - A;
			VecType Normal = VecType::CrossProduct(AB, AC);

			if (Normal.SafeNormalize() < SMALL_NUMBER)
			{
				return true;
			}
		}

		return false;
	};

	const int32 NumTriangles = Elements.GetNumTriangles();
	if (Elements.RequiresLargeIndices())
	{
		return CheckTris(Elements.GetLargeIndexBuffer(), NumTriangles);
	}
	else
	{
		return CheckTris(Elements.GetSmallIndexBuffer(), NumTriangles);
	}
}

// 遍历当前已加载 StaticMesh 的 Chaos 三角碰撞数据，并为每个包含退化三角形的资产记录警告。
void CheckChaosMeshCollision(FOutputDevice& Ar)
{
	for (UStaticMesh* MeshAsset : TObjectRange<UStaticMesh>())
	{
		if (UBodySetup* BodySetup = MeshAsset->GetBodySetup())
		{
			for (const Chaos::FTriangleMeshImplicitObjectPtr& TriMesh : BodySetup->TriMeshGeometries)
			{
				if (Chaos::FTriangleMeshImplicitObject* TriMeshData = TriMesh.GetReference())
				{
					if (CheckMeshDataForProblem(TriMeshData->Particles(), TriMeshData->Elements()))
					{
						UE_LOG(LogConsoleResponse, Warning, TEXT("Mesh asset %s has one or more degenerate triangles in collision data"), *GetPathNameSafe(MeshAsset));
					}
				}
			}
		}
	}
}

// 注册 Lyra.CheckChaosMeshCollision 控制台命令，把检查结果写入调用方输出设备。
FAutoConsoleCommandWithWorldArgsAndOutputDevice GCheckChaosMeshCollisionCmd(
	TEXT("Lyra.CheckChaosMeshCollision"),
	TEXT("Usage:\n")
	TEXT("  Lyra.CheckChaosMeshCollision\n")
	TEXT("\n")
	TEXT("It will check Chaos collision data for all *loaded* static mesh assets for any degenerate triangles"),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateStatic(
		[](const TArray<FString>& Params, UWorld* World, FOutputDevice& Ar)
{
	CheckChaosMeshCollision(Ar);
}));


//////////////////////////////////////////////////////////////////////////

}; // End of namespace
