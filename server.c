#include "functions.h"
#include <sys/time.h>
#include <signal.h>


int desc;
void terminate(){
	close(desc);
	exit(0);
}

int main(int argc, char* argv[]){
	signal(SIGINT, terminate);
	//Initialisation
	struct sockaddr_in adressServer;
	struct sockaddr_in adressClient;
	struct sockaddr_in adressData;
	int sizeResult = sizeof(adressData);
	int descData;	
	socklen_t adressClientLength;
	int nbClients = 0;
	int port, port_data;
	char buffer[RCVSIZE];
	char bufferACK[11];
	char bufferFIN[4] = "FIN\0";
	int msg, try, ACK;
	FILE* file;
	int nbChar = 0, seq=1;

	//Test the number of args
	testArg(&argc);
	port = atoi(argv[1]);
	port_data = port;

	//Socket initialisation
	openSocketUDP(&desc);
	editStructurAdress(&adressServer, port, INADDR_ANY);
	bindServer(&desc, &adressServer);

	while(1){
		if(handShakeServer(&desc,&adressClient, port_data+1) == 1){
			port_data++;
			// Initialize new socket				
			openSocketUDP(&descData);
			printf("port_data : %d\n", port_data);
			editStructurAdress(&adressData, port_data, INADDR_ANY);
			bindServer(&descData, &adressData);

			int pid=fork();
			if(pid==0){ // Processus fils : send data
				char purData[RCVSIZE-6];
				// Receive filename and open the file (and we get here the new client adress, which we'll use to send data)
				char filename[100];
				receiveFileName(descData, adressClient, filename);
				file = fopen(filename, "rb");
				FILE* file2 = fopen("cloned_by_server.jpg", "wb");
				// Calcul file size and send it to the server
				fseek (file , 0 , SEEK_END);
				int size = ftell(file);
				printf("size : %d\n", size);
				fseek(file,0,SEEK_SET);
				// Create file descriptor set in order to test if ACK is received or not
				fd_set set;
				FD_ZERO(&set);
				// send paquet
				while((size-nbChar)>(RCVSIZE-6)){
					int res = fread(purData, 1, RCVSIZE-6, file);
					// copy on 'cloned_by_server.jpg' just for test
					int ecrit = fwrite(purData, sizeof(char), sizeof(purData), file2);
					usleep(1);
					try = sendData(seq, buffer, purData, descData, adressClient, sizeof(adressClient), RCVSIZE);
					printf("%d bytes sent\n", try);
					// test reception ACK
					int sizeResult;
					ACK=receiveACK_Segment(bufferACK, descData, adressClient, &sizeResult, set);
					while(ACK==-1){
						// Resend the paquet
						try = sendData(seq, buffer, purData, descData, adressClient, sizeof(adressClient), RCVSIZE);
						ACK=receiveACK_Segment(bufferACK, descData, adressClient, &sizeResult, set);
					}
					seq++;				
					printf("ACK received : %d\n", ACK);
					nbChar+=(try-6);
				}
				// read last part (less than RCSIZE-6) and send it
				int bytesAtEnd = size-nbChar;
				char bufferEnd[bytesAtEnd];
				char bufferEndWithSeq[bytesAtEnd+6];
				int resultat = fread(bufferEnd, 1, bytesAtEnd, file);
				int ecrit = fwrite(bufferEnd, sizeof(char), sizeof(bufferEnd), file2);
				usleep(1);
				try = sendData(seq, bufferEndWithSeq, bufferEnd, descData, adressClient, sizeof(adressClient), (bytesAtEnd+6));
				// test reception ACK
				int sizeResult;
				ACK=receiveACK_Segment(bufferACK, descData, adressClient, &sizeResult, set);
				printf("%d bytes sent\n", try);
				while(ACK==-1){
					// Resend the paquet
					try = sendData(seq, bufferEndWithSeq, bufferEnd, descData, adressClient, sizeof(adressClient), (bytesAtEnd+6));
					ACK=receiveACK_Segment(bufferACK, descData, adressClient, &sizeResult, set);
				}
				printf("ACK received : %d\n", ACK);

				// send 'FIN'
				sendto(descData, bufferFIN, sizeof(bufferFIN), 0, (struct sockaddr*)&adressClient, sizeof(adressClient));
				printf("File sent !\n");

				fflush(file);
				fclose(file);
				close(descData);
			}else{
				// Processus pere
				close(descData);
 			}
		}
	}
	close(desc);
	return 1;
}
