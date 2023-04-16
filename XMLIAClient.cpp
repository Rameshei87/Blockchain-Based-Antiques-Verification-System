/**
    @file XMLIAClient.cpp
    @brief This file contains the definition for all the member functions of the XMLIAClient class
    @author Anoop Viswambharan

*/

#include <XMLIAClient.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ABL_Exception.h>

extern ABL_Logger gABLLoggerObj;    //!< Global ABL Logger Object for logging

using namespace SPS;


/**
 * @fn sendBytes
 * @param The message which has to be send over Socket
 * @ret returns the number of bytes sent
 * @brief This member function is used to send the xml message over TCP to SPS. During any failure, the fucntion will throw an error which should be handled 
		in the calling function
 */
int XMLIAClient::sendBytes(char *pMessage)
{
	int 	lReturn;	//!< Used to hold the return value of the send call

	//! Sending the message over TCP to SPS
	lReturn = send(_socketDesc, pMessage, strlen(pMessage), 0);
	memset(_logMsgBuf, '\0', sizeof(_logMsgBuf));
	sprintf(_logMsgBuf, "Fired : %s", pMessage);
	gABLLoggerObj<<INFO<<_logMsgBuf<<Endl;
	if (0 == lReturn)
	{
		throw ABL_Exception(5016, __FILE__, __LINE__, "Unable to send data over socket. Socket Error");
	}
	
	//! Returning the number of bytes send
	return lReturn;
}

/**
 * @fn recvResponse
 * @param Nil
 * @ret returns the response received over socket
 * @brief This member function is used to receive the response from SPS
 */
std::string XMLIAClient::recvResponse()
{
	int 	lReturn;			//! Used to hold the return value for recv
	char 	lResponse[4096];	//!< Character buffer to hold the response from SPS
	
	memset(lResponse, '\0', sizeof(lResponse));
	//! Receiving the response from SPS
	lReturn = recv(_socketDesc, lResponse, 4095, 0);

	//! If there is any error in receiving the response from SPS, throw an exception
	if (lReturn <= 0)
	{
		throw ABL_Exception(5016, __FILE__, __LINE__, "Unable to receive data on socket. Socket Error");
	}
	
	//! On successful receive, return the message received over socket to the calling fucntion
	memset(_logMsgBuf, '\0', sizeof(_logMsgBuf));
	sprintf(_logMsgBuf, "Response : %s", lResponse);
	gABLLoggerObj<<INFO<<_logMsgBuf<<Endl;
	
	return lResponse;
}//std::string XMLIAClient::recvResponse()



/**
 * @fn establishSPSConnection
 * @param Nil
 * @ret returns 0 on success and -1 on failure
 * @brief This member function create a socket, and establish connection to the active SPS Server
 */
int XMLIAClient::establishSPSConnection()
{
	int 	lIndex;					//!< Used as index in loops
	int 	lIndex2;				//!< Used as index in loops
	int 	lReturn;				//!< Used to hold the return values for called fucntions
	struct sockaddr_in lServAdd;	//!< Used to hold the address of the SPS Server



	//! Looping through the SPSServerInfoVector to get the details of the active SPS server
	for (lIndex = 0; lIndex < spsSerInfoVec.size(); lIndex++)
	{
		//! Creating a socket to connect to SPS	
		_socketDesc = socket(AF_INET, SOCK_STREAM, 0);

		//! If socket creation fails, return -1 to calling function	
		if (_socketDesc < 0)
		{
			gABLLoggerObj<<CRITICAL<<"Unable to Create Socket"<<Endl;
			return -1;
		}

		//! Storing the address of the SPS server into lServAdd
		memset(&lServAdd, 0, sizeof(lServAdd));
		lServAdd.sin_family = AF_INET;
		lServAdd.sin_port = htons(spsSerInfoVec[lIndex]->portNum);
		lServAdd.sin_addr.s_addr = inet_addr(spsSerInfoVec[lIndex]->ipAddress);
		
		memset(_logMsgBuf, '\0', sizeof(_logMsgBuf));
		sprintf(_logMsgBuf, "Establishing Connenction to : %s on Port : %d", spsSerInfoVec[lIndex]->ipAddress, spsSerInfoVec[lIndex]->portNum);
		gABLLoggerObj<<INFO<<_logMsgBuf<<Endl;
		//! Trying to connect to SPS. If any failure happens, system will retry for the configured attempts	
		for (lIndex2 = 0; lIndex2 < spsSerInfoVec[lIndex]->retryAttempt; lIndex2++)
		{
			//! Connecting to the Server
			lReturn = connect(_socketDesc, (struct sockaddr*) &lServAdd, sizeof(lServAdd));	

			//! During failure, system will retry after the configured interval
			if (lReturn == -1)
			{
				sleep(spsSerInfoVec[lIndex]->retryInterval);
				continue;
			}
			strcpy(_spsHomePath, spsSerInfoVec[lIndex]->spsHomePath);
			//! On succesful connection, it returns 0
			//! Calling the login function to login to SPS
			return (login());
	
		}
	}
	return -1;
}//int XMLIAClient::establishSPSConnection()



/**
 * @fn login
 * @param Nil
 * @ret returns 0 on success and -1 on failure
 * @brief This member function is used to login to the SPS server. This fucntion prepares the login command and validates the response
 */
int XMLIAClient::login()
{
	char 		lLoginRequest[4096];	//!< Character buffer to hold the login request
	char 		lLoginResp[4096];		//!< Character buffer to hold the response for the login request
	std::string lRespStr;				//!< Temporary string to hold the response
	char 		*lpStr;					//!< Character pointer to validate the response
	int lReturn;
	
	//! Creating the login request by substituing the username and password. Current version supports only plain authentication
//	strcpy(lLoginRequest, "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Login xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:noNamespaceSchemaLocation=\"/home/sps/06-Provisioning/trunk/SPS_5.1.0.0/trunk/Conf/SPS.xsd\"><Username>");
	strcpy(lLoginRequest, "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Login xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:noNamespaceSchemaLocation=\"");
	strcat(lLoginRequest, _spsHomePath);
	strcat(lLoginRequest, "/Conf/SPS.xsd\"><Username>");
	strcat(lLoginRequest, pOssUserInfo->userName);
	strcat(lLoginRequest, "</Username><Password>");
	strcat(lLoginRequest, pOssUserInfo->password);
	strcat(lLoginRequest, "</Password><Created></Created><Nonce></Nonce></Login>");

	//! Sending the login request	
	try
	{	
		lReturn = sendBytes(lLoginRequest);
	}	
	catch (ABL_Exception &e)
	{
                gABLLoggerObj<<_ERROR<<"Unable to Send Login Command"<<Endl;
		return -1;
	}

	//! Receiving the response	
	try
	{
		lRespStr.clear();	
		lRespStr = recvResponse();
	}	
	catch (ABL_Exception &e)
	{
                gABLLoggerObj<<_ERROR<<"Unable to Receive Login Response"<<Endl;
		return -1;
	}
//	strcpy(lLoginResp, lRespStr.c_str());

	
	//! Checking for the sub string Login Successful in the received response.If its present, then login is successfull and if not its a login failure
	//lpStr = strstr(lRespStr.c_str(), "Login Successful");
	lpStr = const_cast<char*> (strstr(lRespStr.c_str(), "Login Successful"));
	if (NULL == lpStr)
	{
		gABLLoggerObj<<_ERROR<<"Login to SPS Failed. Please check username and Password"<<Endl;
		//! Returning -1 on login failure
		return -1;
	}
	memset(_logMsgBuf, '\0', sizeof(_logMsgBuf));
        sprintf(_logMsgBuf, "Successfully Logged in to SPS for User : %s", pOssUserInfo->userName);
        gABLLoggerObj<<INFO<<_logMsgBuf<<Endl;
	
	return 0;
	
}//int XMLIAClient::login()



/**
 * @fn logout
 * @param Nil
 * @ret returns 0 on success and -1 on failure
 * @brief This member function s used to prepare and send the logout command to the SPS server.
 */
int XMLIAClient::logout()
{
	char 		lLogoutRequest[4096];	//!< Character buffer to hold the logout request
	std::string lRespStr;				//!< String used to get the response for logout request
	int 		lReturn;				//!< Used to hold the return value from called function

	//! Preparing the logout command
	strcpy(lLogoutRequest, "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Logout xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:noNamespaceSchemaLocation=\"");

strcat(lLogoutRequest, _spsHomePath);
    strcat(lLogoutRequest, "/Conf/SPS.xsd\"></Logout>");

	//! Sending the logout command. On any error, fucntion returns -1
	try
    {
        lReturn = sendBytes(lLogoutRequest);
    }
    catch (ABL_Exception &e)
    {
        gABLLoggerObj<<_ERROR<<"Unable to Send Logout Command"<<Endl;
        return -1;
    }
	
	//! Receiving the response for logout. If any error occurs, function returns -1
    try
    {
		lRespStr.clear();
        lRespStr = recvResponse();
    }
    catch (ABL_Exception &e)
    {
        gABLLoggerObj<<_ERROR<<"Unable to Receive Logout Response"<<Endl;
        return -1;
    }
        gABLLoggerObj<<INFO<<"Successfully Logged Out from SPS"<<Endl;
}//int XMLIAClient::logout()



/**
 * @fn startProcess
 * @param Nil
 * @ret void
 * @brief This is a threaded fucntion and it loops infinitely for any messages coming in the request queue. Post receiving the request, it forwards the same to SPS, gets the response back and push it to the response queue.
 */
void XMLIAClient::startProcess()
{
	int 		lReturn;				//!< Used to hold the return value in funtion calls
	int 		lResponseLen;			//!< Used to hold the response length	
	std::string lResp;					//!< Response will be stored to this variable
//	char 		lResponse[4096];
	MsqQueStruct lReqMsgQueStructObj;	//!< Structure to store the request from the Request Message Queue
	MsqQueStruct lRespMsgQueStructObj;	//!< Structure to hold the response which has to be pushed to the Response Message Queue

	//!< Releasing the move forward Semaphore so that the Start can return to the calling function
	moveForwardSem.mb_release();		

	//! Setting the isconnected flag to true	
	isConnected = true;

	bool lSecondAttempt = false;

	//! Start of an infinite while loop to read the requests from the Message Queue  and send it to SPS
	//! Get the response and send the response back to the response message queue.
	while(true)
	{
		std::cout << "Starting to Read from Queue " << std::endl;
		
		//! Reading the message into the MsqQueStruct object
		memset(lReqMsgQueStructObj.xmlRequest, '\0', 4096);
		lReqMsgQueStructObj = pOssUserInfo->GetMessage();

		memset(_logMsgBuf, '\0', sizeof(_logMsgBuf));
        	sprintf(_logMsgBuf, "Request Received : %s", lReqMsgQueStructObj.xmlRequest);
        	gABLLoggerObj<<INFO<<_logMsgBuf<<Endl;
		
		//!< If the message received is a stop signal
		if (!strcmp(lReqMsgQueStructObj.xmlRequest, "STOP"))
		{
                	gABLLoggerObj<<INFO<<"Stop Signal Received From Parent"<<Endl;
			break;
		}
		
		//! Checking if the message is an error message due to any failure in retreiving the request from the queue.
		if (!strcmp(lReqMsgQueStructObj.xmlRequest, "Error"))
		{
                	gABLLoggerObj<<_ERROR<<"Unable to retrieve the message from the Request Queue"<<Endl;
			break;
		}

		//! Sending the Message to SPS over TCP.
		try
		{
			lReturn = sendBytes(lReqMsgQueStructObj.xmlRequest);
		}
		catch (ABL_Exception &e)
		{


			// Added to try establishing connection to SPS infinitely
			close(_socketDesc);
			
			

			isConnected = false;
			lReturn = establishSPSConnection();
			if ( 0 == lReturn )
			{
				isConnected = true;
			}
			else
			{
				//! If any failure happens in sending the message to SPS, send a hardcoded response in the Response queue.
				//! This message will be picked by the Service Layer and a Service Unavailable error will be send back to the client
				memset(lRespMsgQueStructObj.xmlRequest, '\0', 4096);
				strcpy(lRespMsgQueStructObj.xmlRequest, "s:17:\"SessionLayerError\";");
				memset(_logMsgBuf, '\0', sizeof(_logMsgBuf));
                		sprintf(_logMsgBuf, "Response Sent : %s", lRespMsgQueStructObj.xmlRequest);
                		gABLLoggerObj<<INFO<<_logMsgBuf<<Endl;
	        		lRespMsgQueStructObj.mType = lReqMsgQueStructObj.mType;
    	    			pOssUserInfo->PushMessage(lRespMsgQueStructObj);

				//! Decrementing the connection count and exiting	
				pOssUserInfo->DecrementConnectionCount();
				
				break;
			}
			if (isConnected == true)
			{
				lReturn = sendBytes(lReqMsgQueStructObj.xmlRequest);	
			}
			else
			{
				break;
			}
				
		}
		//memset(lResponse, 0, sizeof(lResponse));

		//! Receiving the response from the SPS.
		try
		{
			lResp.clear();
			lResp = recvResponse();
		}
		catch (ABL_Exception &e)
		{
            		close(_socketDesc);


            		isConnected = false;
                	lReturn = establishSPSConnection();
                	if ( 0 == lReturn )
                	{
				isConnected = true;
                	}
			else
			{
				memset(lRespMsgQueStructObj.xmlRequest, 0, 4096);
                        	strcpy(lRespMsgQueStructObj.xmlRequest, "s:17:\"SessionLayerError\";");
                        	lRespMsgQueStructObj.mType = lReqMsgQueStructObj.mType;
                        	pOssUserInfo->PushMessage(lRespMsgQueStructObj);

                        	memset(_logMsgBuf, '\0', sizeof(_logMsgBuf));
                        	sprintf(_logMsgBuf, "Response Sent : %s", lRespMsgQueStructObj.xmlRequest);
                        	gABLLoggerObj<<INFO<<_logMsgBuf<<Endl;
				
				//! Decrementing the connection count and exiting	
				pOssUserInfo->DecrementConnectionCount();

				break;
			}
			if (isConnected == true)
                        {
                                lReturn = sendBytes(lReqMsgQueStructObj.xmlRequest);
				lResp.clear();
                        	lResp = recvResponse();				
                        }
			else
			{
				break;
			}

        	}

		lResponseLen = strlen(lResp.c_str());
		//sprintf(lRespMsgQueStructObj.xmlRequest,"s:%d:\"%s\";", lResponseLen, lResponse);	

		//! If the response received is SUCCESS, then the message to be pushed into the queue should be in the format s:7:"SUCCESS".
		//! This is because, the Service Layer written in PHP has got a different serialization protocol. The ideal message format is s:<msg len>:"<message>"
		memset(lRespMsgQueStructObj.xmlRequest, '\0', 4096);
		sprintf(lRespMsgQueStructObj.xmlRequest,"s:%d:\"%s\";", lResponseLen, lResp.c_str());	
		lRespMsgQueStructObj.mType = lReqMsgQueStructObj.mType;

		//!< Pushing the message to the response queue
		pOssUserInfo->PushMessage(lRespMsgQueStructObj);

	}

	//! In case of failure or system shut down, logout from SPS and close the socket
	logout();
	close(_socketDesc);
	std::cout << "############# XMLIA CLient Thread Exiting ###############" << threadID <<std::endl;
	isConnected = false;
	pthread_exit(NULL);
}//void XMLIAClient::startProcess()



/**
 * @fn Start
 * @param Nil
 * @ret return 0 on success and -1 on failure
 * @brief This is the public member function and main entry point to the XMLIAClient. 
 */
int XMLIAClient::Start()
{
	int 	lReturn;		//!< Used to hold the return values from called function

	
	//! Establishing connection to SPS
	lReturn = establishSPSConnection();
	//! If connection fails, return -1 to the called function
	if (-1 == lReturn)
	{
                gABLLoggerObj<<_ERROR<<"Unable to Establish Connection with SPS Return from XMLIAClient::Start()"<<Endl;
		return -1;
	}
	
	// Incrementing the connection count
	pOssUserInfo->IncrementConnectionCount();
	
	//! Acquiring the moveforward semaphore. This semaphore will be released once startProcess thread is started
	moveForwardSem.mb_acquire();

	//! Creating a thread to serve the request for the user
	ExecuteInNewThread0(&threadID, NULL, XMLIAClient, this, void, &XMLIAClient::startProcess);
	
	//! Storing the object pointer to the XMLIAClientVector
	XMLIAClient::xmliaClientVec.push_back(this);
	moveForwardSem.mb_acquire();

	return 0;
	
}//int XMLIAClient::Start()

XMLIAClient::XMLIAClient(OSSUserInfo *pObj)
{
	pOssUserInfo = pObj;
	_isStopSignalReceived = false;
}

