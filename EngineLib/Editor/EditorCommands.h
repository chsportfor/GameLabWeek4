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
class UPrimitiveComponent;
class UMeshComponent;
class UStaticMeshComponent;
class USphereComponent;
class UParticleSubUVComponent;

/* Editor Commands */
/* SceneManager Commands */
struct FNewSceneCommand {};
struct FSaveSceneCommand { FString SceneName; };
struct FLoadSceneCommand { FString SceneName; };

struct FSpawnActorCommand { EPrimitive PrimitiveType; int32 SpawnCount; };
struct FSpawnStaticMeshActorCommand { int32 SpawnCount; };
struct FSetStaticMeshCommand { TWeakObjectPtr<UStaticMeshComponent> Target; FName AssetName; };
struct FSetMaterialOverrideCommand { TWeakObjectPtr<UMeshComponent> Target; int32 SlotIndex; FName AssetName; };
struct FClearMaterialOverrideCommand { TWeakObjectPtr<UMeshComponent> Target; int32 SlotIndex; };
struct FImportObjAssetCommand { FString SourcePath; };
struct FToggleObjViewerCommand {};
struct FDeleteActorCommand { TWeakObjectPtr<AActor> Target; };
struct FSpawnParticleCommand { };

struct FSetActorLocationCommand { TWeakObjectPtr<AActor> Target; FVector Location; };
struct FSetActorRotationCommand { TWeakObjectPtr<AActor> Target; FRotator Rotation; };
struct FSetActorScaleCommand { TWeakObjectPtr<AActor> Target; FVector Scale; };
struct FSetActorNameCommand { TWeakObjectPtr<AActor> Target; FName NewName; };
struct FSetSelectedActorCommand { TWeakObjectPtr<AActor> Target; };

struct FSetComponentUseTextureCommand { TWeakObjectPtr<UPrimitiveComponent> Target; bool bUseTexture; };
struct FSetComponentColorCommand { TWeakObjectPtr<UPrimitiveComponent> Target; FLinearColor Color; };
struct FSetSphereComponentSpinCommand { TWeakObjectPtr<USphereComponent> Target; bool bSpin; };
struct FSetSphereComponentSpinSpeedCommand { TWeakObjectPtr<USphereComponent> Target; float SpinSpeed; };
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


struct FSetGridWidthCommand { float GridWidth; };
struct FStartProjectionTransitionCommand { bool bOrthographic; };

using FEditorCommand = std::variant <
	FNewSceneCommand,
	FSaveSceneCommand,
	FLoadSceneCommand,

	FSpawnActorCommand,
	FSpawnStaticMeshActorCommand,
	FSetStaticMeshCommand,
	FSetMaterialOverrideCommand,
	FClearMaterialOverrideCommand,
	FImportObjAssetCommand,
	FToggleObjViewerCommand,
	FDeleteActorCommand,
	FSpawnParticleCommand,

	FSetActorLocationCommand,
	FSetActorRotationCommand,
	FSetActorScaleCommand,
	FSetActorNameCommand,
	FSetSelectedActorCommand,

	FSetComponentUseTextureCommand,
	FSetComponentColorCommand,
	FSetSphereComponentSpinCommand,
	FSetSphereComponentSpinSpeedCommand,
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

	FSetGridWidthCommand,
	FStartProjectionTransitionCommand
> ;
using FEditorCommands = TArray<FEditorCommand>;
