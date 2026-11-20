// Copyright (c) 2026 WinterCrownWORKS. All Rights Reserved.
// Licensed under the MIT License.

#include "WcwSnapJumpSerializer.h"
#include "WcwSnapJumpModule.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

const ANSICHAR* FWcwSnapJumpSerializer::PngKeyword = "WcwSnapJump";
const ANSICHAR* FWcwSnapJumpSerializer::LegacyPngKeyword = "WcwScreenMeta";

bool FWcwSnapJumpSerializer::StructToJsonString(const FWcwSnapJumpData& InData, FString& OutJsonString, bool bPrettyPrint)
{
	return FJsonObjectConverter::UStructToJsonObjectString(InData, OutJsonString, 0, 0, 0, nullptr, bPrettyPrint);
}

bool FWcwSnapJumpSerializer::JsonStringToStruct(const FString& InJsonString, FWcwSnapJumpData& OutData)
{
	return FJsonObjectConverter::JsonObjectStringToUStruct(InJsonString, &OutData, 0, 0);
}

uint32 FWcwSnapJumpSerializer::ComputePngCrc32(const uint8* Data, int32 Length)
{
	static uint32 CrcTable[256];
	static bool bTableInitialized = false;
	if (!bTableInitialized)
	{
		for (uint32 n = 0; n < 256; ++n)
		{
			uint32 c = n;
			for (int32 k = 0; k < 8; ++k)
			{
				if (c & 1)
				{
					c = 0xEDB88320U ^ (c >> 1);
				}
				else
				{
					c = c >> 1;
				}
			}
			CrcTable[n] = c;
		}
		bTableInitialized = true;
	}

	uint32 Crc = 0xFFFFFFFFU;
	for (int32 i = 0; i < Length; ++i)
	{
		Crc = CrcTable[(Crc ^ Data[i]) & 0xFF] ^ (Crc >> 8);
	}
	return Crc ^ 0xFFFFFFFFU;
}

void FWcwSnapJumpSerializer::WriteUint32BE(uint8* Dest, uint32 Value)
{
	Dest[0] = static_cast<uint8>((Value >> 24) & 0xFF);
	Dest[1] = static_cast<uint8>((Value >> 16) & 0xFF);
	Dest[2] = static_cast<uint8>((Value >> 8) & 0xFF);
	Dest[3] = static_cast<uint8>(Value & 0xFF);
}

uint32 FWcwSnapJumpSerializer::ReadUint32BE(const uint8* Src)
{
	return (static_cast<uint32>(Src[0]) << 24) |
	       (static_cast<uint32>(Src[1]) << 16) |
	       (static_cast<uint32>(Src[2]) << 8)  |
	       (static_cast<uint32>(Src[3]));
}

bool FWcwSnapJumpSerializer::InjectMetaIntoPngBytes(const TArray<uint8>& InPngBytes, const FWcwSnapJumpData& InMeta, TArray<uint8>& OutPngBytes)
{
	static const uint8 PngSignature[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
	if (InPngBytes.Num() < 33 || FMemory::Memcmp(InPngBytes.GetData(), PngSignature, 8) != 0)
	{
		UE_LOG(LogWcwSnapJump, Warning, TEXT("InjectMetaIntoPngBytes: Invalid PNG signature."));
		return false;
	}

	FString JsonStr;
	if (!StructToJsonString(InMeta, JsonStr, false))
	{
		return false;
	}

	FTCHARToUTF8 Utf8Json(*JsonStr);
	const int32 KeywordLen = FCStringAnsi::Strlen(PngKeyword);
	const int32 PayloadLen = KeywordLen + 1 + Utf8Json.Length();

	TArray<uint8> TextChunk;
	TextChunk.SetNumUninitialized(4 + 4 + PayloadLen + 4);

	WriteUint32BE(TextChunk.GetData(), static_cast<uint32>(PayloadLen));
	TextChunk[4] = 't';
	TextChunk[5] = 'E';
	TextChunk[6] = 'X';
	TextChunk[7] = 't';

	uint8* DataPtr = TextChunk.GetData() + 8;
	FMemory::Memcpy(DataPtr, PngKeyword, KeywordLen);
	DataPtr[KeywordLen] = 0;
	FMemory::Memcpy(DataPtr + KeywordLen + 1, Utf8Json.Get(), Utf8Json.Length());

	const uint32 Crc = ComputePngCrc32(TextChunk.GetData() + 4, 4 + PayloadLen);
	WriteUint32BE(TextChunk.GetData() + 8 + PayloadLen, Crc);

	const uint32 FirstChunkLen = ReadUint32BE(InPngBytes.GetData() + 8);
	const int32 InsertOffset = 8 + 12 + static_cast<int32>(FirstChunkLen);
	if (InsertOffset > InPngBytes.Num())
	{
		return false;
	}

	OutPngBytes.Reset();
	OutPngBytes.Reserve(InPngBytes.Num() + TextChunk.Num());
	OutPngBytes.Append(InPngBytes.GetData(), InsertOffset);
	OutPngBytes.Append(TextChunk);
	OutPngBytes.Append(InPngBytes.GetData() + InsertOffset, InPngBytes.Num() - InsertOffset);

	return true;
}

bool FWcwSnapJumpSerializer::ExtractMetaFromPngBytes(const TArray<uint8>& InPngBytes, FWcwSnapJumpData& OutMeta)
{
	static const uint8 PngSignature[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
	if (InPngBytes.Num() < 20 || FMemory::Memcmp(InPngBytes.GetData(), PngSignature, 8) != 0)
	{
		return false;
	}

	const int32 KeywordLen = FCStringAnsi::Strlen(PngKeyword);
	const int32 LegacyLen = FCStringAnsi::Strlen(LegacyPngKeyword);
	int32 Offset = 8;

	while (Offset + 12 <= InPngBytes.Num())
	{
		const uint32 ChunkLen = ReadUint32BE(InPngBytes.GetData() + Offset);
		const uint8* ChunkType = InPngBytes.GetData() + Offset + 4;
		const uint8* ChunkData = InPngBytes.GetData() + Offset + 8;

		if (Offset + 12 + static_cast<int64>(ChunkLen) > InPngBytes.Num())
		{
			break;
		}

		if (ChunkType[0] == 't' && ChunkType[1] == 'E' && ChunkType[2] == 'X' && ChunkType[3] == 't')
		{
			int32 MatchedKeyLen = -1;
			if (static_cast<int32>(ChunkLen) > KeywordLen + 1 &&
			    FMemory::Memcmp(ChunkData, PngKeyword, KeywordLen) == 0 &&
			    ChunkData[KeywordLen] == 0)
			{
				MatchedKeyLen = KeywordLen;
			}
			else if (static_cast<int32>(ChunkLen) > LegacyLen + 1 &&
			         FMemory::Memcmp(ChunkData, LegacyPngKeyword, LegacyLen) == 0 &&
			         ChunkData[LegacyLen] == 0)
			{
				MatchedKeyLen = LegacyLen;
			}

			if (MatchedKeyLen > 0)
			{
				const ANSICHAR* JsonStart = reinterpret_cast<const ANSICHAR*>(ChunkData + MatchedKeyLen + 1);
				const int32 JsonByteLen = static_cast<int32>(ChunkLen) - (MatchedKeyLen + 1);

				FUTF8ToTCHAR WideConv(JsonStart, JsonByteLen);
				FString JsonString(WideConv.Length(), WideConv.Get());

				return JsonStringToStruct(JsonString, OutMeta);
			}
		}
		else if (ChunkType[0] == 'I' && ChunkType[1] == 'E' && ChunkType[2] == 'N' && ChunkType[3] == 'D')
		{
			break;
		}

		Offset += 12 + static_cast<int32>(ChunkLen);
	}

	return false;
}

bool FWcwSnapJumpSerializer::ReadMetaFromFile(const FString& InPngFilePath, FWcwSnapJumpData& OutMeta)
{
	FString ResolvedPath = InPngFilePath;
#if PLATFORM_DESKTOP || WITH_EDITOR
	if (FPaths::IsRelative(ResolvedPath))
	{
		ResolvedPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), ResolvedPath);
	}
#endif

	TArray<uint8> FileBytes;
	if (FFileHelper::LoadFileToArray(FileBytes, *ResolvedPath))
	{
		if (ExtractMetaFromPngBytes(FileBytes, OutMeta))
		{
			OutMeta.SourceFilePath = ResolvedPath;
			return true;
		}
	}

	const FString SidecarPath = FPaths::ChangeExtension(ResolvedPath, TEXT("json"));
	FString JsonStr;
	if (FPaths::FileExists(SidecarPath) && FFileHelper::LoadFileToString(JsonStr, *SidecarPath))
	{
		if (JsonStringToStruct(JsonStr, OutMeta))
		{
			OutMeta.SourceFilePath = ResolvedPath;
			return true;
		}
	}

	return false;
}
