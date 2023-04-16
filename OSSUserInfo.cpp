/**
    @file OSSUserInfo.cpp
    @brief This file contains the definition for all the member functions of the OSSUserInfo
    @author Anoop Viswambharan

*/

#include <OSSUserInfo.h>
#include <XMLIAClient.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
using namespace SPS;

extern ABL_Logger gABLLoggerObj;    //!< Global ABL Logger Object for logging

/**
 * @fn CreateQueues
 * @param Nil
 * @ret returns 0 on success and -1 on failure
 * @brief This member function is used to create the Request and Response message queue for the individual user
 */
int OSSUserInfo::CreateQueues()
{
	//! Getting the Message Queue ID for the Request Queue. If message queue is not present, the system call will create a message queue and return its id.
	_requestMsgQueueId = msgget(requestQueueKey, 0666 | IPC_CREAT);

	//! If there is any error in creating the request queue, return -1
	if (_requestMsgQueueId < 0)
	{
		memset(_logMsgBuf, '\0', sizeof(_logMsgBuf));
		sprintf(_logMsgBuf, "Unable to Create the request Message Queue for User : %s ", userName );
		gABLLoggerObj<<_ERROR<<_logMsgBuf<<Endl;
		return -1;
	}
	
	//! Getting the Message Queue ID for the Response Queue. If message queue is not present, the system call will create a message queue and return its id.
	_responseMsgQueueId = msgget(responseQueueKey, 0666 | IPC_CREAT);
	
	//! If there is any error in creating the request queue, return -1
	if (_responseMsgQueueId < 0)
	{
		memset(_logMsgBuf, '\0', sizeof(_logMsgBuf));
		sprintf(_logMsgBuf, "Unable to Create the Response Message Queue for User : %s ", userName );
		gABLLoggerObj<<_ERROR<<_logMsgBuf<<Endl;
		return -1;
	}
}//int OSSUserInfo::CreateQueues()



/**
 * @fn DecrementConnectionCount
 * @param Nil
 * @ret void
 * @brief This member function is used to decrement the connection count to the SPS for the user. 
 */
void OSSUserInfo::DecrementConnectionCount()
{
	int 		lDifferenceInConn;		//!< Used to hold the difference between the max connection for the user and the current connectionc count
	int 		lIndex;					//!< Used as index in loop
	int 		lReturn;				//!< Used to hold the return value on fucntion call
	char 		ltouchStopCmd[1024];	//!< Character buffer to hold the touch command to create the stop file 

	//! Decrementing the connection count
	std::cout << "Decrementing the Connection Count "<< std::endl;
	_connectionSem.mb_acquire();
	--_currentConnCount;
	_connectionSem.mb_release();

	//! If stop signal is not received and the current connection count is less than the max connection, then spawn new XMLIA Client to maintain the max number of connection
	if (!_isStopSigReceived && _currentConnCount < maxConnection)
	{
		//! Checking the difference between the max connection and current connection
		std::cout << "Stop Signal Not Received"<< std::endl;
		std::cout << "Current connection count : " << _currentConnCount << " Maximum connection : "<< maxConnection << std::endl;
		lDifferenceInConn = maxConnection - _currentConnCount;

		XMLIAClient *lptrXMLIAClientObj;
	
		//! Creating those many XMLIAClient objects to maintain the max connections
		for (lIndex = 0; lIndex < lDifferenceInConn ; lIndex++ )
		{
			lptrXMLIAClientObj = new XMLIAClient(this);

			//! Calling the Start method on XMLIAClient to start the XMLIAClient
			lReturn = lptrXMLIAClientObj->Start();	
			
			//! If XMLIAClient is started successfully, then increase the current connection count		
			if (0 == lReturn)
			{
				isConnected = true;
				this->IncrementConnectionCount();
				std::cout << "Incremented the connection : Current Connection count : "<< _currentConnCount << std::endl;
				
			}
		}
	}

	//! If no further connection can be established to SPS and if the current connection count for the user is zero,then create the stop file to stop the complete process
	if (0 == _currentConnCount && !_isStopSigReceived)
	{
		strcpy(ltouchStopCmd, "touch ");
		strcat(ltouchStopCmd, _stopFileName);
		system(ltouchStopCmd);
		remove(touchFileName);
	}


}//void OSSUserInfo::DecrementConnectionCount()



/**
 * @fn IncrementConnectionCount
 * @param Nil
 * @ret void
 * @brief This member function is used to increment the connection count when a connection is successfully established for the user.
 */
void OSSUserInfo::IncrementConnectionCount()
{
	char lTouchFileCmd[TOUCH_FILE_NAME_LENGTH];
	//! Acquiring the connection semaphore and then incrementing the connection count 
	_connectionSem.mb_acquire();
	_currentConnCount++;
	_connectionSem.mb_release();
	strcpy(lTouchFileCmd, "touch ");
	strcat(lTouchFileCmd, touchFileName);
	system(lTouchFileCmd);
}//!void OSSUserInfo::IncrementConnectionCount()



/**
 * @fn StopUserConnections
 * @param Nil
 * @ret true
 * @brief This member function is used to send the stop messages in the Request Message queue for shutting down the system.
 */
bool OSSUserInfo::StopUserConnections()
{
	int lIndex;		//!< Local index used in loops

	MsqQueStruct lMsgQueStrObj;	//!< Message Queue object to send the stop messages
	
	remove(touchFileName);	
	lMsgQueStrObj.mType = 123123;	//!< Setting the mType of the request to 123123, hardcoded and understood by the XMLIAClient
	_isStopSigReceived = true;
    strcpy(lMsgQueStrObj.xmlRequest, "STOP");

	//! Acquiring the connection count semaphore, else there might be a scenario where the connection count will be decreased by the first XMLIAClient thread which get the stop message and this might give run time logical error in the below for loop
	_connectionSem.mb_acquire();

	memset(_logMsgBuf, '\0', sizeof(_logMsgBuf));
        sprintf(_logMsgBuf, "Current Number of Connections for User : %s is %d", userName,_currentConnCount );
        gABLLoggerObj<<INFO<<_logMsgBuf<<Endl;

	//! Sending the stop messages to those many active connections
	if (0 != _currentConnCount )
    	{
		for (lIndex = 0; lIndex < _currentConnCount ; lIndex++)
    		{
        		msgsnd(_requestMsgQueueId, &lMsgQueStrObj, sizeof(lMsgQueStrObj), 0);
    		}
	}
	for (lIndex = 0; lIndex < XMLIAClient::xmliaClientVec.size(); lIndex++)
    	{

		// Checking if the XMLIAClient object is created by, this object. If yes set, the _isStopSignalReceived flag of the XMLIAClient to true.
		// This is required, when the XMLIAClient object gets into an infinite loop to connect to SPS, in case of connectivity error.
		if (this == XMLIAClient::xmliaClientVec[lIndex]->pOssUserInfo)
		{
			XMLIAClient::xmliaClientVec[lIndex]->_isStopSignalReceived = true;
		}
    	}
	//! Releasing the connection count semaphore
	_connectionSem.mb_release();
    std::cout << "Stop Signals send to XMLIA Client Objects" << std::endl;
        gABLLoggerObj<<INFO<<"Stop Signals send to XMLIA Clients"<<Endl;

	//! Releasing the stop now semaphore
    stopNowSemaphore.mb_release();
}//bool OSSUserInfo::StopUserConnections()



/**
 * @fn CreateSPSConnections
 * @param Nil
 * @ret return 0 on success and -1 on failure
 * @brief This member function is used to create XMLIAClient objects to establish connection to SPS
 */
int OSSUserInfo::CreateSPSConnections()
{
	int 	lIndex;							//!< Used as index in loops
	int 	lReturn;						//!< Used to hold the return values on function call
	
	XMLIAClient *lptrXMLIAClientObj;		//!< Used to hold the XMLIAClient objects

	//! Looping through the max number of user connections configured and creating those many XMLIAClient objects
	for (lIndex = 0; lIndex < maxConnection; lIndex++)
	{
		std::cout << "Creating a new XML IA client for User Name : " << userName << " | Thread Number " << lIndex << std::endl;
		lptrXMLIAClientObj = new XMLIAClient(this);

		//! Invoking the Start method on the XMLIAClient object
		lReturn = lptrXMLIAClientObj->Start();

		//! If there is atleast one active connection for the user, then setting the isConnected to true	
		if (0 == lReturn)
		{
			isConnected  = true;
		}
		else
		{
			delete lptrXMLIAClientObj;
		}
		
		//! Inducing a delay of one second while creating the new XMLIAClient objects for proper synchronization	
		sleep (1);
	}

	//! If there is not even a single active connection for the user, return a failure to the calling fucntion
	if (!(this->isConnected))
	{
		return -1;
	}

	//! Once all connections are made, acquire the stop now semaphore and return 0 to the calling fucntion	
	stopNowSemaphore.mb_acquire();
	return 0;

}//int OSSUserInfo::CreateSPSConnections()



/**
 * @fn OSSUserInfo
 * @param character pointer to the stop file name
 * @brief Constructor of the OSSUserInfo used to intialize the member variables
 */
OSSUserInfo::OSSUserInfo(char *pStopFile)
{
	strcpy(_stopFileName, pStopFile);
	_currentConnCount = 0;
	_isStopSigReceived = false;
}



/**
 * @fn ~OSSUserInfo
 * @param Nil
 * @brief Destructor of the OSSUserInfo. Once destructor is invoked, the request and response message queues will be removed from the server
 */
OSSUserInfo::~OSSUserInfo()
{
	msgctl(_requestMsgQueueId, IPC_RMID, NULL);
	msgctl(_responseMsgQueueId, IPC_RMID, NULL);
}//OSSUserInfo::~OSSUserInfo()

