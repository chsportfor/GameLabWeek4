#include "Core/Core.h"
#include "Core/enum.h"
#include "Core/Math/Color.h"

/* Editor Commands */
/* SceneManager Commands */
struct FNewSceneCommand {};
struct FSaveSceneCommand { FString SceneName; };
struct FLoadSceneCommand { FString SceneName; };

struct FSpawnActorCommand { EPrimitive PrimitiveType; int32 SpawnCount; };
struct FDeleteActorCommand { FObjectID ObjectID; };
struct FSpawnParticleCommand { };

struct FSetActorLocationCommand { FObjectID ObjectID; FVector Location; };
struct FSetActorRotationCommand { FObjectID ObjectID; FRotator Rotation; };
struct FSetActorScaleCommand { FObjectID ObjectID; FVector Scale; };
struct FSetActorNameCommand { FObjectID ObjectID; FName NewName; };
struct FSetSelectedActorCommand { FObjectID ObjectID; };

struct FSetComponentUseTextureCommand { FObjectID ObjectID; bool bUseTexture; };
struct FSetComponentColorCommand { FObjectID ObjectID; FLinearColor Color; };
struct FSetSphereComponentSpinCommand { FObjectID ObjectID; bool bSpin; };
struct FSetSphereComponentSpinSpeedCommand { FObjectID ObjectID; float SpinSpeed; };
struct FSetParticleSubUVComponentLoopingCommand { FObjectID ObjectID; bool bLooping; };
struct FSetParticleSubUVComponentPlayRateCommand { FObjectID ObjectID; float PlayRate; };
struct FSetParticleSubUVComponentBlendStateTypeCommand { FObjectID ObjectID; EBlendStateType BlendStateType; };

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
