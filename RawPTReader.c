//
//  main.c
//  RawPTReader
//
//  Created by WSY on 2017/10/31.
//  Copyright © 2017 WSY. All rights reserved.
//

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

int processingArgs(int argc, const char* argv[]);
void printUsage(const char *arg0);
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

int SectorSize = 512;
int verbose = 0;
FILE *file = NULL;
UInt8 buffer[4096] = { 0 };
char zeroGuid[16] = { '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0' };
UInt32 GptEntryCRC = 0xFFFFFFFF;
UInt32 GptHeaderCRC = 0xFFFFFFFF;

int main(int argc, const char * argv[])
{
	int result = processingArgs(argc, argv);
	if (result < 0)
	{
		printUsage(argv[0]);
		return result;
	}
	file = fopen(argv[1], "rb");
	if (!file)
	{
		printf("Open failed!\n");
		return 1;
	}
	fread(buffer, sizeof(char), 1, file);
	printf("Open succeed!\n");
	handleMBR();
	handleGPT();
	return 0;
}

int processingArgs(int argc, const char* argv[])
{
	int currentArgIndex = 3;
	while (currentArgIndex < argc)
	{
		const char* currentArg = argv[currentArgIndex];
		if (strcmp(currentArg, "-v") == 0 || strcmp(currentArg, "-V") == 0 || strcmp(currentArg, "--verbose") == 0 || strcmp(currentArg, "--Verbose") == 0 || strcmp(currentArg, "--VERBOSE") == 0)
		{
			verbose += 1;
		}
		else if (strcmp(currentArg, "-vv") == 0 || strcmp(currentArg, "-VV") == 0 || strcmp(currentArg, "--veryverbose") == 0 || strcmp(currentArg, "--VeryVerbose") == 0 || strcmp(currentArg, "--VERYVERBOSE") == 0)
		{
			verbose += 2;
		}
		else
		{
			return -3;
		}
		currentArgIndex++;
	}
	if (argc > 2)
	{
		if (strcmp(argv[2], "4k") == 0 || strcmp(argv[2], "4K") == 0 || strcmp(argv[2], "4096") == 0)
		{
			SectorSize = 4096;
		}
		else if (strcmp(argv[2], "512") == 0)
		{
			SectorSize = 512;
		}
		else
		{
			return -2;
		}
	}
	if (argc > 1)
	{
		return 0;
	}
	return -1;
}

void printUsage(const char *arg0)
{
	printf("Raw PartitionTable Reader v1.1\n");
	printf("Usage:\n");
	printf("\t%s fileName\n", arg0);
	printf("\t%s fileName [<SectorSize>]\n", arg0);
	printf("\tValid SectorSize is either \"512\"(default) or \"4k\" or \"4K\" or \"4096\"\n");
	exit(0);
}

void handleMBR()
{
	readSector(0L);
	MbrHeader* mbrHeader = (MbrHeader*)buffer;
	if (mbrHeader->Signature != 0xAA55) // 0xAA evaluates to integer -86
	{
		printf("Invalid MBR partition table!\n");
		return;
	}
	MbrPartitionEntry* partitionEntry = mbrHeader->PartitionEntries;
	printf("MBR Partitions:\n");
	printf("  Partition#1:    ");
	mbrEntry(&partitionEntry[0]);
	printf("  Partition#2:    ");
	mbrEntry(&partitionEntry[1]);
	printf("  Partition#3:    ");
	mbrEntry(&partitionEntry[2]);
	printf("  Partition#4:    ");
	mbrEntry(&partitionEntry[3]);
}

void mbrEntry(MbrPartitionEntry* entry)
{
	bool isActive = false;
	unsigned char partitionType = '\0';
	UInt64 startByte = 0;
	UInt64 sizeInByte = 0;
	if (entry->BootIndicator < 0)
	{
		isActive = true;
	}
	partitionType = entry->PartitionType;
	startByte = (entry->StartingLBA) * (UInt64)SectorSize;
	sizeInByte = (entry->SizeInLBA) * (UInt64)SectorSize;
	printf(" Offset: ");
	printHumanReadableSize(startByte);
	printf("\tSize: ");
	printHumanReadableSize(sizeInByte);
	printf("\tType: ");
	if (partitionType == '\0')
	{
		// Empty partition slot!
		printf("Empty Slot!\n");
		return;
	}
	else if (partitionType != 0x0f && partitionType != 0x05)
	{
		// Primary partition
		printf("Primary Partition %02hhX\n", partitionType);
		return;
	}
	// Extended partition
	printf("ExtendedPartition %02hhX\n", partitionType);
	handleEBR(startByte, startByte);
}

void handleEBR(UInt64 extendedPartitionStartOffset, UInt64 offset)
{
	unsigned char partitionType = '\0';
	UInt64 startByte = 0;
	UInt64 sizeInByte = 0;
	UInt64 nextEBR = 0;
	readSector(offset);
	MbrHeader* ebrHeader = (MbrHeader*)buffer;
	if (ebrHeader->Signature != 0xAA55) // 0xAA evaluates to integer -86
	{
		printf("    Invalid extended partition table!\n");
		return;
	}
	MbrPartitionEntry* partitionEntry = ebrHeader->PartitionEntries;
	partitionType = partitionEntry[0].PartitionType;
#ifdef DEBUG
	printf("DEBUG\tHandling EBR at offset %lld\n", offset);
	printf("DEBUG\tRawData\tStartSector: %02hhX %02hhX %02hhX %02hhX\n", buffer[446 + 8 + 0], buffer[446 + 8 + 1], buffer[446 + 8 + 2], buffer[446 + 8 + 3]);
	printf("DEBUG\tRawData\tSizeInSector: %02hhX %02hhX %02hhX %02hhX\n", buffer[446 + 12 + 0], buffer[446 + 12 + 1], buffer[446 + 12 + 2], buffer[446 + 12 + 3]);
	printf("DEBUG\tRawData\tNextEBRSector: %02hhX %02hhX %02hhX %02hhX\n", buffer[462 + 8 + 0], buffer[462 + 8 + 1], buffer[462 + 8 + 2], buffer[462 + 8 + 3]);
#endif
	startByte = (partitionEntry[0].StartingLBA) * (UInt64)SectorSize + offset;
	sizeInByte = (partitionEntry[0].SizeInLBA) * (UInt64)SectorSize;
	printf("    LogicalDrive: ");
	printf(" Offset: ");
	printHumanReadableSize(startByte);
	printf("\tSize: ");
	printHumanReadableSize(sizeInByte);
	printf("\tType: LogicalDrive ");
	printf("%02hhX\n", partitionType);
	nextEBR = (partitionEntry[1].StartingLBA) * (UInt64)SectorSize;
	if (nextEBR != 0)
	{
#ifdef DEBUG
		printf("DEBUG\tNextEBR exist at offset %lld", nextEBR);
		system("pause");
		printf("\n");
#endif // DEBUG
		handleEBR(extendedPartitionStartOffset, nextEBR + extendedPartitionStartOffset);
	}
}

void handleGPT()
{
	int i = 0;
	int numberOfPartitions = 0;
	int partitionEntrySize = 0;
	int entryPerSector = 0;
	UInt64 partitionEntryOffset = 2;
	readSector(SectorSize);

	GptHeader* gptHeader = (GptHeader*) buffer;
	if (buffer[0] != 'E' || buffer[1] != 'F' || buffer[2] != 'I' || buffer[3] != ' '
		|| buffer[4] != 'P' || buffer[5] != 'A' || buffer[6] != 'R' || buffer[7] != 'T')
	{
		printf("Not a GPT disk!\n");
		return;
	}

	UInt32 actualHeaderCRC = gptHeader->HeaderCRC32;
	UInt32 actualEntryCRC = gptHeader->PartitionEntryCRC32;
	gptHeader->HeaderCRC32 = 0;
	GptHeaderCRC = ~crc32(GptHeaderCRC, buffer, 92);
	partitionEntryOffset = gptHeader->PartitionEntryOffset;
	numberOfPartitions = gptHeader -> PartitionEntryCount; // (*(int*)(buffer + 80));
	partitionEntrySize = gptHeader -> PartitionEntrySize; // (*(int*)(buffer + 84));
	entryPerSector = SectorSize / partitionEntrySize;
	printf("GPT Disk. Header Calculated CRC: %08X, Actual CRC: %08X", GptHeaderCRC, actualHeaderCRC);
	if (verbose > 0)
	{
		printGptInfo(gptHeader);
	}
	printf("\n");
	for (i = 0; i < numberOfPartitions * partitionEntrySize / SectorSize; i++)
	{
		int j = 0;
		readSector(SectorSize * (i + partitionEntryOffset));
		GptEntryCRC = crc32(GptEntryCRC, buffer, SectorSize);
		for (j = 0; j < entryPerSector; j++)
		{
			gptEntry(i * entryPerSector + j, j * partitionEntrySize, partitionEntrySize);
		}
	}
	GptEntryCRC = ~GptEntryCRC;
	printf("GPT Entry Calculated CRC: %08X, Actual CRC: %08X\n", GptEntryCRC, actualEntryCRC);
}

void printGptInfo(GptHeader* gptHeader)
{
	printf(" Revision: %d.%d", gptHeader -> MajorRevision, gptHeader -> MinorRevision);
	printf(" Header Size: %d bytes\n", gptHeader -> HeaderSize);
	printf(" Disk ID: "); printGUID(56); printf(" Disk Size: "); printHumanReadableSize((gptHeader->AlternateLBA + 1) * SectorSize);
	if (verbose > 1)
	{
		printf("\n CurrentLBA: %016llX, AlternateLBA: %016llX, EntryLBA: %016llX", gptHeader -> CurrentLBA, gptHeader -> AlternateLBA, gptHeader->PartitionEntryOffset);
	}
}

void gptEntry(int entryNumber, int offset, int partitionEntrySize)
{
	UInt64 startByte = 0;
	UInt64 sizeInByte = 0;
	if (isEmptyGPTSlot(offset))
	{
		return;
	}
	printf("  P%03d:", entryNumber);
	startByte = (*(UInt64*)(buffer + offset + 32)) * SectorSize;
	sizeInByte = (*(UInt64*)(buffer + offset + 40)) * SectorSize;
	sizeInByte -= startByte - SectorSize;
	printf(" Type: ");
	printGUID(offset);
	printf(" ID: ");
	printGUID(offset + 16);
	//printf("\n");
	printf(" Offset: ");
	printHumanReadableSize(startByte);
	printf(" Size: ");
	printHumanReadableSize(sizeInByte);
	printf(" Attribute: ");
	printf("0x%016llX", *(UInt64*)&buffer[offset + 48]);
	printf("\n");
}

bool isEmptyGPTSlot(int offset)
{
	return memcmp(zeroGuid, buffer + offset, 16) == 0;
}

int readSector(Int64 offset)
{
	size_t BytesRead = 0;
	Int64 remainingOffset = offset;
#ifdef DEBUG
	printf("DEBUG\tReading sector at location %lld\n", offset);
#endif // DEBUG
	fseek(file, 0, SEEK_SET);
	while (remainingOffset > 0x7FFFFFFF)
	{
		fseek(file, 0x40000000, SEEK_CUR);
		remainingOffset -= 0x40000000;
	}
	BytesRead = fseek(file, (Int32)remainingOffset, SEEK_CUR);
#ifdef DEBUG
	printf("DEBUG\tfseek returned %d\n", BytesRead);
#endif
	BytesRead = fread(buffer, sizeof(char), SectorSize, file);
#ifdef DEBUG
	printf("DEBUG\tfread returned %d\n", BytesRead);
#endif // DEBUG

	return BytesRead != SectorSize;
}

void printHumanReadableSize(UInt64 number)
{
	if (number > 0x3ffffffffffffUL)
	{
		printf("  ∞TiB    ∞GiB    ∞MiB    ∞KiB    ∞B");
		return;
	}
	printf("%3dTiB ", (int)(number >> 40));
	number &= 0x000000FFFFFFFFFFUL;
	printf("%4dGiB ", (int)(number >> 30));
	number &= 0x000000003FFFFFFFUL;
	printf("%4dMiB ", (int)(number >> 20));
	number &= 0x00000000000FFFFFUL;
	printf("%4dKiB ", (int)(number >> 10));
	number &= 0x00000000000003FFUL;
	printf("%4dB", (int)(number));
}

void printGUID(int offset)
{
	printf("%08X-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX",
		*(UInt32*)&buffer[offset],
		*(UInt16*)&buffer[offset + 4],
		*(UInt16*)&buffer[offset + 6],
		buffer[offset + 8], buffer[offset + 9],
		buffer[offset + 10], buffer[offset + 11], buffer[offset + 12], buffer[offset + 13], buffer[offset + 14], buffer[offset + 15]);
}
