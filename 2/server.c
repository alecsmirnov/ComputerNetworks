#pragma comment(lib,"Ws2_32.lib")
#include <winsock2.h>
#include <stdio.h>
#include <stdbool.h>

#define PORT 2006

#define IP_LEN 16
#define HOST_NAME_LEN 32

#define ACCEPT_SIZE 32
#define CONNECTION_QUEUE_SIZE 4

#define SHUTDOWN 0


BOOL fileExists(LPCTSTR sz_path) {				// Проверка файла на существование
	DWORD dw_attr = GetFileAttributes(sz_path);

	return (dw_attr != INVALID_FILE_ATTRIBUTES &&
		  !(dw_attr & FILE_ATTRIBUTE_DIRECTORY));
}

int main(int argc, char** argv) {					
	WORD ver = MAKEWORD(2, 2);					// Младший байт -- версия, старший -- под.версия
	WSADATA wsa_data;							// Информация о версии WinSock: адрес, семей-ство

	WSAStartup(ver, (LPWSADATA)&wsa_data);		// Инициализация WinSock

	// Создание серверного сокета
	// AF_INET -- протокол интернет-приложений
	// SOCK_STREAM -- тип потокового сокета (TCP)
	// IPPROTO_TCP -- транспортный протокол, используемый сокетом (TCP протокол)
	SOCKET serv_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (serv_sock == INVALID_SOCKET) {
		fprintf(stderr, "Unable to create socket!\n");
		WSACleanup();							// Освобождение сокетов
		system("pause");
		return SOCKET_ERROR;
	}

	SOCKADDR_IN server_info;					// Параметры адреса для серверного сокета
	server_info.sin_family = PF_INET;			// Протокол / тип адреса
	server_info.sin_port = htons(PORT);			// Порт и его перевод в TCP представление
	server_info.sin_addr.s_addr = INADDR_ANY;	// Привязка ко всем локальным адресам

	// Связывание сокета с адресом и портом
	// serv_sock -- дескриптор сокет
	// server_info -- указатель на структуру адреса
	// sizeof(server_info) -- размер структуры в байтах
	int ret_val = bind(serv_sock, (LPSOCKADDR)&server_info, sizeof(server_info));
	if (ret_val == SOCKET_ERROR) {
		fprintf(stderr, "Unable to bind!\n");
		WSACleanup();
		system("pause");
		return SOCKET_ERROR;
	}

	char host_ip[IP_LEN];						// IP хоста
	char host_name[HOST_NAME_LEN];				// Имя хоста

	// Получаем имя хоста
	if (!gethostname(host_name, HOST_NAME_LEN)) {
		// Получаем адрес сервера
		LPHOSTENT host_ent = gethostbyname(host_name);
		// Получаем IP хоста и преобразуем в 10-ю строку
		if (host_ent)
			strcpy(host_ip, inet_ntoa(*((IN_ADDR*)host_ent->h_addr_list[0])));
	}

	printf("Server started at %s, port %d\n", host_ip, htons(server_info.sin_port));

	bool close = false;
	while (!close) {
		// Слушаем сокет -- переходим в режим ожидания подключений
		// serv_sock -- дескриптор сокета
		// CONNECTION_QUEUE_SIZE -- максимальный размер очереди сообщений
              // (запросов соединений)
		ret_val = listen(serv_sock, CONNECTION_QUEUE_SIZE);
		if (ret_val == SOCKET_ERROR) {
			fprintf(stderr, "Unable to listen!\n");
			WSACleanup();
			system("pause");
			return SOCKET_ERROR;
		}

		// Создание сокета для обмена данными с клиентом, запросившим соединение
		SOCKET client_sock;
		SOCKADDR_IN input;
		int input_len = sizeof(input);

		// Извлечение запроса на соединение из очереди
		// Создание нового сокета, связывание и возврат его дескриптора
		// В input заносятся сведения о подключившемся клиенте (IP-адрес, порт)
		client_sock = accept(serv_sock, (struct sockaddr*)&input, &input_len);
		if (client_sock == INVALID_SOCKET) {
			fprintf(stderr, "Unable to accept!\n");
			WSACleanup();
			system("pause");
			return SOCKET_ERROR;
		}

		printf("New connection accepted from %s, port %d\n", 
                     inet_ntoa(input.sin_addr), htons(input.sin_port));
		
		char accept[ACCEPT_SIZE];				// Буфер для хранения данных, принятых от кли-ента

		// Получаем данные от клиента, чтение из клиентского сокета
		// client_sock -- сокет, из которого читаем
		// accept -- буфер для записи прочитанных данных
		// 0 -- не использовать флаги
		ret_val = recv(client_sock, accept, ACCEPT_SIZE, 0);
		if (ret_val == SOCKET_ERROR) {
			fprintf(stderr, "Unable to recv!\n");
			system("pause");
			return SOCKET_ERROR;
		}

		printf("Data received.\n");
		// Отключение сервера
		if (strcmp(accept, "c") == SHUTDOWN) {
			const char* msg = "Server shutdown!";
			// Посылаем клиенту сообщение о завершении работы сервера
			// client_sock -- сокет, которому посылаем сообщение
			// strlen(msg) + 1 -- длина сообщения
			// 0 -- не использовать флаги
			ret_val = send(client_sock, msg, strlen(msg) + 1, 0);
			closesocket(client_sock);
			return 0;
		}
		else {
			// Файл найден
			if (fileExists(accept)) {
				FILE* f = fopen(accept, "rb");

				fseek(f, 0, SEEK_END);
				size_t buf_size = ftell(f);
				fseek(f, 0, SEEK_SET);

				char* buf = (char*)malloc(sizeof(char) * (buf_size + 1));

				if (buf) {
					fread(buf, 1, buf_size - 1, f);
					buf[buf_size - 1] = '\0';
				}
				else
					fprintf(stderr, "Memory allocation error!\n");

				fclose(f);

				// Посылаем данные клиенту
				// client_sock -- сокет, которому посылаем данные
				// buf -- буфер, хранящий содержимое найденного файла
				// buf_size -- размер буфера
				// 0 -- не использовать флаги
				ret_val = send(client_sock, buf, buf_size, 0);
				printf("Sending response from server\n");
				if (ret_val == SOCKET_ERROR) {
					fprintf(stderr, "Unable to send!\n");
					system("pause");
					return SOCKET_ERROR;
				}

				free(buf);
			}
			else {
				// Файл не найден
				fprintf(stderr, "Requested file is not found!\n");
				const char* msg = "No such file!";
				ret_val = send(client_sock, msg, strlen(msg) + 1, 0);
			}

			closesocket(client_sock);
			printf("Connection closed.\n");
			close = true;
		}
	}

	closesocket(serv_sock);
	WSACleanup();
	system("pause");

	return 0;
}
