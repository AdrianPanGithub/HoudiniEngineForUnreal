// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniEngineUtils.h"

#include "HAPI/HAPI_Version.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineSettings.h"
#include "HoudiniApi.h"
#include "HoudiniEngineCommon.h"


static FString GetValidHoudiniDir(FString HoudiniDir)
{
	if (!HoudiniDir.IsEmpty())
	{
		HoudiniDir.ReplaceCharInline(TCHAR('\\'), TCHAR('/'), ESearchCase::CaseSensitive);
		HoudiniDir.ReplaceInline(TEXT("//"), TEXT("/"), ESearchCase::CaseSensitive);
		HoudiniDir.RemoveFromEnd(TEXT("/"));
		HoudiniDir.RemoveFromEnd(TEXT("/" HAPI_HOUDINI_BIN_DIR));
#if PLATFORM_MAC
		HoudiniDir.RemoveFromEnd(TEXT("/" HAPI_LIB_DIR));
		HoudiniDir.RemoveFromEnd(TEXT("/Libraries"));
		if (!HoudiniDir.EndsWith(TEXT("/Resources")))
			HoudiniDir += TEXT("/Resources");
#elif PLATFORM_LINUX
		HoudiniDir.RemoveFromEnd(TEXT("/" HAPI_LIB_DIR));
#endif
		if (FPaths::FileExists(HoudiniDir + TEXT("/" HAPI_LIB_DIR "/" HAPI_LIB_OBJECT)))
			return HoudiniDir;
	}

	return FString();
}

static void TryFindHoudiniDir(FString& OutHoudiniDir);  // Search valid Houdini installation in system

void FHoudiniEngine::InitializeHAPI()
{
	if (LoadedHAPILibraryHandle)
	{
		FHoudiniApi::FinalizeHAPI();
		FPlatformProcess::FreeDllHandle(LoadedHAPILibraryHandle);
	}

	LoadedHoudiniDir = GetValidHoudiniDir(GetDefault<UHoudiniEngineSettings>()->CustomHoudiniLocation.Path);

	if (LoadedHoudiniDir.IsEmpty())  // Look up HAPI_PATH environment variable.
		LoadedHoudiniDir = GetValidHoudiniDir(FPlatformMisc::GetEnvironmentVariable(TEXT("HAPI_PATH")));

	if (LoadedHoudiniDir.IsEmpty())  // Look up HFS environment variable.
		LoadedHoudiniDir = GetValidHoudiniDir(FPlatformMisc::GetEnvironmentVariable(TEXT("HFS")));

	if (LoadedHoudiniDir.IsEmpty())
		TryFindHoudiniDir(LoadedHoudiniDir);

	if (LoadedHoudiniDir.IsEmpty())
	{
		UE_LOG(LogHoudiniEngine, Warning, TEXT("No Houdini >= %d.%d.%d found. Please specify a Houdini location manually in plugin settings"),
			HAPI_VERSION_HOUDINI_MAJOR, HAPI_VERSION_HOUDINI_MINOR, HAPI_VERSION_HOUDINI_BUILD);
		return;
	}

	LoadedHAPILibraryHandle = FPlatformProcess::GetDllHandle(*(LoadedHoudiniDir + TEXT("/" HAPI_LIB_DIR "/" HAPI_LIB_OBJECT)));
	if (LoadedHAPILibraryHandle)
		FHoudiniApi::InitializeHAPI(LoadedHAPILibraryHandle);

	if (FHoudiniApi::IsHAPIInitialized())
	{
		StartTick();
		UE_LOG(LogHoudiniEngine, Log, TEXT("Loaded from Houdini directory: %s"), *LoadedHoudiniDir);
	}
	else
	{
		if (LoadedHAPILibraryHandle)
		{
			FHoudiniApi::FinalizeHAPI();
			FPlatformProcess::FreeDllHandle(LoadedHAPILibraryHandle);
			LoadedHAPILibraryHandle = nullptr;
		}

		UE_LOG(LogHoudiniEngine, Error, TEXT("Failed to load \"%s/%s\", please re-install Houdini"), *LoadedHoudiniDir, TEXT(HAPI_LIB_DIR "/" HAPI_LIB_OBJECT));
		LoadedHoudiniDir.Empty();
	}
}


#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"

void TryFindHoudiniDir(FString& OutHoudiniDir)
{
	HKEY hKey;
	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("Software\\Side Effects Software\\Houdini"), 0, KEY_READ, &hKey) == ERROR_SUCCESS)
	{
		const FString HoudiniMainVersionStr = FString::Printf(TEXT("%d.%d.0."), HAPI_VERSION_HOUDINI_MAJOR, HAPI_VERSION_HOUDINI_MINOR);

		int32 NewestHoudiniBuild = HAPI_VERSION_HOUDINI_BUILD - 1;

		// Enumerate the values
		DWORD dwIndex = 0;
		TCHAR valueName[MAX_PATH];
		DWORD valueNameSize = MAX_PATH;
		LONG lResult;
		while ((lResult = RegEnumValue(hKey, dwIndex, valueName, &valueNameSize, NULL, NULL, NULL, NULL)) == ERROR_SUCCESS)
		{
			FString Key(valueName);
			if (Key.RemoveFromStart(HoudiniMainVersionStr))
			{
				const int32 HoudiniBuild = FCString::Atoi(*Key);
				if (NewestHoudiniBuild < HoudiniBuild)
				{
					TCHAR houdiniInstallPath[MAX_PATH];  // Assuming the value is a string
					DWORD bufferSize = sizeof(houdiniInstallPath);
					if (RegQueryValueEx(hKey, valueName, NULL, NULL, (LPBYTE)houdiniInstallPath, &bufferSize) == ERROR_SUCCESS)
					{
						const FString FoundHoudiniDir = GetValidHoudiniDir(houdiniInstallPath);
						if (!FoundHoudiniDir.IsEmpty())
						{
							OutHoudiniDir = FoundHoudiniDir;
							NewestHoudiniBuild = HoudiniBuild;
						}
					}
				}
			}

			dwIndex++;
			valueNameSize = MAX_PATH;
		}

		RegCloseKey(hKey);
	}
}

const float* FHoudiniEngineUtils::GetSharedMemory(const TCHAR* SHMPath, const size_t& Size32, size_t& OutHandle)
{
	HANDLE hMapFile = OpenFileMapping(
		FILE_MAP_READ,   // read/write access
		false,           // do not inherit the name
		SHMPath);        // name of mapping object

	if (hMapFile != NULL)
	{
		const void* shm_data = MapViewOfFile(hMapFile,   // handle to map object
			FILE_MAP_READ, // read/write permission
			0,
			0,
			Size32 * sizeof(float));

		if (shm_data)
		{
			OutHandle = (size_t)hMapFile;
			return (const float*)shm_data;
		}
	}
	
	return nullptr;
}

float* FHoudiniEngineUtils::FindOrCreateSharedMemory(const TCHAR* SHMPath, const size_t& Size32, size_t& InOutHandle, bool& bOutFound)
{
	HANDLE hMapFile = OpenFileMapping(
		FILE_MAP_ALL_ACCESS,   // read/write access
		false,                 // do not inherit the name
		SHMPath);               // name of mapping object

	if (!hMapFile)
	{
		bOutFound = false;
		hMapFile = CreateFileMapping(
			INVALID_HANDLE_VALUE,    // use paging file
			NULL,                    // default security
			PAGE_READWRITE,          // read/write access
			0,                       // maximum object size (high-order DWORD)
			Size32 * sizeof(float),       // maximum object size (low-order DWORD)
			SHMPath);                 // name of mapping object
	}
	else
		bOutFound = true;

	if (HANDLE(InOutHandle) != hMapFile)
	{
		if (InOutHandle)
			CloseHandle(HANDLE(InOutHandle));
	}
	InOutHandle = (size_t)hMapFile;

	if (!hMapFile)
		return nullptr;

	return (float*)MapViewOfFile(hMapFile,   // handle to map object
		FILE_MAP_ALL_ACCESS, // read/write permission
		0,
		0,
		Size32 * sizeof(float));
}

void FHoudiniEngineUtils::UnmapSharedMemory(const void* SHM)
{
	if (SHM)
		UnmapViewOfFile(SHM);
}

void FHoudiniEngineUtils::CloseSharedMemoryHandle(const size_t& Handle)
{
	if (Handle)
		CloseHandle(HANDLE(Handle));
}

// Of course, Windows defines its own GetGeoInfo, so we need undefine to avoid collision
#ifdef GetGeoInfo
#undef GetGeoInfo
#endif

#else

#include <filesystem>

static void TryFindHoudiniDir(FString& OutHoudiniDir)
{
	int32 NewestHoudiniBuild = HAPI_VERSION_HOUDINI_BUILD - 1;

#if PLATFORM_MAC
	const FString DirPrefix = FString::Printf(TEXT("Houdini%d.%d."), HAPI_VERSION_HOUDINI_MAJOR, HAPI_VERSION_HOUDINI_MINOR);
	const FString DirSuffix = FString::Printf(TEXT("/Frameworks/Houdini.framework/Versions/%d.%d/Resources"), HAPI_VERSION_HOUDINI_MAJOR, HAPI_VERSION_HOUDINI_MINOR);
	for (const auto& found_dir : std::filesystem::directory_iterator("/Applications/Houdini"))
#else
	const FString DirPrefix = FString::Printf(TEXT("hfs%d.%d."), HAPI_VERSION_HOUDINI_MAJOR, HAPI_VERSION_HOUDINI_MINOR);
	for (const auto& found_dir : std::filesystem::directory_iterator("/opt"))
#endif
	{
		if (found_dir.is_directory())
		{
			const FString HoudiniDir = UTF8_TO_TCHAR(found_dir.path().filename().c_str());
			FString HoudiniBuildVerStr = HoudiniDir;
			if (HoudiniBuildVerStr.RemoveFromStart(DirPrefix))
			{
				const int32 HoudiniBuild = FCString::Atoi(*HoudiniBuildVerStr);
				if (NewestHoudiniBuild < HoudiniBuild)
				{
#if PLATFORM_MAC
					const FString FoundHoudiniDir = GetValidHoudiniDir(TEXT("/Applications/Houdini/") + HoudiniDir + DirSuffix);
#else
					const FString FoundHoudiniDir = GetValidHoudiniDir(TEXT("/opt/") + HoudiniDir);
#endif
					if (!FoundHoudiniDir.IsEmpty())
					{
						OutHoudiniDir = FoundHoudiniDir;
						NewestHoudiniBuild = HoudiniBuild;
					}
				}
			}
		}
	}
#if PLATFORM_MAC
	if (OutHoudiniDir.IsEmpty())
		OutHoudiniDir = GetValidHoudiniDir(TEXT("/Users/Shared/Houdini/HoudiniIndieSteam/Frameworks/Houdini.framework/Versions/Current/Resources"));
#endif
}

#include <sys/mman.h>

static TMap<const void*, size_t> GHoudiniEngineSHMSizeMap;

const float* FHoudiniEngineUtils::GetSharedMemory(const TCHAR* SHMPath, const size_t& Size32, size_t& OutHandle)
{
	int shm_fd = shm_open(TCHAR_TO_UTF8(SHMPath), O_RDONLY, 0666);
	if (shm_fd >= 0) {
		const void* shm_data = mmap(NULL, Size32 * sizeof(float), PROT_READ, MAP_SHARED, shm_fd, 0);
		if (shm_data) {
			GHoudiniEngineSHMSizeMap.FindOrAdd(shm_data, Size32 * sizeof(float));
			OutHandle = (size_t(shm_fd) + 1);
			return (const float*)shm_data;
		}
	}
    
	return nullptr;
}

static TMap<int, std::string> GHoudiniEngineSHMPathMap;

float* FHoudiniEngineUtils::FindOrCreateSharedMemory(const TCHAR* SHMPath, const size_t& Size32, size_t& InOutHandle, bool& bOutFound)
{
	const std::string shm_path = TCHAR_TO_UTF8(SHMPath);
	int shm_fd = shm_open(shm_path.c_str(), O_RDWR, 0666);
	
	if (shm_fd < 0)
	{
		bOutFound = false;
		if (InOutHandle)
			CloseSharedMemoryHandle(InOutHandle);
		
		shm_fd = shm_open(shm_path.c_str(), O_CREAT | O_RDWR, 0666);
		if (ftruncate(shm_fd, Size32 * sizeof(float)) == -1)
			return nullptr;
	}
	else {
		bOutFound = true;
		if (InOutHandle && (int(InOutHandle - 1) != shm_fd)) {
			close(int(InOutHandle - 1));  // Only close the old file desc, NOT to unlink
			GHoudiniEngineSHMPathMap.Remove(int(InOutHandle - 1));
		}
	}
	
	GHoudiniEngineSHMPathMap.FindOrAdd(shm_fd, shm_path);

	InOutHandle = (size_t(shm_fd) + 1);

	if (!InOutHandle)
		return nullptr;
	
	float* shm_data = (float*)mmap(NULL, Size32 * sizeof(float), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
	if (shm_data)
		GHoudiniEngineSHMSizeMap.FindOrAdd(shm_data, Size32 * sizeof(float));

	return shm_data;
}

void FHoudiniEngineUtils::UnmapSharedMemory(const void* SHM)
{
	if (SHM)
	{
		size_t found_shm_size = 0;
		if (GHoudiniEngineSHMSizeMap.RemoveAndCopyValue(SHM, found_shm_size))
			munmap((void*)SHM, found_shm_size);
	}
}

void FHoudiniEngineUtils::CloseSharedMemoryHandle(const size_t& Handle)
{
	if (Handle)
	{
		close(int(Handle - 1));
		std::string shm_path;
		if (GHoudiniEngineSHMPathMap.RemoveAndCopyValue(int(Handle - 1), shm_path))
			shm_unlink(shm_path.c_str());
	}
}

#endif
