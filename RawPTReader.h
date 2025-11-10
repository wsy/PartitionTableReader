#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "CRC32.h"

typedef unsigned long long UInt64;
typedef unsigned int UInt32;
typedef unsigned short UInt16;
typedef unsigned char UInt8;
typedef long long Int64;
typedef int Int32;
typedef short Int16;
typedef char Int8;

typedef struct
{
	UInt8 BootIndicator;
	UInt8 StartingCHS[3];
	UInt8 PartitionType;
	UInt8 EndingCHS[3];
	UInt32 StartingLBA;
	UInt32 SizeInLBA;
} MbrPartitionEntry;
typedef struct
{
	UInt8 BootstrapCode[446];
	MbrPartitionEntry PartitionEntries[4];
	UInt16 Signature;
} MbrHeader;
typedef struct
{
	char Signature[8];
	UInt16 MinorRevision;
	UInt16 MajorRevision;
	UInt32 HeaderSize;
	UInt32 HeaderCRC32;
	UInt8 Reserved[4];
	UInt64 CurrentLBA;
	UInt64 AlternateLBA;
	UInt64 FirstUsableLBA;
	UInt64 LastUsableLBA;
	UInt8 DiskGUID[16];
	UInt64 PartitionEntryOffset;
	UInt32 PartitionEntryCount;
	UInt32 PartitionEntrySize;
	UInt32 PartitionEntryCRC32;
} GptHeader;

void printUsage(const char* arg0);
int processingArgs(int argc, const char* argv[]);
void handleMBR(void);
void handleEBR(UInt64 extendedPartitionStartOffset, UInt64 offset);
void mbrEntry(MbrPartitionEntry* entry);
void handleGPT(void);
void printGptInfo(GptHeader* gptHeader);
void gptEntry(int entryNumber, int offset, int partitionEntrySize);
bool isEmptyGPTSlot(int offset);
int readSector(Int64 offset);
void printHumanReadableSize(UInt64 number);
void printGUID(int offset);
