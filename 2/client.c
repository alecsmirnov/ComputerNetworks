#pragma comment(lib, "Ws2_32.lib")
#include <winsock.h>
#include <stdio.h>

#define PORT 2006

#define IP_LEN 16

#define BUF_SIZE 32
#define RESP_SIZE 10000


int main(int argc, char** argv) {	
	WORD ver = MAKEWORD(2, 2);					// Младший байт -- версия, старший -- под.версия
	WSADATA wsa_data;	       					// Информация о версии WinSock: адрес, семейство

	WSAStartup(ver, (LPWSADATA)&wsa_data);		// Инициализация WinSock

	// Получение доменного имени для частных IP-адресов
	// Установка, соединение и передача информации для серверов,
	// работающих на том же компьютере, что и клиент
	LPHOSTENT host_ent = gethostbyname("localhost");
	if (!host_ent) {
		fprintf(stderr, "Unable to collect gethostbyname!\n");
		WSACleanup();
		system("pause");
		return SOCKET_ERROR;
	}

	// Создание клиентского сокета
	// AF_INET -- протокол интернет-приложений
	// SOCK_STREAM -- тип потокового сокета (TCP)
	// IPPROTO_TCP -- транспортный протокол, используемый сокетом (TCP протокол)
	SOCKET client_sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (client_sock == SOCKET_ERROR) {
		fprintf(stderr, "Unable to create socket!\n");
		WSACleanup();
		system("pause");
		return SOCKET_ERROR;
	}

	char ip[IP_LEN];							// Буфер для ввода IP-адреса сервера
	printf("Enter IP: ");
	scanf("%s", ip);

	char buf[BUF_SIZE];							// Буфер для данных, переданных на сервер
	*buf = '\0';								// (Имя файла или запрос на выключение сервера)

	SOCKADDR_IN server_info;					// Параметры адреса для серверного сокета
	server_info.sin_family = PF_INET;			// Протокол / тип адреса
	server_info.sin_addr.S_un.S_addr = inet_addr(ip);
	server_info.sin_port = htons(PORT);			// Порт

	// Устанавливаем соединение с удалённым узлом
	// client_sock -- дескриптор клиентского сокета
	// server_info -- адрес и порт удалённого узла, с которым происходит соединение
	// sizeof(server_info) -- размер структуры для хранения адреса
	int ret_val = connect(client_sock, (LPSOCKADDR)&server_info, sizeof(server_info));
	if (ret_val == SOCKET_ERROR) {
		fprintf(stderr, "Unable to connect!\n");
		WSACleanup();
		system("pause");
		return SOCKET_ERROR;
	}

	printf("Connection made sucessfully!\n");

	printf("Enter name of file: ");
	scanf("%s", buf);

	printf("Sending request from client...\n");

	// Отсылаем данные на сервер
	ret_val = send(client_sock, buf, BUF_SIZE, 0);
	if (ret_val == SOCKET_ERROR) {
		fprintf(stderr, "Unable to send!\n");
		WSACleanup();
		system("pause");
		return SOCKET_ERROR;
	}

	// Получаем ответ от сервера
	char responce[RESP_SIZE];
	ret_val = recv(client_sock, responce, RESP_SIZE, 0);
	if (ret_val == SOCKET_ERROR) {
		fprintf(stderr, "Unable to receive!\n");
		WSACleanup();
		system("pause");
		return SOCKET_ERROR;
	}

	if (buf[0] != 'c' && responce[0] != 'N')
		printf("Content of file:\n%s\n", responce);
	else
		printf("%s\n", responce);

	closesocket(client_sock);
	WSACleanup();								// Освобождение сокетов
	system("pause");

	return 0;
}
