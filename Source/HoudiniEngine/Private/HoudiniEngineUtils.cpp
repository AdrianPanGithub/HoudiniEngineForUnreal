// Copyright Yuzhe Pan (childadrianpan@gmail.com). All Rights Reserved.

#include "HoudiniEngineUtils.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"

#include "HoudiniEngineCommon.h"
#include "HoudiniEngine.h"
#include "HoudiniApi.h"


bool FHoudiniEngineUtils::HapiConvertStringHandle(const HAPI_StringHandle& InSH, FString& OutString)
{
    if (InSH == 0)  // 0 is null string, and should be empty
    {
        OutString.Empty();
        return true;
    }

    int32 StringLength = 0;
    HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetStringBufLength(FHoudiniEngine::Get().GetSession(), InSH, &StringLength));

    if (StringLength <= 0)
    {
        OutString.Empty();
        return true;
    }

    TArray<char> Buffer;
    Buffer.SetNumUninitialized(StringLength);
    HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetString(FHoudiniEngine::Get().GetSession(), InSH, Buffer.GetData(), StringLength));
    OutString = FString(UTF8_TO_TCHAR(Buffer.GetData()));  // Different from std::string

    return true;
}

bool FHoudiniEngineUtils::HapiConvertStringHandle(const HAPI_StringHandle& InSH, std::string& OutString)
{
    if (InSH == 0)  // 0 is null string, and should be empty
    {
        OutString.clear();
        return true;
    }

    int32 StringLength = 0;
    HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetStringBufLength(FHoudiniEngine::Get().GetSession(), InSH, &StringLength));

    if (StringLength <= 0)
    {
        OutString.clear();
        return true;
    }

    TArray<char> Buffer;
    Buffer.SetNumUninitialized(StringLength);
    HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetString(FHoudiniEngine::Get().GetSession(), InSH, Buffer.GetData(), StringLength));
    OutString = Buffer.GetData();  // Different from FString

    return true;
}

bool FHoudiniEngineUtils::HapiGetStringHandles(const TArray<HAPI_StringHandle>& InSHs, TArray<char>& Buffer)
{
    int32 BufferSize = 0;
    HAPI_Result Result = FHoudiniApi::GetStringBatchSize(FHoudiniEngine::Get().GetSession(), InSHs.GetData(), InSHs.Num(), &BufferSize);
    if (HAPI_RESULT_SUCCESS != Result)
        return !(HAPI_SESSION_INVALID_RESULT(Result));

    if (BufferSize <= 0)
        return true;

    Buffer.SetNumUninitialized(BufferSize);
    Result = FHoudiniApi::GetStringBatch(FHoudiniEngine::Get().GetSession(), Buffer.GetData(), BufferSize);
    if (HAPI_RESULT_SUCCESS != Result)
        return !(HAPI_SESSION_INVALID_RESULT(Result));

    return true;
}

bool FHoudiniEngineUtils::HapiConvertStringHandles(const TArray<HAPI_StringHandle>& InSHs, TArray<FString>& OutStrings)
{
    OutStrings.SetNum(InSHs.Num());

    if (InSHs.IsEmpty())
        return true;
    else if (InSHs.Num() == 1)
        return HapiConvertStringHandle(InSHs[0], OutStrings[0]);

    TArray<HAPI_StringHandle> UniqueSHs;
    {
        TSet<HAPI_StringHandle> SHSet(InSHs);
        SHSet.Remove(0);
        UniqueSHs = SHSet.Array();
    }

    if (UniqueSHs.IsEmpty())
        return true;

    int32 BufferSize = 0;
    HAPI_Result Result = FHoudiniApi::GetStringBatchSize(FHoudiniEngine::Get().GetSession(), UniqueSHs.GetData(), UniqueSHs.Num(), &BufferSize);
    if (HAPI_RESULT_SUCCESS != Result)
        return !(HAPI_SESSION_INVALID_RESULT(Result));

    if (BufferSize <= 0)
        return true;

    TArray<char> Buffer;
    Buffer.SetNumUninitialized(BufferSize);
    Result = FHoudiniApi::GetStringBatch(FHoudiniEngine::Get().GetSession(), Buffer.GetData(), BufferSize);
    if (HAPI_RESULT_SUCCESS != Result)
        return !(HAPI_SESSION_INVALID_RESULT(Result));

    // Parse the buffer to a string array
    TMap<HAPI_StringHandle, FString> StringMap;  // Different from std::string
    int32 StringIdx = 0;
    int32 CharOffset = 0;
    while (CharOffset < BufferSize)
    {
        // Add the current string to our dictionary.
        StringMap.Add(UniqueSHs[StringIdx], FString(UTF8_TO_TCHAR(&Buffer[CharOffset])));  // Different from std::string

        ++StringIdx;  // Move on to next indexed string
        CharOffset += strlen(&Buffer[CharOffset]) + 1;
    }

    if (StringMap.Num() != UniqueSHs.Num())
        return false;

    // Fill the output array using the map
    for (int32 SHIdx = 0; SHIdx < InSHs.Num(); ++SHIdx)
    {
        if (InSHs[SHIdx] == 0)  // 0 is null string, and should be empty
            OutStrings[SHIdx].Empty();
        else
            OutStrings[SHIdx] = StringMap[InSHs[SHIdx]];
    }

    return true;
}

bool FHoudiniEngineUtils::HapiConvertStringHandles(const TArray<HAPI_StringHandle>& InSHs, TArray<std::string>& OutStrings)
{
    OutStrings.SetNum(InSHs.Num());
    
    if (InSHs.IsEmpty())
        return true;
    else if (InSHs.Num() == 1)
        return HapiConvertStringHandle(InSHs[0], OutStrings[0]);

    TArray<HAPI_StringHandle> UniqueSHs;
    {
        TSet<HAPI_StringHandle> SHSet(InSHs);
        SHSet.Remove(0);  // 0 is null string, and should be empty
        UniqueSHs = SHSet.Array();
    }

    if (UniqueSHs.IsEmpty())
        return true;

    int32 BufferSize = 0;
    HAPI_Result Result = FHoudiniApi::GetStringBatchSize(FHoudiniEngine::Get().GetSession(), UniqueSHs.GetData(), UniqueSHs.Num(), &BufferSize);
    if (HAPI_RESULT_SUCCESS != Result)
        return !(HAPI_SESSION_INVALID_RESULT(Result));

    if (BufferSize <= 0)
        return true;

    TArray<char> Buffer;
    Buffer.SetNumUninitialized(BufferSize);
    Result = FHoudiniApi::GetStringBatch(FHoudiniEngine::Get().GetSession(), Buffer.GetData(), BufferSize);
    if (HAPI_RESULT_SUCCESS != Result)
        return !(HAPI_SESSION_INVALID_RESULT(Result));

    // Parse the buffer to a string array
    TMap<HAPI_StringHandle, std::string> StringMap;  // Different from FString
    int32 StringIdx = 0;
    int32 CharOffset = 0;
    while (CharOffset < BufferSize)
    {
        // Add the current string to our dictionary.
        std::string String(Buffer.GetData() + CharOffset);
        StringMap.Add(UniqueSHs[StringIdx], String);

        ++StringIdx;  // Move on to next indexed string
        CharOffset += String.length() + 1;
    }

    if (StringMap.Num() != UniqueSHs.Num())
        return false;

    // Fill the output array using the map
    for (int32 SHIdx = 0; SHIdx < InSHs.Num(); ++SHIdx)
    {
        if (InSHs[SHIdx] == 0)  // 0 is null string, and should be empty
            OutStrings[SHIdx].clear();
        else
            OutStrings[SHIdx] = StringMap[InSHs[SHIdx]];
    }

    return true;
}

bool FHoudiniEngineUtils::HapiConvertUniqueStringHandles(const TArray<HAPI_StringHandle>& UniqueSHs, TArray<FString>& OutStrings)
{
    OutStrings.SetNum(UniqueSHs.Num());

    if (UniqueSHs.IsEmpty())
        return true;
    else if (UniqueSHs.Num() == 1)
        return HapiConvertStringHandle(UniqueSHs[0], OutStrings[0]);

    int32 BufferSize = 0;
    HAPI_Result Result = FHoudiniApi::GetStringBatchSize(FHoudiniEngine::Get().GetSession(), UniqueSHs.GetData(), UniqueSHs.Num(), &BufferSize);
    if (HAPI_RESULT_SUCCESS != Result)
        return !(HAPI_SESSION_INVALID_RESULT(Result));

    if (BufferSize <= 0)
        return true;

    TArray<char> Buffer;
    Buffer.SetNumUninitialized(BufferSize);
    Result = FHoudiniApi::GetStringBatch(FHoudiniEngine::Get().GetSession(), Buffer.GetData(), BufferSize);
    if (HAPI_RESULT_SUCCESS != Result)
        return !(HAPI_SESSION_INVALID_RESULT(Result));

    // Parse the buffer to a string array
    int32 StringIdx = 0;
    int32 CharOffset = 0;
    while (CharOffset < BufferSize)
    {
        OutStrings[StringIdx] = UTF8_TO_TCHAR(&Buffer[CharOffset]);
        
        ++StringIdx;  // Move on to next indexed string
        CharOffset += strlen(&Buffer[CharOffset]) + 1;
    }

    return true;
}

bool FHoudiniEngineUtils::HapiConvertUniqueStringHandles(const TArray<HAPI_StringHandle>& UniqueSHs, TArray<std::string>& OutStrings)
{
    OutStrings.SetNum(UniqueSHs.Num());

    if (UniqueSHs.IsEmpty())
        return true;
    else if (UniqueSHs.Num() == 1)
        return HapiConvertStringHandle(UniqueSHs[0], OutStrings[0]);

    int32 BufferSize = 0;
    HAPI_Result Result = FHoudiniApi::GetStringBatchSize(FHoudiniEngine::Get().GetSession(), UniqueSHs.GetData(), UniqueSHs.Num(), &BufferSize);
    if (HAPI_RESULT_SUCCESS != Result)
        return !(HAPI_SESSION_INVALID_RESULT(Result));

    if (BufferSize <= 0)
        return true;

    TArray<char> Buffer;
    Buffer.SetNumUninitialized(BufferSize);
    Result = FHoudiniApi::GetStringBatch(FHoudiniEngine::Get().GetSession(), Buffer.GetData(), BufferSize);
    if (HAPI_RESULT_SUCCESS != Result)
        return !(HAPI_SESSION_INVALID_RESULT(Result));

    // Parse the buffer to a string array
    int32 StringIdx = 0;
    int32 CharOffset = 0;
    while (CharOffset < BufferSize)
    {
        OutStrings[StringIdx] = Buffer.GetData() + CharOffset;

        CharOffset += OutStrings[StringIdx].length() + 1;
        ++StringIdx;  // Move on to next indexed string
    }

    return true;
}


bool FHoudiniEngineUtils::HapiGetStatusString(const HAPI_StatusType& StatusType, const HAPI_StatusVerbosity& StatusVerbosity, FString& OutStatusString)
{
    int32 StatusBufferLength = 0;
    HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetStatusStringBufLength(FHoudiniEngine::Get().GetSession(),
        StatusType, StatusVerbosity, &StatusBufferLength));

    TArray<char> StatusStringBuffer;
    StatusStringBuffer.SetNumZeroed(StatusBufferLength + 1);
    HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetStatusString(FHoudiniEngine::Get().GetSession(),
        StatusType, StatusStringBuffer.GetData(), StatusBufferLength));

    OutStatusString = StatusStringBuffer.GetData();

    return true;
}

void FHoudiniEngineUtils::PrintFailedResult(const TCHAR* SourceLocationStr, const HAPI_Result& Result)
{
    switch (Result)
    { 
    case HAPI_RESULT_SUCCESS: return;
    case HAPI_RESULT_FAILURE: UE_LOG(LogHoudiniEngine, Error, TEXT("%s: %s"), SourceLocationStr, TEXT("FAILURE")); return;
    case HAPI_RESULT_ALREADY_INITIALIZED: UE_LOG(LogHoudiniEngine, Error, TEXT("%s: %s"), SourceLocationStr, TEXT("ALREADY_INITIALIZED")); return;
    case HAPI_RESULT_NOT_INITIALIZED: UE_LOG(LogHoudiniEngine, Error, TEXT("%s: %s"), SourceLocationStr, TEXT("NOT_INITIALIZED")); return;
    case HAPI_RESULT_CANT_LOADFILE: UE_LOG(LogHoudiniEngine, Error, TEXT("%s: %s"), SourceLocationStr, TEXT("CANT_LOADFILE")); return;
    case HAPI_RESULT_PARM_SET_FAILED: UE_LOG(LogHoudiniEngine, Error, TEXT("%s: %s"), SourceLocationStr, TEXT("PARM_SET_FAILED")); return;
    case HAPI_RESULT_INVALID_ARGUMENT: UE_LOG(LogHoudiniEngine, Error, TEXT("%s: %s"), SourceLocationStr, TEXT("INVALID_ARGUMENT")); return;
    case HAPI_RESULT_CANT_LOAD_GEO: UE_LOG(LogHoudiniEngine, Error, TEXT("%s: %s"), SourceLocationStr, TEXT("CANT_LOAD_GEO")); return;
    case HAPI_RESULT_CANT_GENERATE_PRESET: UE_LOG(LogHoudiniEngine, Error, TEXT("%s: %s"), SourceLocationStr, TEXT("CANT_GENERATE_PRESET")); return;
    case HAPI_RESULT_CANT_LOAD_PRESET: UE_LOG(LogHoudiniEngine, Error, TEXT("%s: %s"), SourceLocationStr, TEXT("LOAD_PRESET")); return;
    case HAPI_RESULT_ASSET_DEF_ALREADY_LOADED: UE_LOG(LogHoudiniEngine, Error, TEXT("%s: %s"), SourceLocationStr, TEXT("ASSET_DEF_ALREADY_LOADED")); return;

    case HAPI_RESULT_NO_LICENSE_FOUND: UE_LOG(LogHoudiniEngine, Error, TEXT("%s: %s"), SourceLocationStr, TEXT("NO_LICENSE_FOUND")); return;
    case HAPI_RESULT_DISALLOWED_NC_LICENSE_FOUND: UE_LOG(LogHoudiniEngine, Error, TEXT("%s: %s"), SourceLocationStr, TEXT("DISALLOWED_NC_LICENSE_FOUND")); return;
    case HAPI_RESULT_DISALLOWED_NC_ASSET_WITH_C_LICENSE: UE_LOG(LogHoudiniEngine, Error, TEXT("%s: %s"), SourceLocationStr, TEXT("DISALLOWED_NC_ASSET_WITH_C_LICENSE")); return;
    case HAPI_RESULT_DISALLOWED_NC_ASSET_WITH_LC_LICENSE: UE_LOG(LogHoudiniEngine, Error, TEXT("%s: %s"), SourceLocationStr, TEXT("DISALLOWED_NC_ASSET_WITH_LC_LICENSE")); return;
    case HAPI_RESULT_DISALLOWED_LC_ASSET_WITH_C_LICENSE: UE_LOG(LogHoudiniEngine, Error, TEXT("%s: %s"), SourceLocationStr, TEXT("DISALLOWED_LC_ASSET_WITH_C_LICENSE")); return;
    case HAPI_RESULT_DISALLOWED_HENGINEINDIE_W_3PARTY_PLUGIN: UE_LOG(LogHoudiniEngine, Error, TEXT("%s: %s"), SourceLocationStr, TEXT("DISALLOWED_HENGINEINDIE_W_3PARTY_PLUGIN")); return;

    case HAPI_RESULT_ASSET_INVALID: UE_LOG(LogHoudiniEngine, Error, TEXT("%s: %s"), SourceLocationStr, TEXT("ASSET_INVALID")); return;
    case HAPI_RESULT_NODE_INVALID: UE_LOG(LogHoudiniEngine, Error, TEXT("%s: %s"), SourceLocationStr, TEXT("NODE_INVALID")); return;

    case HAPI_RESULT_USER_INTERRUPTED: UE_LOG(LogHoudiniEngine, Error, TEXT("%s: %s"), SourceLocationStr, TEXT("USER_INTERRUPTED")); return;

    case HAPI_RESULT_INVALID_SESSION: UE_LOG(LogHoudiniEngine, Error, TEXT("%s: %s"), SourceLocationStr, TEXT("INVALID_SESSION")); return;
    default:
        UE_LOG(LogHoudiniEngine, Error, TEXT("%s: %s"), SourceLocationStr, TEXT("Unknown Error")); return;
    }
}


// -------- String Relevant --------
FString FHoudiniEngineUtils::ConvertXformToString(const FTransform& Transform, const FVector& Pivot)
{
    const FVector T = Transform.GetLocation();
    const FRotator R = Transform.Rotator();
    const FVector S = Transform.GetScale3D();

    return
        (T.X == 0.0 ? "0.0," : (FString::SanitizeFloat(T.X * POSITION_SCALE_TO_HOUDINI) + ",")) +
        (T.Z == 0.0 ? "0.0," : (FString::SanitizeFloat(T.Z * POSITION_SCALE_TO_HOUDINI) + ",")) +
        (T.Y == 0.0 ? "0.0;" : (FString::SanitizeFloat(T.Y * POSITION_SCALE_TO_HOUDINI) + ";")) +
        (R.Roll == 0.0 ? "0.0," : (FString::SanitizeFloat(R.Roll) + ",")) +
        (R.Yaw == 0.0 ? "0.0," : (FString::SanitizeFloat(-R.Yaw) + ",")) +
        (R.Pitch == 0.0 ? "0.0;" : (FString::SanitizeFloat(R.Pitch) + ";")) +
        (S.X == 1.0 ? "1.0," : (FString::SanitizeFloat(S.X) + ",")) +
        (S.Z == 1.0 ? "1.0," : (FString::SanitizeFloat(S.Z) + ",")) +
        (S.Y == 1.0 ? "1.0" : FString::SanitizeFloat(S.Y)) +
        FString::Printf(TEXT(";%f,%f,%f"),
            Pivot.X * POSITION_SCALE_TO_HOUDINI, Pivot.Z * POSITION_SCALE_TO_HOUDINI, Pivot.Y * POSITION_SCALE_TO_HOUDINI);
}

FString FHoudiniEngineUtils::GetValidatedString(FString String)
{
    for (TCHAR& Char : String)
    {
        if (!IS_VALID_CHAR_IN_HOUDINI(Char))
            Char = TCHAR('_');
    }

    return String;
}

void FHoudiniEngineUtils::ParseFilterPattern(const FString& FilterPattern, TArray<FString>& OutFilters, TArray<FString>& OutInvertedFilters)
{
    OutFilters.Empty();
    OutInvertedFilters.Empty();

    bool bExpect = false;
    FString Filter;
    for (const TCHAR& Char : FilterPattern)
    {
        if (IS_VALID_CHAR_IN_HOUDINI(Char) || (Char == TCHAR('/')) || (Char == TCHAR('.')))
            Filter.AppendChar(Char);
        else
        {
            if (!Filter.IsEmpty())
            {
                if (bExpect) OutInvertedFilters.Add(Filter);
                else OutFilters.Add(Filter);
                Filter.Empty();
                bExpect = false;
            }
            bExpect = (Char == '^');
        }
    }

    if (!Filter.IsEmpty())
    {
        if (bExpect) OutInvertedFilters.Add(Filter);
        else OutFilters.Add(Filter);
        Filter.Empty();
    }
}

FString FHoudiniEngineUtils::GetAssetReference(const UObject* Asset)
{
    return Asset ? (Asset->GetClass()->GetPathName() + TEXT("'") + Asset->GetPathName() + TEXT("'")) : FString();
}

FString FHoudiniEngineUtils::GetAssetReference(const FSoftObjectPath& AssetPath)
{
    FAssetData AssetData = IAssetRegistry::GetChecked().GetAssetByObjectPath(AssetPath);
    return AssetData.IsValid() ? (AssetData.AssetClassPath.ToString() + TEXT("'") + AssetPath.ToString() + TEXT("'")) : FString();  // See FAssetData::GetExportTextName()
}


// -------- Attribute And Group --------
EHoudiniStorageType FHoudiniEngineUtils::ConvertStorageType(HAPI_StorageType Storage)
{
    if (Storage >= HAPI_STORAGETYPE_INT_ARRAY)
        Storage = HAPI_StorageType(Storage - HAPI_STORAGETYPE_INT_ARRAY);

    switch (Storage)
    {
    case HAPI_STORAGETYPE_INT:
    case HAPI_STORAGETYPE_INT64: return EHoudiniStorageType::Int;
    case HAPI_STORAGETYPE_FLOAT:
    case HAPI_STORAGETYPE_FLOAT64: return EHoudiniStorageType::Float;
    case HAPI_STORAGETYPE_STRING: return EHoudiniStorageType::String;
    case HAPI_STORAGETYPE_UINT8:
    case HAPI_STORAGETYPE_INT8:
    case HAPI_STORAGETYPE_INT16: return EHoudiniStorageType::Int;
    case HAPI_STORAGETYPE_DICTIONARY: return EHoudiniStorageType::String;
    }

    return EHoudiniStorageType::Invalid;
}

bool FHoudiniEngineUtils::HapiGetAttributeNames(const int32& NodeId, const int32& PartId, const int AttribCounts[HAPI_ATTROWNER_MAX],
    TArray<std::string>& OutAttribNames)
{
    for (HAPI_AttributeOwner Owner = HAPI_ATTROWNER_VERTEX; Owner < HAPI_ATTROWNER_MAX; Owner = HAPI_AttributeOwner(int(Owner) + 1))
    {
        const int32& NumAttribs = AttribCounts[Owner];
        if (NumAttribs <= 0)
            continue;

        TArray<HAPI_StringHandle> AttribNameSHs;
        AttribNameSHs.SetNumUninitialized(NumAttribs);
        HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeNames(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
            Owner, AttribNameSHs.GetData(), NumAttribs));

        TArray<std::string> AttribNames;
        HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertStringHandles(AttribNameSHs, AttribNames));
        OutAttribNames.Append(AttribNames);
    }

    return true;
}

HAPI_AttributeOwner FHoudiniEngineUtils::QueryAttributeOwner(
    const TArray<std::string>& AttribNames, const int AttribCounts[HAPI_ATTROWNER_MAX], const std::string& AttribName)
{
    const int32 FoundIdx = AttribNames.IndexOfByKey(AttribName);
    if (AttribNames.IsValidIndex(FoundIdx))
    {
        int32 NumAttribs = 0;
        for (HAPI_AttributeOwner Owner = HAPI_ATTROWNER_VERTEX; Owner < HAPI_ATTROWNER_MAX; Owner = HAPI_AttributeOwner(int(Owner) + 1))
        {
            NumAttribs += AttribCounts[Owner];
            if (FoundIdx < NumAttribs)
                return Owner;
        }
    }

    return HAPI_ATTROWNER_INVALID;
}

bool FHoudiniEngineUtils::IsAttributeExists(
    const TArray<std::string>& AttribNames, const int AttribCounts[HAPI_ATTROWNER_MAX], const std::string& AttribName, const HAPI_AttributeOwner& Owner)
{
    if (Owner == HAPI_ATTROWNER_INVALID)
        return AttribNames.Contains(AttribName);

    int32 StartIdx = 0;
    int32 EndIdx = AttribCounts[0];
    for (int32 OwnerIdx = 1; OwnerIdx <= Owner; ++OwnerIdx)
    {
        StartIdx = EndIdx;
        EndIdx += AttribCounts[OwnerIdx];
    }

    for (int32 AttribIdx = StartIdx; AttribIdx < EndIdx; ++AttribIdx)
    {
        if (AttribNames[AttribIdx] == AttribName)
            return true;
    }

    return false;
}

bool FHoudiniEngineUtils::HapiGetEnumAttributeData(const int32& NodeId, const int32& PartId, const char* AttribName,
    TArray<int8>& OutData, HAPI_AttributeOwner& InOutOwner)
{
    if (InOutOwner == HAPI_ATTROWNER_INVALID)
        return true;

    HAPI_AttributeInfo AttribInfo;
    FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
        AttribName, InOutOwner, &AttribInfo);

    if (AttribInfo.exists && !IsArray(AttribInfo.storage) && ((ConvertStorageType(AttribInfo.storage) == EHoudiniStorageType::Int) ||
        (ConvertStorageType(AttribInfo.storage) == EHoudiniStorageType::Float)))
    {
        AttribInfo.tupleSize = 1;  // The tupleSize should be 1 when used for enum
        OutData.SetNumUninitialized(AttribInfo.count);

        HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInt8Data(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
            AttribName, &AttribInfo, -1, OutData.GetData(), 0, AttribInfo.count));
    }
    else
        InOutOwner = HAPI_ATTROWNER_INVALID;

    return true;
}

bool FHoudiniEngineUtils::HapiGetEnumAttributeData(const int32& NodeId, const int32& PartId, const char* AttribName, TFunctionRef<int8(FUtf8StringView)> EnumGetter,
    TArray<int8>& OutData, HAPI_AttributeOwner& InOutOwner)
{
    if (InOutOwner == HAPI_ATTROWNER_INVALID)
        return true;

    HAPI_AttributeInfo AttribInfo;
    FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
        AttribName, InOutOwner, &AttribInfo);

    if (AttribInfo.exists && !IsArray(AttribInfo.storage))
    {
        if ((ConvertStorageType(AttribInfo.storage) == EHoudiniStorageType::Int) ||
            (ConvertStorageType(AttribInfo.storage) == EHoudiniStorageType::Float))
        {
            AttribInfo.tupleSize = 1;  // The tupleSize should be 1 when used for enum
            OutData.SetNumUninitialized(AttribInfo.count);

            HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInt8Data(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
                AttribName, &AttribInfo, -1, OutData.GetData(), 0, AttribInfo.count));
        }
        else if (AttribInfo.storage == HAPI_STORAGETYPE_STRING)
        {
            TArray<HAPI_StringHandle> SHs;
            SHs.SetNumUninitialized(AttribInfo.count);
            HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeStringData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
                AttribName, &AttribInfo, SHs.GetData(), 0, AttribInfo.count));

            TMap<HAPI_StringHandle, int8> SHEnumMap;
            {
                TArray<HAPI_StringHandle> UniqueSHs = TSet<HAPI_StringHandle>(SHs).Array();
                TArray<std::string> UniqueStrs;
                HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertStringHandles(UniqueSHs, UniqueStrs));
                for (int32 UniqueIdx = 0; UniqueIdx < UniqueSHs.Num(); ++UniqueIdx)
                    SHEnumMap.Add(UniqueSHs[UniqueIdx], EnumGetter(UniqueStrs[UniqueIdx].c_str()));
            }
            OutData.SetNumUninitialized(AttribInfo.count);
            for (int32 ElemIdx = 0; ElemIdx < AttribInfo.count; ++ElemIdx)
                OutData[ElemIdx] = SHEnumMap[SHs[ElemIdx]];
        }
        else
            InOutOwner = HAPI_ATTROWNER_INVALID;
    }
    else
        InOutOwner = HAPI_ATTROWNER_INVALID;

    return true;
}

bool FHoudiniEngineUtils::HapiGetFloatAttributeData(const int32& NodeId, const int32& PartId,
    const char* AttribName, const int32& DesiredTupleSize, TArray<float>& OutData, HAPI_AttributeOwner& InOutOwner)
{
    if (InOutOwner == HAPI_ATTROWNER_INVALID)
        return true;

    HAPI_AttributeInfo AttribInfo;
    FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
        AttribName, InOutOwner, &AttribInfo);

    if (AttribInfo.exists && ((AttribInfo.storage == HAPI_STORAGETYPE_FLOAT) || (AttribInfo.storage == HAPI_STORAGETYPE_FLOAT64)) &&
        ((DesiredTupleSize == -1) || (AttribInfo.tupleSize >= DesiredTupleSize)))
    {
        AttribInfo.tupleSize = (DesiredTupleSize >= 1) ? DesiredTupleSize : AttribInfo.tupleSize;
        OutData.SetNumUninitialized(AttribInfo.count * AttribInfo.tupleSize);

        HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeFloatData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
            AttribName, &AttribInfo, -1, OutData.GetData(), 0, AttribInfo.count));
    }
    else
        InOutOwner = HAPI_ATTROWNER_INVALID;

    return true;
}

bool FHoudiniEngineUtils::HapiGetStringAttributeValue(const int32& NodeId, const int32& PartId, const TArray<std::string>& AttribNames, const int AttribCounts[HAPI_ATTROWNER_MAX],
    const char* AttribName, FString& OutValue, bool* bOutHasAttrib)
{
    const HAPI_AttributeOwner AttribOwner = FHoudiniEngineUtils::QueryAttributeOwner(
        AttribNames, AttribCounts, AttribName);

    if (AttribOwner == HAPI_ATTROWNER_INVALID)
    {
        if (bOutHasAttrib)
            *bOutHasAttrib = false;
        return true;
    }
    
    if (bOutHasAttrib)
        *bOutHasAttrib = true;

    HAPI_AttributeInfo AttribInfo;
    HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeInfo(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
        AttribName, AttribOwner, &AttribInfo));
    if (AttribInfo.storage != HAPI_STORAGETYPE_STRING)
        return true;

    HAPI_StringHandle ValueSH;
    HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetAttributeStringData(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
        AttribName, &AttribInfo, &ValueSH, 0, 1));

    return FHoudiniEngineUtils::HapiConvertStringHandle(ValueSH, OutValue);
}


bool FHoudiniEngineUtils::HapiGetPrimitiveGroupNames(const HAPI_GeoInfo& GeoInfo, const HAPI_PartInfo& PartInfo, TArray<std::string>& OutPrimGroupNames)
{
    int32 NumPrimGroups = GeoInfo.primitiveGroupCount;
    if (PartInfo.isInstanced)  // if a packed mesh, we should query group count on part
    {
        int32 NumPointGroups;
        HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetGroupCountOnPackedInstancePart(FHoudiniEngine::Get().GetSession(), GeoInfo.nodeId, PartInfo.id,
            &NumPointGroups, &NumPrimGroups));
    }

    if (NumPrimGroups)
    {
        TArray<HAPI_StringHandle> PrimGroupNameSHs;
        PrimGroupNameSHs.SetNumUninitialized(NumPrimGroups);
        if (PartInfo.isInstanced)
            HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetGroupNamesOnPackedInstancePart(FHoudiniEngine::Get().GetSession(),
                GeoInfo.nodeId, PartInfo.id, HAPI_GROUPTYPE_PRIM, PrimGroupNameSHs.GetData(), NumPrimGroups))
        else
            HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetGroupNames(FHoudiniEngine::Get().GetSession(),
                GeoInfo.nodeId, HAPI_GROUPTYPE_PRIM, PrimGroupNameSHs.GetData(), NumPrimGroups));

        HOUDINI_FAIL_RETURN(FHoudiniEngineUtils::HapiConvertStringHandles(PrimGroupNameSHs, OutPrimGroupNames));
    }

    return true;
}

bool FHoudiniEngineUtils::HapiGetGroupMembership(const int32& NodeId, const int32& PartId,
    const int32 NumElems, const bool bOnPackedPart, const HAPI_GroupType& GroupType, const char* GroupName,
    TArray<int32>& GroupMembership, bool& bMembershipConst)
{
    GroupMembership.SetNumUninitialized(NumElems);
    if (bOnPackedPart)
        HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetGroupMembershipOnPackedInstancePart(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
            GroupType, GroupName, &bMembershipConst, GroupMembership.GetData(), 0, NumElems))
    else
        HAPI_SESSION_FAIL_RETURN(FHoudiniApi::GetGroupMembership(FHoudiniEngine::Get().GetSession(), NodeId, PartId,
            GroupType, GroupName, &bMembershipConst, GroupMembership.GetData(), 0, NumElems))

    return true;
}


// -------- Asset ---------
UMaterial* FHoudiniEngineUtils::GetVertexColorMaterial(const bool& bGetTranslucent)
{
    static UMaterial* VertexColorMaterial = nullptr;
    static UMaterial* TranslucentVertexColorMaterial = nullptr;
    if (!bGetTranslucent)
    {
        if (!VertexColorMaterial)
            VertexColorMaterial = LoadObject<UMaterial>(nullptr, TEXT("/" UE_PLUGIN_NAME "/Materials/M_VertexColor.M_VertexColor"));
    }
    else if (!TranslucentVertexColorMaterial)
        TranslucentVertexColorMaterial = LoadObject<UMaterial>(nullptr, TEXT("/" UE_PLUGIN_NAME "/Materials/M_TranslucentVertexColor.M_TranslucentVertexColor"));

    return bGetTranslucent ? TranslucentVertexColorMaterial : VertexColorMaterial;
}

FString FHoudiniEngineUtils::GetPackagePath(const FString& AssetPath)
{
    int32 SplitIdx;
    const FString ObjectPath = FPackageName::ExportTextPathToObjectPath(
        AssetPath.FindChar(TCHAR(';'), SplitIdx) ? AssetPath.Left(SplitIdx) : AssetPath);  // See UHoudiniParameterAsset

    return ObjectPath.FindLastChar(TCHAR('.'), SplitIdx) ? ObjectPath.Left(SplitIdx) : ObjectPath;
}

UObject* FHoudiniEngineUtils::FindOrCreateAsset(const UClass* AssetClass, const FString& AssetPath, bool* bOutFound)
{
    if (bOutFound)
        *bOutFound = true;

    int32 SplitIdx;
    const FString ObjectPath = FPackageName::ExportTextPathToObjectPath(
        AssetPath.FindChar(TCHAR(';'), SplitIdx) ? AssetPath.Left(SplitIdx) : AssetPath);  // See UHoudiniParameterAsset

    UObject* TargetAsset = LoadObject<UObject>(nullptr, *ObjectPath, nullptr, LOAD_Quiet | LOAD_NoWarn);
    if (IsValid(TargetAsset))
    {
        if (TargetAsset->GetClass() == AssetClass)
            return TargetAsset;
        else
        {
            UE_LOG(LogHoudiniEngine, Warning, TEXT("%s: Replace %s to a new %s"), *ObjectPath, *TargetAsset->GetClass()->GetName(), *AssetClass->GetName());
            TargetAsset->ClearFlags(RF_Standalone);
            TargetAsset->MarkAsGarbage();
            CollectGarbage(RF_NoFlags, true);
        }
    }

    if (bOutFound)
        *bOutFound = false;

    FString PackagePath;
    FString AssetName;
    if (!ObjectPath.Split(TEXT("."), &PackagePath, &AssetName, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
    {
        PackagePath = ObjectPath;
        AssetName = FPaths::GetBaseFilename(ObjectPath);
    }

    UPackage* Package = CreatePackage(*PackagePath);
    
    Package->Modify();
    UObject* NewAsset = NewObject<UObject>(Package, AssetClass, *AssetName, RF_Public | RF_Standalone);
    FAssetRegistryModule::AssetCreated(NewAsset);  // Let Content Browser to show the NewAsset
    return NewAsset;
}

UObject* FHoudiniEngineUtils::CreateAsset(const UClass* AssetClass, const FString& AssetPath)
{
    int32 SplitIdx;
    const FString ObjectPath = FPackageName::ExportTextPathToObjectPath(
        AssetPath.FindChar(TCHAR(';'), SplitIdx) ? AssetPath.Left(SplitIdx) : AssetPath);  // See UHoudiniParameterAsset

    UObject* TargetAsset = LoadObject<UObject>(nullptr, *ObjectPath, nullptr, LOAD_Quiet | LOAD_NoWarn);
    if (IsValid(TargetAsset))
    {
        if (TargetAsset->GetClass() != AssetClass)
        {
            UE_LOG(LogHoudiniEngine, Warning, TEXT("%s: Replace %s to a new %s"), *ObjectPath, *TargetAsset->GetClass()->GetName(), *AssetClass->GetName());
            TargetAsset->ClearFlags(RF_Standalone);
            TargetAsset->MarkAsGarbage();
            CollectGarbage(RF_NoFlags, true);
        }
    }

    FString PackagePath;
    FString AssetName;
    if (!ObjectPath.Split(TEXT("."), &PackagePath, &AssetName, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
    {
        PackagePath = ObjectPath;
        AssetName = FPaths::GetBaseFilename(ObjectPath);
    }

    UPackage* Package = CreatePackage(*PackagePath);

    Package->Modify();
    UObject* NewAsset = NewObject<UObject>(Package, AssetClass, *AssetName, RF_Public | RF_Standalone);
    FAssetRegistryModule::AssetCreated(NewAsset);  // Let Content Browser to show the NewAsset
    return NewAsset;
}

USceneComponent* FHoudiniEngineUtils::CreateComponent(AActor* Owner, const TSubclassOf<USceneComponent>& ComponentClass)
{
    USceneComponent* Component = NewObject<USceneComponent>(Owner, ComponentClass, NAME_None, RF_Public | RF_Transactional);

    Component->CreationMethod = EComponentCreationMethod::Instance;
    Component->SetMobility(Owner->GetRootComponent()->Mobility);
    Component->AttachToComponent(Owner->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
    Component->OnComponentCreated();
    Component->RegisterComponent();
    Owner->AddInstanceComponent(Component);

    return Component;
}

void FHoudiniEngineUtils::DestroyComponent(USceneComponent* Component)
{
    Component->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
    Component->UnregisterComponent();
    Component->DestroyComponent();
}

bool FHoudiniEngineUtils::FilterClass(const TArray<const UClass*>& AllowClasses, const TArray<const UClass*>& DisallowClasses, const UClass* ObjectClass)
{
    if (!IsValid(ObjectClass))
        return false;

    if (DisallowClasses.Contains(ObjectClass))
        return false;

    if (AllowClasses.IsEmpty())
        return true;

    for (const UClass* AllowClass : AllowClasses)
    {
        if (ObjectClass->IsChildOf(AllowClass))
            return true;
    }

    return false;
}


// -------- Misc ---------
void FHoudiniEngineUtils::ConvertLandscapeTransform(float OutHapiTransform[9],
    const FTransform& LandscapeTransform, const FIntRect& LandscapeExtent)
{
    const FVector Scale = LandscapeTransform.GetScale3D() * POSITION_SCALE_TO_HOUDINI;
    const FVector Translation = LandscapeTransform.TransformPosition(
        FVector((LandscapeExtent.Max.X + LandscapeExtent.Min.X) * 0.5, (LandscapeExtent.Max.Y + LandscapeExtent.Min.Y) * 0.5, 0.0)) * POSITION_SCALE_TO_HOUDINI;
    const FRotator Rotator = LandscapeTransform.GetRotation().Rotator();

    OutHapiTransform[0] = float(Translation.X);
    OutHapiTransform[1] = float(Translation.Z);
    OutHapiTransform[2] = float(Translation.Y);
    OutHapiTransform[3] = float(Rotator.Pitch - 90.0);
    OutHapiTransform[4] = float(-Rotator.Yaw - 90.0);
    OutHapiTransform[5] = float(Rotator.Roll);
    OutHapiTransform[6] = float(Scale.X) * 0.5f * (LandscapeExtent.Height() + 1);
    OutHapiTransform[7] = float(Scale.Y) * 0.5f * (LandscapeExtent.Width() + 1);
    OutHapiTransform[8] = 0.5f;
}

HAPI_TransformEuler FHoudiniEngineUtils::ConvertTransform(const FTransform& Transform)
{
    HAPI_TransformEuler HapiTransformEuler;
    FHoudiniApi::TransformEuler_Init(&HapiTransformEuler);

    HapiTransformEuler.rstOrder = HAPI_SRT;
    HapiTransformEuler.rotationOrder = HAPI_XYZ;

    FQuat UnrealRotation = Transform.GetRotation();
    const FVector UnrealTranslation = Transform.GetTranslation() * POSITION_SCALE_TO_HOUDINI;
    const FVector UnrealScale = Transform.GetScale3D();

    // switch the quaternion to Y-up, LHR by Swapping Y/Z and negating W
    Swap(UnrealRotation.Y, UnrealRotation.Z);
    UnrealRotation.W = -UnrealRotation.W;
    const FRotator Rotator = UnrealRotation.Rotator();

    // Negate roll and pitch since they are actually RHR
    HapiTransformEuler.rotationEuler[0] = -(float)Rotator.Roll;
    HapiTransformEuler.rotationEuler[1] = -(float)Rotator.Pitch;
    HapiTransformEuler.rotationEuler[2] = (float)Rotator.Yaw;

    // Swap Y/Z, scale
    HapiTransformEuler.position[0] = (float)(UnrealTranslation.X);
    HapiTransformEuler.position[1] = (float)(UnrealTranslation.Z);
    HapiTransformEuler.position[2] = (float)(UnrealTranslation.Y);

    // Swap Y/Z
    HapiTransformEuler.scale[0] = (float)UnrealScale.X;
    HapiTransformEuler.scale[1] = (float)UnrealScale.Z;
    HapiTransformEuler.scale[2] = (float)UnrealScale.Y;

    return HapiTransformEuler;
}

int32 FHoudiniEngineUtils::BinarySearch(const TArray<int32>& SortedArray, const int32& Target)
{
    int32 T0 = -1, T1 = SortedArray.Num();
    for (int32 SearchIter = SortedArray.Num() - 1; SearchIter >= 0; --SearchIter)
    {
        const int32 Idx = (T0 + T1) / 2;
        const int32& Elem = SortedArray[Idx];
        if (Elem == Target) return Idx;
        else if (Elem > Target) T1 = Idx;
        else T0 = Idx;

        if (T1 - T0 <= 1)
            return -T1 - 1;
    }
    return -T1 - 1;
}

EHoudiniVolumeConvertDataType FHoudiniEngineUtils::ConvertTextureSourceFormat(const ETextureSourceFormat& TextureFormat)
{
    switch (TextureFormat)
    {
    case TSF_G8:
    case TSF_BGRA8:
    case TSF_BGRE8: return EHoudiniVolumeConvertDataType::Uint8;
    case TSF_RGBA16: return EHoudiniVolumeConvertDataType::Uint16;
    case TSF_RGBA16F: return EHoudiniVolumeConvertDataType::Float16;
    case TSF_G16: return EHoudiniVolumeConvertDataType::Uint16;
    case TSF_RGBA32F: return EHoudiniVolumeConvertDataType::Float;
    case TSF_R16F: return EHoudiniVolumeConvertDataType::Float16;
    case TSF_R32F: return EHoudiniVolumeConvertDataType::Float;
    }

    return EHoudiniVolumeConvertDataType::Uint8;
}

size_t FHoudiniEngineUtils::GetElemSize(const EHoudiniVolumeConvertDataType& DataType)
{
    switch (DataType)
    {
    case EHoudiniVolumeConvertDataType::Uint8: return 1;
    case EHoudiniVolumeConvertDataType::Uint16: return 2;
    case EHoudiniVolumeConvertDataType::Uint:
    case EHoudiniVolumeConvertDataType::Int: return 4;
    case EHoudiniVolumeConvertDataType::Int64: return 8;
    case EHoudiniVolumeConvertDataType::Float16: return 2;
    case EHoudiniVolumeConvertDataType::Float: return 4;
    }

    return 0;
}

ULevel* FHoudiniEngineUtils::GetCurrentLevel()
{
    return GEngine->GetWorldContexts()[0].World()->GetCurrentLevel();
}
