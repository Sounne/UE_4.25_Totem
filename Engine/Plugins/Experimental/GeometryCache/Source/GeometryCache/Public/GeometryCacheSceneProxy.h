// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Containers/ResourceArray.h"
#include "RenderResource.h"
#include "PrimitiveViewRelevance.h"
#include "PrimitiveSceneProxy.h"
#include "Materials/MaterialInterface.h"
#include "GeometryCacheVertexFactory.h"
#include "DynamicMeshBuilder.h"
#include "StaticMeshResources.h"
#include "Logging/LogMacros.h"

class FMeshElementCollector;
struct FGeometryCacheMeshData;
class FGeometryCacheTrackStreamableRenderResource;
struct FVisibilitySample;
class UGeometryCacheTrack;

/** Resource array to pass  */
class GEOMETRYCACHE_API FGeomCacheVertexBuffer : public FVertexBuffer
{
public:

	void Init(int32 InSizeInBytes)
	{
		check(this->IsInitialized() == false);
		SizeInBytes = InSizeInBytes;		
	}

	/* Create on rhi thread. Uninitialized with NumVertices space.*/
	virtual void InitRHI() override;

	/**
	 * Sugar function to update from a typed array.
	 */
	template<class DataType> void Update(const TArray<DataType> &Vertices)
	{
		int32 InSize = Vertices.Num() * sizeof(DataType);
		UpdateRaw(Vertices.GetData(), InSize, 1, 1);
	}

	void UpdatePositionsOnly(const TArray<FDynamicMeshVertex> &Vertices)
	{
		const uint32 PositionOffset = STRUCT_OFFSET(FDynamicMeshVertex, Position);
		const uint32 PositionSize = sizeof(((FDynamicMeshVertex*)nullptr)->Position);
		UpdateRaw(Vertices.GetData() + PositionOffset, Vertices.Num(), PositionSize, sizeof(FDynamicMeshVertex));
	}

	void UpdateExceptPositions(const TArray<FDynamicMeshVertex> &Vertices)
	{
		const uint32 PositionSize = sizeof(((FDynamicMeshVertex*)nullptr)->Position);
		const uint32 PositionOffset = STRUCT_OFFSET(FDynamicMeshVertex, Position);

		static_assert(PositionOffset == 0, "Expecting position to be the first struct member");
		static_assert(PositionSize == STRUCT_OFFSET(FDynamicMeshVertex, TextureCoordinate), "Expecting the texture coordinate to immediately follow the Position");

		UpdateRaw( (int8 *)Vertices.GetData() + PositionSize, Vertices.Num(), sizeof(FDynamicMeshVertex) - PositionSize, sizeof(FDynamicMeshVertex));
	}

	/**
	 * Update the raw contents of the buffer, possibly reallocate if needed.
	 */
	void UpdateRaw(const void *Data, int32 NumItems, int32 ItemSizeBytes, int32 ItemStrideBytes);

	/**
	 * Resize the buffer but don't initialize it with any data.
	 */
	void UpdateSize(int32 NewSizeInBytes);

	/**
	* Resize the buffer but don't initialize it with any data.
	*/
	template<class DataType> void UpdateSizeTyped(int32 NewSizeInElements)
	{
		UpdateSize(sizeof(DataType) * NewSizeInElements);
	}

	/**
	 * Get the current size of the buffer
	 */
	unsigned GetSizeInBytes() { return SizeInBytes; }

	virtual FString GetFriendlyName() const override { return TEXT("FGeomCacheVertexBuffer"); }

private:
	int32 SizeInBytes;
};

/** Index Buffer */
class GEOMETRYCACHE_API FGeomCacheIndexBuffer : public FIndexBuffer
{
public:
	int32 NumIndices;

	/* Create on rhi thread. Uninitialized with NumIndices space.*/
	virtual void InitRHI() override;

	/**
		Update the data and possibly reallocate if needed.
	*/
	void Update(const TArray<uint32> &Indices);

	void UpdateSizeOnly(int32 NewNumIndices);

	unsigned SizeInBytes() { return NumIndices * sizeof(uint32); }
};

/** Vertex Factory */
class GEOMETRYCACHE_API FGeomCacheVertexFactory : public FGeometryCacheVertexVertexFactory
{
public:

	FGeomCacheVertexFactory(ERHIFeatureLevel::Type InFeatureLevel);

	/** Init function that should only be called on render thread. */
	void Init_RenderThread(const FVertexBuffer* PositionBuffer, const FVertexBuffer* MotionBlurDataBuffer, const FVertexBuffer* TangentXBuffer, const FVertexBuffer* TangentZBuffer, const FVertexBuffer* TextureCoordinateBuffer, const FVertexBuffer* ColorBuffer);

	/** Init function that can be called on any thread, and will do the right thing (enqueue command if called on main thread) */
	void Init(const FVertexBuffer* PositionBuffer, const FVertexBuffer* MotionBlurDataBuffer, const FVertexBuffer* TangentXBuffer, const FVertexBuffer* TangentZBuffer, const FVertexBuffer* TextureCoordinateBuffer, const FVertexBuffer* ColorBuffer);
};

/**
 * This the track proxy has some "double double buffering" going on.
 * First we keep two mesh frames. The one just before the current time and the one just after the current time. This is the full mesh and
 * we interpolate between it to derive the actual mesh for the exact time we're at.
 * Secondly we have two position buffers. The one for the current rendered frame and the one from the previous rendered frame (this is not the same as
 * the mesh frame, the mesh may be at say 10 fps then get interpolated to 60 fps rendered frames)
 */
class GEOMETRYCACHE_API FGeomCacheTrackProxy
{
public:

	FGeomCacheTrackProxy(ERHIFeatureLevel::Type InFeatureLevel)
		: VertexFactory(InFeatureLevel/*, "FGeomCacheTrackProxy"*/)
	{}

	virtual ~FGeomCacheTrackProxy() {}

	/**
	 * Update the SampleIndex and MeshData for a given time
	 *
	 * @param Time - (Elapsed)Time to check against
	 * @param bLooping - Whether or not the animation is being played in a loop
	 * @param InOutMeshSampleIndex - Hold the MeshSampleIndex and will be updated if changed according to the Elapsed Time
	 * @param OutMeshData - Will hold the new MeshData if the SampleIndex changed
	 * @return true if the SampleIndex and MeshData were updated
	 */
	virtual bool UpdateMeshData(float Time, bool bLooping, int32& InOutMeshSampleIndex, FGeometryCacheMeshData& OutMeshData);

	/**
	 * Get the MeshData for a given SampleIndex
	 *
	 * @param SampleIndex - The sample index at which to retrieve the MeshData
	 * @param OutMeshData - Will hold the MeshData if it could be retrieved
	 * @return true if the MeshData was retrieved successfully
	 */
	virtual bool GetMeshData(int32 SampleIndex, FGeometryCacheMeshData& OutMeshData);

	/**
	 * Check if the topology of two given SampleIndexes are compatible (ie. same topology)
	 *
	 * @param SampleIndexA - The first sample index to compare the topology
	 * @param SampleIndexB - The second sample index to compare the topology
	 * @return true if the topology is the same
	 */
	virtual bool IsTopologyCompatible(int32 SampleIndexA, int32 SampleIndexB);

	/**
	 * Get the VisibilitySample for a given time
	 *
	 * @param Time - (Elapsed)Time to check against
	 * @param bLooping - Whether or not the animation is being played in a loop
	 * @return the VisibilitySample that corresponds to the given time
	 */
	virtual const FVisibilitySample& GetVisibilitySample(float Time, const bool bLooping) const;

	/**
	 * Find the two frames closest to the given time
	 * InterpolationFactor gives the position of the requested time slot between the two returned frames.
	 * 0.0 => We are very close to OutFrameIndex
	 * 1.0 => We are very close to OutNextFrameIndex
	 * If bIsPlayingBackwards it will return exactly the same indexes but in the reversed order. The
	 * InterpolationFactor will also be updated accordingly
	 *
	 * @param Time - (Elapsed)Time to check against
	 * @param bLooping - Whether or not the animation is being played in a loop
	 * @param bIsPlayingBackwards - Whether the animation is playing backwards or forwards
	 * @param OutFrameIndex - The closest frame index that corresponds to the given time
	 * @param OutNextFrameIndex - The frame index that follows OutFrameIndex
	 * @param InterpolationFactor - The interpolation value between the two frame times
	 */
	virtual void FindSampleIndexesFromTime(float Time, bool bLooping, bool bIsPlayingBackwards, int32 &OutFrameIndex, int32 &OutNextFrameIndex, float &InInterpolationFactor);

	/** MeshData storing information used for rendering this Track */
	FGeometryCacheMeshData *MeshData;
	FGeometryCacheMeshData *NextFrameMeshData;

	/** Frame numbers corresponding to MeshData, NextFrameMeshData */
	int32 FrameIndex;
	int32 NextFrameIndex;
	float InterpolationFactor;

	/** Material applied to this Track */
	TArray<UMaterialInterface*> Materials;
	
	/** Vertex buffers for this Track. There are two position buffers which we double buffer between, current frame and last frame*/
	FGeomCacheVertexBuffer PositionBuffers[2];
	uint32 PositionBufferFrameIndices[2]; // Frame indexes of the positions in the position buffer 
	float PositionBufferFrameTimes[2]; // Exact time after interpolation of the positions in the position buffer.
	uint32 CurrentPositionBufferIndex; // CurrentPositionBufferIndex%2  is the last updated position buffer

	FGeomCacheVertexBuffer TangentXBuffer;
	FGeomCacheVertexBuffer TangentZBuffer;
	FGeomCacheVertexBuffer TextureCoordinatesBuffer;
	FGeomCacheVertexBuffer ColorBuffer;

	/** Index buffer for this Track */
	FGeomCacheIndexBuffer IndexBuffer;

	/** Vertex factory for this Track */
	FGeomCacheVertexFactory VertexFactory;
	
	/** World Matrix for this Track */
	FMatrix WorldMatrix;

	/** The GeometryCacheTrack to which the proxy is associated */
	UGeometryCacheTrack* Track;

	int32 UploadedSampleIndex;

#if RHI_RAYTRACING
	FRayTracingGeometry RayTracingGeometry;
#endif
};

/** Procedural mesh scene proxy */
class GEOMETRYCACHE_API FGeometryCacheSceneProxy : public FPrimitiveSceneProxy
{
public:
	SIZE_T GetTypeHash() const override;

	FGeometryCacheSceneProxy(class UGeometryCacheComponent* Component);
	FGeometryCacheSceneProxy(class UGeometryCacheComponent* Component, TFunction<FGeomCacheTrackProxy*()> TrackProxyCreator);

	virtual ~FGeometryCacheSceneProxy();

	// Begin FPrimitiveSceneProxy interface.
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;

	virtual bool CanBeOccluded() const override;
	virtual bool IsUsingDistanceCullFade() const override;
	virtual uint32 GetMemoryFootprint(void) const;
	uint32 GetAllocatedSize(void) const;
	// End FPrimitiveSceneProxy interface.

	void UpdateAnimation(float NewTime, bool bLooping, bool bIsPlayingBackwards, float PlaybackSpeed);

	/** Update world matrix for specific section */
	void UpdateSectionWorldMatrix(const int32 SectionIndex, const FMatrix& WorldMatrix);
	/** Update vertex buffer for specific section */
	void UpdateSectionVertexBuffer(const int32 SectionIndex, FGeometryCacheMeshData* MeshData );
	/** Update index buffer for specific section */
	void UpdateSectionIndexBuffer(const int32 SectionIndex, const TArray<uint32>& Indices);

	/** Clears the Sections array*/
	void ClearSections();

#if RHI_RAYTRACING
	virtual void GetDynamicRayTracingInstances(FRayTracingMaterialGatheringContext& Context, TArray<FRayTracingInstance>& OutRayTracingInstances) override final;
	virtual bool IsRayTracingRelevant() const override { return true; }
#endif
	
private:	
	FMaterialRelevance MaterialRelevance;
	float Time;
	bool bLooping;
	bool bIsPlayingBackwards;
	float PlaybackSpeed;
	uint32 UpdatedFrameNum;

	/** Array of Track Proxies */
	TArray<FGeomCacheTrackProxy*> Tracks;

	void FrameUpdate() const;

	void CreateMeshBatch(
		const FGeomCacheTrackProxy* TrackProxy,
		const struct FGeometryCacheMeshBatchInfo& BatchInfo,
		class FGeometryCacheVertexFactoryUserDataWrapper& UserDataWrapper,
		FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer,
		FMeshBatch& Mesh) const;

	/** Function used to create a new track proxy at construction */
	TFunction<FGeomCacheTrackProxy*()> CreateTrackProxy;
};
