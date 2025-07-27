typedef enum { 
	WSA_CREATE_MGR_SERVER_FAILED,
	WSA_CLOSE_MGR_SERVER_FAILED,
	SOCKET_CLOSE_MGR_SERVER_FAILED,


	
	
} ErrorCode_t;

typedef struct {
	char IP[20];
	int port;
} Address_t;

typedef struct ADDRESS_NODE {
	Address_t AddressAndPort;
	struct ADDRESS_NODE* next;
} AddressNode;
