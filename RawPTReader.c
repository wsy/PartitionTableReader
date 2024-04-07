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

typedef struct
{
	char Signature[8];
	UInt16 MinorRevision;
	UInt16 MajorRevision;
	UInt32 HeaderSize;
	UInt32 HeaderCRC32;
	UInt8 Reserved[4];
	UInt64 CurrentLBA;
	UInt64 BackupLBA;
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
void mbrEntry(int offset);
void handleGPT(void);
void printGptInfo(GptHeader* gptHeader);
void gptEntry(int entryNumber, int offset, int partitionEntrySize);
bool isEmptyGPTSlot(int offset);
int readSector(long long offset);
void printHumanReadableSize(UInt64 number);
void printGUID(int offset);

int SectorSize = 512;
FILE *file = NULL;
unsigned char buffer[4096] = { 0 };
char zeroGuid[16] = { '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0' };
UInt32 GptEntryCRC = 0xFFFFFFFF;
UInt32 GptHeaderCRC = 0xFFFFFFFF;

int main(int argc, const char * argv[])
{
	processingArgs(argc, argv);
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
			return -1;
		}
	}
	if (argc > 1)
	{
		return 0;
	}
	else
	{
		printUsage(argv[0]);
	}
	return -1;
}

void printUsage(const char *arg0)
{
	printf("Usage:\n");
	printf("\t%s fileName\n", arg0);
	printf("\t%s fileName [<SectorSize>]\n", arg0);
	printf("\tValid SectorSize is either \"512\"(default) or \"4k\" or \"4K\" or \"4096\"\n");
	exit(0);
}

void handleMBR()
{
	readSector(0L);
	if (buffer[510] != 0x55 || buffer[511] != 0xAA) // 0xAA evaluates to integer -86
	{
		printf("Invalid MBR partition table!\n");
		return;
	}
	printf("MBR Partitions:\n");
	printf("  Partition#1:    ");
	mbrEntry(446);
	printf("  Partition#2:    ");
	mbrEntry(462);
	printf("  Partition#3:    ");
	mbrEntry(478);
	printf("  Partition#4:    ");
	mbrEntry(494);
}

void mbrEntry(int offset)
{
	bool isActive = false;
	unsigned char partitionType = '\0';
	UInt64 startByte = 0;
	UInt64 sizeInByte = 0;
	if (buffer[offset] < 0)
	{
		isActive = true;
	}
	partitionType = buffer[offset + 4];
	startByte = (*(unsigned int*)(buffer + offset + 8)) * (UInt64)SectorSize;
	sizeInByte = (*(unsigned int*)(buffer + offset + 12)) * (UInt64)SectorSize;
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
	if (buffer[510] != 0x55 || buffer[511] != 0xAA) // 0xAA evaluates to integer -86
	{
		printf("    Invalid extended partition table!\n");
		return;
	}
	partitionType = buffer[446 + 4];
#ifdef DEBUG
	printf("DEBUG\tHandling EBR at offset %lld\n", offset);
	printf("DEBUG\tRawData\tStartSector: %02hhX %02hhX %02hhX %02hhX\n", buffer[446 + 8 + 0], buffer[446 + 8 + 1], buffer[446 + 8 + 2], buffer[446 + 8 + 3]);
	printf("DEBUG\tRawData\tSizeInSector: %02hhX %02hhX %02hhX %02hhX\n", buffer[446 + 12 + 0], buffer[446 + 12 + 1], buffer[446 + 12 + 2], buffer[446 + 12 + 3]);
	printf("DEBUG\tRawData\tNextEBRSector: %02hhX %02hhX %02hhX %02hhX\n", buffer[462 + 8 + 0], buffer[462 + 8 + 1], buffer[462 + 8 + 2], buffer[462 + 8 + 3]);
#endif
	startByte = (*(unsigned int*)(buffer + 446 + 8)) * (UInt64)SectorSize + offset;
	sizeInByte = (*(unsigned int*)(buffer + 446 + 12)) * (UInt64)SectorSize;
	printf("    LogicalDrive: ");
	printf(" Offset: ");
	printHumanReadableSize(startByte);
	printf("\tSize: ");
	printHumanReadableSize(sizeInByte);
	printf("\tType: LogicalDrive ");
	printf("%02hhX\n", partitionType);
	nextEBR = (*(unsigned int*)(buffer + 462 + 8)) * (UInt64)SectorSize;
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

	numberOfPartitions = gptHeader -> PartitionEntryCount; // (*(int*)(buffer + 80));
	partitionEntrySize = gptHeader -> PartitionEntrySize; // (*(int*)(buffer + 84));
	entryPerSector = SectorSize / partitionEntrySize;
	printf("GPT Disk. Calculated CRC: %08X, Header CRC: %08X", GptHeaderCRC, actualHeaderCRC);
	printGptInfo(gptHeader);
	for (i = 0; i < numberOfPartitions * partitionEntrySize / SectorSize; i++)
	{
		int j = 0;
		readSector(SectorSize * (i + 2));
		GptEntryCRC = crc32(GptEntryCRC, buffer, SectorSize);
		for (j = 0; j < entryPerSector; j++)
		{
			gptEntry(i * entryPerSector + j, j * partitionEntrySize, partitionEntrySize);
		}
	}
	GptEntryCRC = ~GptEntryCRC;
	printf("GPT Entry Calculated CRC: %08X, Header CRC: %08X\n", GptEntryCRC, actualEntryCRC);
}

void printGptInfo(GptHeader* gptHeader)
{
	printf(" Revision: %d.%d", gptHeader -> MajorRevision, gptHeader -> MinorRevision);
	printf(" Header Size: %d bytes\n", gptHeader -> HeaderSize);
	printf(" Disk ID: "); printGUID(56); printf(" Disk Size: "); printHumanReadableSize((gptHeader -> BackupLBA + 1) * SectorSize); printf("\n");
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

int readSector(long long offset)
{
	size_t BytesRead = 0;
	long long remainingOffset = offset;
#ifdef DEBUG
	printf("DEBUG\tReading sector at location %lld\n", offset);
#endif // DEBUG
	fseek(file, 0, SEEK_SET);
	while (remainingOffset > 0x7FFFFFFF)
	{
		fseek(file, 0x40000000, SEEK_CUR);
		remainingOffset -= 0x40000000;
	}
	BytesRead = fseek(file, (long)remainingOffset, SEEK_CUR);
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
