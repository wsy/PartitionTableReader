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

int processingArgs(int argc, const char* argv[]);
void printUsage(const char *arg0);
void handleMBR(void);
void handleEBR(unsigned long long extendedPartitionStartOffset, unsigned long long offset);
void mbrEntry(int offset);
void handleGPT(void);
void gptEntry(int entryNumber, int offset, int partitionEntrySize);
bool isEmptyGPTSlot(int offset);
int readSector(unsigned long long offset);
void printHumanReadableNumber(unsigned long long number);
void printGUID(int offset);

int SectorSize = 512;
FILE *file = NULL;
char buffer[4096] = { 0 };
char zeroGuid[16]={'\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0', '\0'};
char Hex[256][3] = {
    "00","01","02","03","04","05","06","07","08","09","0A","0B","0C","0D","0E","0F",
    "10","11","12","13","14","15","16","17","18","19","1A","1B","1C","1D","1E","1F",
    "20","21","22","23","24","25","26","27","28","29","2A","2B","2C","2D","2E","2F",
    "30","31","32","33","34","35","36","37","38","39","3A","3B","3C","3D","3E","3F",
    "40","41","42","43","44","45","46","47","48","49","4A","4B","4C","4D","4E","4F",
    "50","51","52","53","54","55","56","57","58","59","5A","5B","5C","5D","5E","5F",
    "60","61","62","63","64","65","66","67","68","69","6A","6B","6C","6D","6E","6F",
    "70","71","72","73","74","75","76","77","78","79","7A","7B","7C","7D","7E","7F",
    "80","81","82","83","84","85","86","87","88","89","8A","8B","8C","8D","8E","8F",
    "90","91","92","93","94","95","96","97","98","99","9A","9B","9C","9D","9E","9F",
    "A0","A1","A2","A3","A4","A5","A6","A7","A8","A9","AA","AB","AC","AD","AE","AF",
    "B0","B1","B2","B3","B4","B5","B6","B7","B8","B9","BA","BB","BC","BD","BE","BF",
    "C0","C1","C2","C3","C4","C5","C6","C7","C8","C9","CA","CB","CC","CD","CE","CF",
    "D0","D1","D2","D3","D4","D5","D6","D7","D8","D9","DA","DB","DC","DD","DE","DF",
    "E0","E1","E2","E3","E4","E5","E6","E7","E8","E9","EA","EB","EC","ED","EE","EF",
    "F0","F1","F2","F3","F4","F5","F6","F7","F8","F9","FA","FB","FC","FD","FE","FF"
};

int main(int argc, const char * argv[])
{
    processingArgs(argc, argv);
    file = fopen(argv[1], "rb");
    if (!file)
    {
        printf("Open failed!\n");
        return 1;
    }
    printf("Open succeed!\n");
    handleMBR();
    handleGPT();
    return 0;
}

int processingArgs(int argc, const char* argv[])
{
    if (argc > 2)
    {
        if(strcmp(argv[2], "4k") == 0 || strcmp(argv[2], "4K") == 0)
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
    printf("\tValid SectorSize is either 512(default) or 4k or 4K");
    exit(0);
}

void handleMBR()
{
    readSector(0L);
    if(buffer[510] != 0x55 || buffer[511] != -86) // 0xAA evaluates to integer -86
    {
        printf("Invalid MBR partition table!");
        return;
    }
    printf("MBR Partitions:\n");
    printf("  Partition#1:");
    mbrEntry(446);
    printf("  Partition#2:");
    mbrEntry(462);
    printf("  Partition#3:");
    mbrEntry(478);
    printf("  Partition#4:");
    mbrEntry(494);
}

void mbrEntry(int offset)
{
    bool isActive = false;
    char partitionType = '\0';
    unsigned long long startByte = 0;
    unsigned long long sizeInByte = 0;
    if(buffer[offset] < 0)
    {
        isActive = true;
    }
    partitionType = buffer[offset + 4];
    startByte = (*(int*)(buffer + offset + 8)) * SectorSize;
    sizeInByte = (*(int*)(buffer + offset + 12)) * SectorSize;
    printf(" Offset: ");
    printHumanReadableNumber(startByte);
    printf(" Size: ");
    printHumanReadableNumber(sizeInByte);
    printf(" Type: ");
    if(partitionType=='\0')
    {
        // Empty partition slot!
        printf("Empty Slot!\n");
        return;
    }
    else if(partitionType != 0x0f && partitionType != 0x05)
    {
        // Primary partition
        printf("Primary Partition %s\n", Hex[(unsigned char)partitionType]);
        return;
    }
    // Extended partition
    printf("ExtendedPartition %s\n", Hex[(unsigned char)partitionType]);
    handleEBR(startByte, startByte);
}

void handleEBR(unsigned long long extendedPartitionStartOffset, unsigned long long offset)
{
    char partitionType = '\0';
    unsigned long long startByte = 0;
    unsigned long long sizeInByte = 0;
    unsigned long long nextEBR = 0;
    readSector(offset);
    if(buffer[510] != 0x55 || buffer[511] != -86) // 0xAA evaluates to integer -86
    {
        printf("    Invalid extended partition table!\n");
        return;
    }
    partitionType = buffer[446 + 4];
    startByte = (*(int*)(buffer + 446 + 8)) * SectorSize + offset;
    sizeInByte = (*(int*)(buffer + 446 + 12)) * SectorSize;
    printf("    LogicalDrive:");
    printf(" Offset: ");
    printHumanReadableNumber(startByte);
    printf(" Size: ");
    printHumanReadableNumber(sizeInByte);
    printf(" Type: LogicalDrive ");
    printf("%s\n", Hex[(unsigned char)partitionType]);
    nextEBR = (*(int*)(buffer + 462 + 8)) * SectorSize;
    if(nextEBR != 0)
    {
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
    if(buffer[0] != 'E' || buffer[1] != 'F' || buffer[2] != 'I' || buffer[3] != ' '
       ||buffer[4] != 'P' || buffer[5] != 'A' || buffer[6] != 'R' || buffer[7] != 'T')
    {
        printf("Not a GPT disk!\n");
        return;
    }
    numberOfPartitions = (*(int*)(buffer + 80));
    partitionEntrySize = (*(int*)(buffer + 84));
    entryPerSector = SectorSize / partitionEntrySize;
    printf("GPT Disk.");
    printf(" DiskID: ");
    printGUID(56);
    printf("\n");
    for(i = 0; i < numberOfPartitions * partitionEntrySize / SectorSize; i++)
    {
        int j = 0;
        readSector(SectorSize * (i + 2));
        for(j = 0; j < entryPerSector; j++)
        {
            gptEntry(i * entryPerSector + j, j * partitionEntrySize, partitionEntrySize);
        }
    }
}

void gptEntry(int entryNumber, int offset, int partitionEntrySize)
{
    unsigned long long startByte = 0;
    unsigned long long sizeInByte = 0;
    if(isEmptyGPTSlot(offset))
    {
        return;
    }
    printf("  P%03d:", entryNumber);
    startByte = (*(long*)(buffer + offset + 32)) * SectorSize;
    sizeInByte = (*(long*)(buffer + offset + 40)) * SectorSize;
    sizeInByte -= startByte - SectorSize;
    printf(" Type: ");
    printGUID(offset);
    printf(" ID: ");
    printGUID(offset + 16);
    //printf("\n");
    printf(" Offset: ");
    printHumanReadableNumber(startByte);
    printf(" Size: ");
    printHumanReadableNumber(sizeInByte);
    printf(" Attribute: ");
    printf("0x%s%s%s%s%s%s%s%s",
           Hex[buffer[offset + 48 + 7]], Hex[buffer[offset + 48 + 6]],
           Hex[buffer[offset + 48 + 5]], Hex[buffer[offset + 48 + 4]],
           Hex[buffer[offset + 48 + 3]], Hex[buffer[offset + 48 + 2]],
           Hex[buffer[offset + 48 + 1]], Hex[buffer[offset + 48 + 0]]);
    printf("\n");
}

bool isEmptyGPTSlot(int offset)
{
    return memcmp(zeroGuid, buffer+offset, 16) == 0;
}

int readSector(unsigned long long offset)
{
    int BytesRead = 0;
    fseek(file, offset, SEEK_SET);
    for (BytesRead = 0; BytesRead < SectorSize; BytesRead++)
    {
        fscanf(file, "%c", &buffer[BytesRead]);
        if (feof(file))
        {
            break;
        }
    }
    return BytesRead != SectorSize;
}

void printHumanReadableNumber(unsigned long long number)
{
    if (number > 0x3ffffffffffffUL)
    {
        printf(" ∞TiB    ∞GiB    ∞MiB    ∞KiB    ∞B");
        return;
    }
    printf("% 2dTiB ", (int)(number >> 40));
    number &= 0x000000FFFFFFFFFFUL;
    printf("% 4dGiB ", (int)(number >> 30));
    number &= 0x000000003FFFFFFFUL;
    printf("% 4dMiB ", (int)(number >> 20));
    number &= 0x00000000000FFFFFUL;
    printf("% 4dKiB ", (int)(number >> 10));
    number &= 0x00000000000003FFUL;
    printf("% 4dB", (int)(number));
}

void printGUID(int offset)
{
    printf("%s%s%s%s-%s%s-%s%s-%s%s-%s%s%s%s%s%s",
           Hex[(unsigned char)buffer[offset+3]], Hex[(unsigned char)buffer[offset+2]], Hex[(unsigned char)buffer[offset+1]], Hex[(unsigned char)buffer[offset]],
           Hex[(unsigned char)buffer[offset+5]], Hex[(unsigned char)buffer[offset+4]], Hex[(unsigned char)buffer[offset+7]], Hex[(unsigned char)buffer[offset+6]],
           Hex[(unsigned char)buffer[offset+8]], Hex[(unsigned char)buffer[offset+9]], Hex[(unsigned char)buffer[offset+10]], Hex[(unsigned char)buffer[offset+11]],
           Hex[(unsigned char)buffer[offset+12]], Hex[(unsigned char)buffer[offset+13]], Hex[(unsigned char)buffer[offset+14]], Hex[(unsigned char)buffer[offset+15]]);
}














