// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniEngineSettings.h"

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineCommon.h"


void UHoudiniEngineSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UHoudiniEngineSettings, bCookOnInputChanged))
	{
		if (!bCookOnInputChanged)
		{
			if (FMessageDialog::Open(EAppMsgType::OkCancel, NSLOCTEXT(HOUDINI_LOCTEXT_NAMESPACE, "CookOnInputChangedConfirm", "Turn off means all inputs including:\nActors, Landscapes, Curves, etc.\nchanged will NEVER trigger node cook!\n\nDo you want disable it?")) == EAppReturnType::Cancel)
				bCookOnInputChanged = true;
		}
	}
	else if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(FDirectoryPath, Path))
	{
		CustomHoudiniLocation.Path.RemoveFromEnd(TEXT("/"));
		CustomHoudiniLocation.Path.RemoveFromEnd(TEXT("/" HAPI_HOUDINI_BIN_DIR));
#if PLATFORM_MAC
		CustomHoudiniLocation.Path.RemoveFromEnd(TEXT("/" HAPI_LIB_DIR));
		CustomHoudiniLocation.Path.RemoveFromEnd(TEXT("/Libraries"));
		if (!CustomHoudiniLocation.Path.EndsWith(TEXT("/Resources")))
			CustomHoudiniLocation.Path += TEXT("/Resources");
#elif PLATFORM_LINUX
		CustomHoudiniLocation.Path.RemoveFromEnd(TEXT("/" HAPI_LIB_DIR));
#endif

		if (FHoudiniEngine::Get().GetHoudiniDir() == CustomHoudiniLocation.Path)  // This houdini has been loaded
			return;

		if (FPaths::FileExists(CustomHoudiniLocation.Path + TEXT("/" HAPI_LIB_DIR "/" HAPI_LIB_OBJECT)))
		{
			if (!FHoudiniEngine::Get().IsNullSession())  // If session is started, then we should close it and invalidate data first
			{
				FHoudiniEngine::Get().HapiStopSession();
				FHoudiniEngine::Get().InvalidateSessionData();
			}

			FHoudiniEngine::Get().InitializeHAPI();  // Reload HAPILib
		}
		else
			CustomHoudiniLocation.Path.Empty();
	}
	else if ((PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UHoudiniEngineSettings, SessionType)) ||
		(PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UHoudiniEngineSettings, SharedMemoryBufferSize)) ||
		(PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UHoudiniEngineSettings, bVerbose)))
	{
		if (!FHoudiniEngine::Get().IsNullSession())
			FHoudiniEngine::Get().HapiStopSession();
		FHoudiniEngine::Get().InvalidateSessionData();
	}
	else if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UHoudiniEngineSettings, HoudiniEngineFolder))
	{
		if (!FHoudiniEngine::Get().IsNullSession())  // Update houdini engine folder env var
		{
			const HAPI_Result HapiResult = FHoudiniApi::SetServerEnvString(FHoudiniEngine::Get().GetSession(),
				HAPI_ENV_HOUDINI_ENGINE_FOLDER, TCHAR_TO_UTF8(*HoudiniEngineFolder));
			if (HAPI_SESSION_INVALID_RESULT(HapiResult))
				FHoudiniEngine::Get().InvalidateSessionData();
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
