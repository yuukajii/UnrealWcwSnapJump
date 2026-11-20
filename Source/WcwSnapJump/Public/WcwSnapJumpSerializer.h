// Copyright (c) 2026 WinterCrownWORKS. All Rights Reserved.
// Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "WcwSnapJumpTypes.h"

/**
 * Utility class for serializing FWcwSnapJumpData to/from JSON and embedding/extracting
 * custom PNG tEXt chunks ("WcwSnapJump") without external dependencies.
 */
class WCWSNAPJUMP_API FWcwSnapJumpSerializer
{
public:
	/** PNG chunk keyword used to identify embedded Wcw snapshot metadata */
	static const ANSICHAR* PngKeyword;
	/** Legacy keyword accepted on read for backward compatibility */
	static const ANSICHAR* LegacyPngKeyword;

	/** Convert FWcwSnapJumpData struct to JSON string */
	static bool StructToJsonString(const FWcwSnapJumpData& InData, FString& OutJsonString, bool bPrettyPrint = false);

	/** Parse FWcwSnapJumpData struct from JSON string */
	static bool JsonStringToStruct(const FString& InJsonString, FWcwSnapJumpData& OutData);

	/**
	 * Inject a PNG "tEXt" chunk containing the JSON string right after the "IHDR" chunk.
	 */
	static bool InjectMetaIntoPngBytes(const TArray<uint8>& InPngBytes, const FWcwSnapJumpData& InMeta, TArray<uint8>& OutPngBytes);

	/**
	 * Extract the embedded "tEXt" chunk with keyword "WcwSnapJump" (or legacy "WcwScreenMeta") directly from PNG bytes.
	 */
	static bool ExtractMetaFromPngBytes(const TArray<uint8>& InPngBytes, FWcwSnapJumpData& OutMeta);

	/**
	 * Fast file-level metadata reader: reads PNG chunks (or fallback .json sidecar) from disk.
	 */
	static bool ReadMetaFromFile(const FString& InPngFilePath, FWcwSnapJumpData& OutMeta);

private:
	static uint32 ComputePngCrc32(const uint8* Data, int32 Length);
	static void WriteUint32BE(uint8* Dest, uint32 Value);
	static uint32 ReadUint32BE(const uint8* Src);
};
