#pragma once
#include <variant>
#include "Core/Container/TArray.h"
#include "Core/Name.h"
#include "Core/Math/Vector.h"
#include "Core/Math/Rotator.h"
#include "Core/Object/WeakObjectPtr.h"
#include "Core/Core.h"
#include "Core/enum.h"
#include "Core/Math/Color.h"

class AActor;
class UBillboardComponent;
class UMeshComponent;
class UStaticMeshComponent;
class UParticleSubUVComponent;

/* Editor Commands */
/* SceneManager Commands */
struct FNewSceneCommand {};
struct FSaveSceneCommand { FString SceneName; };
struct FLoadSceneCommand { FString SceneName; };

struct FSpawnStaticMeshActorCommand { FName ActorName; FName MeshAssetName; int32 SpawnCount; };
struct FSetStaticMeshCommand { TWeakObjectPtr<UStaticMeshComponent> Target; FName AssetName; };
struct FSetComponentUseUVScrolltoXCommand { TWeakObjectPtr<UStaticMeshComponent> Target; int32 SlotIndex; bool bUVScrolltoX; };
struct FSetComponentUseUVScrolltoYCommand { TWeakObjectPtr<UStaticMeshComponent> Target; int32 SlotIndex; bool bUVScrolltoY; };
struct FSetComponentUseUVScrollSpeedCommand { TWeakObjectPtr<UStaticMeshComponent> Target; int32 SlotIndex; float UVScrollSpeed; };
struct FSetMaterialOverrideCommand { TWeakObjectPtr<UMeshComponent> Target; int32 SlotIndex; FName AssetName; };
struct FClearMaterialOverrideCommand { TWeakObjectPtr<UMeshComponent> Target; int32 SlotIndex; };
struct FImportObjAssetCommand { FString SourcePath; };
struct FOpenStaticMeshImportCommand { FString DestinationDirectory; };
struct FDeleteActorCommand { TWeakObjectPtr<AActor> Target; };
struct FSpawnParticleCommand { };

struct FSetActorLocationCommand { TWeakObjectPtr<AActor> Target; FVector Location; };
struct FSetActorRotationCommand { TWeakObjectPtr<AActor> Target; FRotator Rotation; };
struct FSetActorScaleCommand { TWeakObjectPtr<AActor> Target; FVector Scale; };
struct FSetActorNameCommand { TWeakObjectPtr<AActor> Target; FName NewName; };
struct FSetSelectedActorCommand { TWeakObjectPtr<AActor> Target; };

struct FSetComponentColorCommand { TWeakObjectPtr<UBillboardComponent> Target; FLinearColor Color; };
struct FSetParticleSubUVComponentLoopingCommand { TWeakObjectPtr<UParticleSubUVComponent> Target; bool bLooping; };
struct FSetParticleSubUVComponentPlayRateCommand { TWeakObjectPtr<UParticleSubUVComponent> Target; float PlayRate; };
struct FSetParticleSubUVComponentBlendStateTypeCommand { TWeakObjectPtr<UParticleSubUVComponent> Target; EBlendStateType BlendStateType; };

/* EditorViewportClient Commands */
struct FSetViewModeCommand { EViewModeIndex ViewMode; };
struct FSetShowFlagCommand { uint32 ShowFlags; };
struct FSetCameraSensitivityCommand { float Sensitivity; };
struct FSetCameraFovCommand { float Fov; };
struct FSetCameraLocationCommand { FVector Location; };
struct FSetCameraRotationCommand { FRotator Rotation; };
struct FSetGizmoModeCommand { EGIZMO_TYPE GizmoMode; };
struct FCycleGizmoModeCommand {};
struct FSetGizmoWorldModeCommand { bool bWorldMode; };


struct FSetGridWidthCommand { float GridWidth; };
struct FStartProjectionTransitionCommand { bool bOrthographic; };

struct FSetRatioHCommand { float RatioH; };
struct FSetRatioVCommand{ float RatioV; };

using FEditorCommand = std::variant <
	FNewSceneCommand,
	FSaveSceneCommand,
	FLoadSceneCommand,

	FSpawnStaticMeshActorCommand,
	FSetStaticMeshCommand,
	FSetMaterialOverrideCommand,
	FClearMaterialOverrideCommand,
	FImportObjAssetCommand,
	FOpenStaticMeshImportCommand,
	FDeleteActorCommand,
	FSpawnParticleCommand,

	FSetActorLocationCommand,
	FSetActorRotationCommand,
	FSetActorScaleCommand,
	FSetActorNameCommand,
	FSetSelectedActorCommand,

	FSetComponentColorCommand,
	FSetParticleSubUVComponentLoopingCommand,
	FSetParticleSubUVComponentPlayRateCommand,
	FSetParticleSubUVComponentBlendStateTypeCommand,

	FSetViewModeCommand,
	FSetShowFlagCommand,
	FSetCameraSensitivityCommand,
	FSetCameraFovCommand,
	FSetCameraLocationCommand,
	FSetCameraRotationCommand,
	FSetGizmoModeCommand,
	FCycleGizmoModeCommand,
	FSetGizmoWorldModeCommand,

	FSetGridWidthCommand,
	FStartProjectionTransitionCommand,


	FSetComponentUseUVScrolltoXCommand,
	FSetComponentUseUVScrolltoYCommand,
	FSetComponentUseUVScrollSpeedCommand,

	FSetRatioHCommand,
	FSetRatioVCommand

> ;
using FEditorCommands = TArray<FEditorCommand>;
