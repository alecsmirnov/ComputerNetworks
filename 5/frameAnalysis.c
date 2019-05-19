#define _CRT_SECURE_NO_WARNINGS
#include "frameAnalysis.h"

#pragma comment(lib,"Ws2_32.lib")
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include <Windows.h>

#define NO_OFFSET 0				// Признак отсутвия смещения

#define FRAME_DATA_SIZE  1500	// Размер данных фрейма RAW, SNAP, LLC
#define FRAMETYPES_COUNT 6

#define ETHERNET_II_LEN 14		// Длина протокола Ethernet II

// Общие данные фреймов
#define MAC_LEN       6			// Длина МАС адреса
#define FRAMETYPE_LEN 2			// Длина типа

// Данные фрейма IPv4 (протокола IP)
#define IP_LEN		   4		// Длина IP адреса
#define IP_VERSION_LEN 2		// Длина версии IP
#define IP_INDENT_LEN  12		// Отступ до IP адресов

// Данные фрейма ARP (протокола ARP)
#define SENDER_OFFSET_LEN 14	// Смещение до адреса отправителя
#define TARGET_OFFSET_LEN 24	// Смещение до адреса цели

// Данные фреймов RAW, SNAP, LLC 
#define DSC_LEN  3				// Заголовок LLC
#define SNAP_LEN 5				// Заголовок SNAP
#define FCS_LEN  4				// Длина проверки целостности фрейма

// Длины/типы фреймов в HEX представлении
typedef enum FrameLT {
	FRAME_IPv4 = 0x0800,
	FRAME_ARP  = 0x0806,
	FRAME_RAW  = 0xFFFF,
	FRAME_SNAP = 0xAAAA,
} FrameLT;

// Тип фрейма в списке фреймов
typedef enum {IPv4, ARP, DIX, RAW, SNAP, LLC} FrameType;

// Элемент списка фреймов
typedef struct Frame {
	const char* name;				// Имя
	size_t count;					// Количетсов
} Frame;

// Список фреймов
typedef struct FrameInfo {
	Frame frame[FRAMETYPES_COUNT];	// Перечень фреймов
	size_t frame_count;				// Общее кол-во
} FrameInfo;

// Инициализация списка фреймов
static void frameInfoInit(FrameInfo* info) {
	info->frame_count = 0;

	info->frame[IPv4] = (Frame){"IPv4", 0};
	info->frame[ARP]  = (Frame){"ARP",  0};
	info->frame[DIX]  = (Frame){"DIX",  0};
	info->frame[RAW]  = (Frame){"RAW",  0};
	info->frame[SNAP] = (Frame){"SNAP", 0};
	info->frame[LLC]  = (Frame){"LLC",  0};
}

// Проверка МАС адреса на пустоту (весь из 0)
static bool isEmptyMAC(byte* data) {
	bool check = true;
	for (byte* ptr = data; ptr < data + MAC_LEN && check; ++ptr)
		if (*ptr != 0)
			check = false;

	return check;
}

// Получить размер файле по его имени
static size_t getFileSize(const char* filename) {
	size_t size = 0;

	FILE* fp;
	if (fp = fopen(filename, "rb")) {
		fseek(fp, NO_OFFSET, SEEK_END);
		size = ftell(fp);
		fseek(fp, NO_OFFSET, SEEK_SET);

		fclose(fp);
	}
	else {
		fprintf(stderr, "Error: can't open file \"%s\" to get size!\n", filename);
		exit(EXIT_FAILURE);
	}

	return size;
}

// Вывести на экран МАС адрес
static void printMAC(byte* data) {
	byte* ptr = data;

	for (; ptr < data + MAC_LEN - 1; ++ptr)
		printf("%02X:", *ptr);
	printf("%02X\t|\n", *ptr);
}

// Вывести на экран IP адрес
static void printIP(byte* data) {
	byte* ptr = data;

	for (; ptr < data + IP_LEN - 1; ++ptr)
		printf("%d.", *ptr);
	printf("%d\t\t|\n", *ptr);
}

// Вывести на экран тип фрейма и увеличить кол-во
// фреймов заданного типа, общее кол-во фреймов
static void printFrame(FrameInfo* info, FrameType type) {
	printf("Frame type:\t\t %s\t\t\t|\n", info->frame[type].name);

	++info->frame[type].count;
	++info->frame_count;
}

// Вывести на экран информацию о всех фреймах
static void printFramesInfo(FrameInfo* info, byte* data, size_t data_size) {
	for (byte* ptr = data; ptr < data + data_size;) {
		printf("________________________________________________\n");
		printf("Frame:\t\t\t %02zd\t\t\t|\n", info->frame_count + 1);

		// Пропустить пустые МАС адреса
		while (isEmptyMAC(ptr))
			ptr += MAC_LEN;

		printf("Destination MAC address: ");
		printMAC(ptr);

		printf("Source MAC address:\t ");
		printMAC(ptr + MAC_LEN);

		// Получить длину/тип фрейма в десятичном формате
		uint16_t frame_LT = ntohs(*(uint16_t*)(ptr + 2 * MAC_LEN));

		if (FRAME_DATA_SIZE < frame_LT)		// Если тип фрейма не Ethernet II
			switch (frame_LT) {
				case FRAME_IPv4: {
					printFrame(info, IPv4);

					printf("Source IP address:\t ");
					printIP(ptr + 2 * MAC_LEN + FRAMETYPE_LEN + IP_INDENT_LEN);

					printf("Destination IP address:\t ");
					printIP(ptr + 2 * MAC_LEN + FRAMETYPE_LEN + IP_INDENT_LEN + IP_LEN);

					frame_LT = ntohs(*(uint16_t*)(ptr + 2 * MAC_LEN + FRAMETYPE_LEN + IP_VERSION_LEN)) + ETHERNET_II_LEN;
					printf("Data size:\t\t %hu bytes\t\t|\n", frame_LT);

					ptr += frame_LT;
					break;
				}
				case FRAME_ARP: {
					printFrame(info, ARP);

					printf("Sender IP address: ");
					printIP(ptr + 2 * MAC_LEN + FRAMETYPE_LEN + SENDER_OFFSET_LEN);

					printf("Target IP address: ");
					printIP(ptr + 2 * MAC_LEN + FRAMETYPE_LEN + TARGET_OFFSET_LEN);

					ptr += 2 * MAC_LEN + FRAMETYPE_LEN + SENDER_OFFSET_LEN + ETHERNET_II_LEN;
					break;
				}
				default: {
					printFrame(info, DIX);

					ptr += frame_LT + ETHERNET_II_LEN;
				}
			}
		else {								// Если тип фрейма Ethernet II
			// Два байта загаловка LLC в десятичном представлении 
			uint16_t frame_LLC = ntohs(*(uint16_t*)(ptr + 2 * MAC_LEN + FRAMETYPE_LEN));

			switch (frame_LLC) {
				case FRAME_RAW:  printFrame(info, RAW);  break;
				case FRAME_SNAP: printFrame(info, SNAP); break;
				default:		 printFrame(info, LLC);
			}

			ptr += frame_LT + FRAMETYPE_LEN + DSC_LEN + SNAP_LEN + FCS_LEN;
		}
		printf("________________________________________________|\n");
	}
}

// Вывести типы фреймов, их кол-во и общее число фреймов
static void printFramesCount(FrameInfo* info) {
	printf("___________________\n");
	printf("Frame type | Count |\n");
	printf("===========|=======|\n");
	for (uint8_t i = 0; i != FRAMETYPES_COUNT; ++i)
		printf("%s \t   | %-5zd |\n", info->frame[i].name, info->frame[i].count);
	printf("___________|_______|\n");

	printf("Total: \t     %zd    |\n", info->frame_count);
	printf("___________________|\n");
}

// Вывести содержимое файла фреймов по его имени
static void printFrameFile(const char* filename) {
	FILE* fp;

	if (fp = fopen(filename, "rb")) {
		size_t data_size = getFileSize(filename);
		byte *data = (byte*)malloc(sizeof(byte) * data_size);

		fread(data, sizeof(byte), data_size, fp);
		fclose(fp);

		FrameInfo info;
		frameInfoInit(&info);

		printFramesInfo(&info, data, data_size);
		printFramesCount(&info);

		free(data);
	}
	else {
		fprintf(stderr, "Error: can't open file \"%s\"!\n", filename);
		exit(EXIT_FAILURE);
	}
}

// Функция анализа фреймов по введённому имени файла
void frameAnalysis() {
	char filename[FILENAME_SIZE];

	printf("Enter filename: ");
	scanf("%s", filename);

	printFrameFile(filename);
}