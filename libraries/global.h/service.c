#include "Service.h"


Address_t SeperateAddressToIPandPORT(char* AddressIn)
{
	int i = 0;
	Address_t tempAddressSeperated;

	while ( AddressIn[i] != ':' )
	{
		(tempAddressSeperated.IP)[i]=AddressIn[i];
		i++;
	}
	(tempAddressSeperated.IP)[i]='\0';
	i++;

	AddressIn=&(AddressIn[i]);
	tempAddressSeperated.port=atoi(AddressIn);
	return tempAddressSeperated; /// Wherever it returns, Must checking if the port is 0. if its 0 there might be a failure in atoi or the port is illegal
}


Address
