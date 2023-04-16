/**
    @file Main.cpp
    @brief The Main entry point for the Session Layer component of SPS
    @author Anoop Viswambharan
	
	This file contains the main for the Session Layer process. All the signal handlings and SessionLayer object creation are present in this.
*/


#include <string.h>
#include <signal.h>
#include <XMLIAClient.h>
#include <SessionLayer.h>

using namespace std;
using namespace SPS;

SPSServerInfoVector 	XMLIAClient::spsSerInfoVec;		//!< Forward Declaration of static SPSServerInfoVector
XMLIAClientVector 		XMLIAClient::xmliaClientVec;	//!< Forward Declaration of static XMLIAClientVector

char 				GProcessStopCheckFileName[1024];	//!< Global variable which holds the filename which indicates successful stop of SessionLayerobj.
char 				GSessionStopfileName[1024];			//!< Global variable which holds the stop file name required to stop the SessionLayer.
char				GLogFileName[1024];                 //!< Global variable used to store the Log File Name.

ABL_Logger          		gABLLoggerObj;                      //!< Global ABL Logger Object for logging

	
extern "C"
{
	/**
	 * @fn sigint_handler
	 * @param Integer indicating the type of the signal
	 * @brief This will be invoked when any signals are captured. 
	 *
	 *	The task of this function is to create the stop signal for the graceful shut down of the SessionLayer. When any signals are captured, this function 
	 *	create the touch file required to stop the SessionLayer. Upon successfull completion of the task by the SessionLayer, it offers a smooth shut down.
	 */
	void sigint_handler(int sig)
	{
		char 		lTouchCmd[2048];	//!< Used to hold the touch command used to create the stop file
		struct stat stFileInfo;			//!< Structure used to hold the status of a file used with the stat function.

		//! Checking the type of the signal
		switch (sig)
    	{
        	case SIGABRT:
                std::cout << "Process Abort signal received" << std::endl;
		gABLLoggerObj<<_ERROR<<"Process Abort signal received"<<Endl;
                break;
        	case SIGALRM:
		gABLLoggerObj<<_ERROR<<"Alarm Received"<<Endl;
                break;
	        case SIGINT:
		gABLLoggerObj<<_ERROR<<"Process interrupt signal received"<<Endl;
                break;
    	    	case SIGTERM:
		gABLLoggerObj<<_ERROR<<"Process termination signal received"<<Endl;
                break;
        	case SIGSEGV:
		gABLLoggerObj<<_ERROR<<"Segmentation Fault Encountered"<<Endl;
                break;
			case SIGUSR1:
				gABLLoggerObj<<CRITICAL<<"Stop signal received due to connection error with SPS"<<Endl;
				break;
    	}

		//! Preparing the touch command used to create a touch file in the SESSION_HOME_PATH directory. The name of the stop file is passed as an argument to 		 //! to the Main and its copied to the GSessionStopfileName variable.
		strcpy(lTouchCmd, "touch ");
		strcat(lTouchCmd, GSessionStopfileName);

		//! Executing the system function to create the touch file
		system(lTouchCmd);

		//! Entering the lop for checking the existence of the process stop check touch file. When the stop signals are received by the Session Layer,
		//! Session Layer closes all the TCP connections with the SPS. Once all the connections are closed, Session Layer indicates the successfull shut down		 //! to the Main through the  process stop check touch file
		while( 0 != stat(GProcessStopCheckFileName, &stFileInfo))
		{
			//! Sleep and continue untill. the notification arrives from the Session Layer
			sleep(1);
		}

		//! Removing the  process stop check touch file after successfull stopping of the Session Layer
		remove(GProcessStopCheckFileName);
		gABLLoggerObj<<INFO<<"Removed the Stop Signal File"<<Endl;
		gABLLoggerObj<<INFO<<"Log File Closed"<<Endl;
        	gABLLoggerObj<<INFO<<"****************************************"<<Endl;

		exit(0);
	}//void sigint_handler(int sig)
}



/**
 * @fn main
 * @param Integer indicating the number of arguments passed from command line
 * @param Pointer to a character array which stores all the arguments passed to the main
 * @brief Main function which creates an object of the SessionLayer and registers the signals to be handled in the process.
 */
int main(int argc, char* argv[])
{
	char 	*lTemp;				//! Character pointer which stores the Session Layer Home path retrieved from the env variable
	char 	lDBConfFile[1024];	//! Used to store the Db configuration file name.
	int 	lReturn;			//!< Used to hold the return values during function calls.
	char 	lLogMsgBuf[512];		//!< Logger Message Buffer
	
	//! Checking for the number of arguments passed. If its not equal to 3, then exit.
	//! The arguments to be passed are the <stop file name> and the <service layer indicator file name>.
	//! The <service layer indicator file name> should be configured in Config.php of the ServiceLayer also
	if (2 != argc)
	{
		std::cout << "Invalid Number of Command Line Arguments" << std::endl;
		std::cout << "Usage : <program name> <stop file name>" << std::endl;
		return -1;
	}

	//! Getting the home directory of the Session Layer. The home directory should be set in the SESSION_LAYER_HOME path before starting the application.
	lTemp = getenv("SESSION_LAYER_HOME");

	//! If SESSION_LAYER_HOME is not set, exit
	if (NULL == lTemp)
	{
		std::cout << "SESSION_LAYER_HOME env variable not set" << std::endl;
		return -1;
	}

	//! Copying the complete path of the stop file.
	strcpy(GProcessStopCheckFileName, lTemp);
	strcat(GProcessStopCheckFileName, "/");
	strcpy(GSessionStopfileName, GProcessStopCheckFileName);
	
	strcpy(GLogFileName, lTemp);
    	strcat(GLogFileName, "/Logs/SessionLog");
	std::cout << "Log file name : " << GLogFileName << std::endl;
	
	gABLLoggerObj.mb_initLogger(GLogFileName, 0, "TAR", LOG_FILE_SIZE, LOG_FILE_SEQUENCE, true);
	gABLLoggerObj<<INFO<<"Log File Opened Successfully"<<Endl;
	gABLLoggerObj<<INFO<<"----------------------------"<<Endl;

	//! The GProcessStopCheckFileName file will be created by the Session Layer once it is exiting. 
	//! This is required by the signal handler process to wait untill Session Layer completes its task.
	strcat(GProcessStopCheckFileName, "SessStoppedIndi");
	
	strcat(GSessionStopfileName, argv[1]);

	//! The below if conditions handles the respective type of signals.	
	if (signal(SIGSEGV, sigint_handler) == SIG_ERR)
    {
        std::cout << "SIG SEGV not set" << std::endl;
        return -1;
    }
    if (signal(SIGTERM, sigint_handler) == SIG_ERR)
    {
        std::cout << "SIGTERM not set" << std::endl;
        return -1;
    }
    if (signal(SIGINT, sigint_handler) == SIG_ERR)
    {
        std::cout << "SIG SEGV not set" << std::endl;
        return -1;
    }
    if (signal(SIGABRT, sigint_handler) == SIG_ERR)
    {
        std::cout << "SIG SEGV not set" << std::endl;
        return -1;
    }
    if (signal(SIGALRM, sigint_handler) == SIG_ERR)
    {
        std::cout << "SIG ALRM not set" << std::endl;
        return -1;
    }
    if (signal(SIGUSR1, sigint_handler) == SIG_ERR)
    {
        std::cout << "SIG ALRM not set" << std::endl;
        return -1;
    }
	
	//! Creating the object of SessionLayer. Arguments passed are <stop file name>, <service layer indicator file name> and <home path>
	SessionLayer lSesLayerObj(argv[1], lTemp);

	//! Creating the DB Configuration file name. The database configuration file name should be db.conf and it should be present in the Conf directory under the SESSION_LAYER_HOME path
	strcpy(lDBConfFile, lTemp);
	strcat(lDBConfFile, "/Conf/db.conf");
	
	memset(lLogMsgBuf, '\0', sizeof(lLogMsgBuf));
	sprintf(lLogMsgBuf, "Configuration File : %s", lDBConfFile);
	gABLLoggerObj<<INFO<<lLogMsgBuf<<Endl;

	//! Establishing connection to the database.	
	try
	{	
		lReturn = lSesLayerObj.EstablishDBConnection(lDBConfFile);
	}
	catch(ABL_Exception)
	{
		gABLLoggerObj<<INFO<<"Unable to establish connection to the Database: Please check the db.conf file in the Conf Directory"<<Endl;
		gABLLoggerObj<<INFO<<"Log File Closed"<<Endl;
		gABLLoggerObj<<INFO<<"****************************************"<<Endl;
		return -1;
	}
	if (0 != lReturn)
	{
		gABLLoggerObj<<INFO<<"Unable to establish connection to the Database: Please check the db.conf file in the Conf Directory"<<Endl;
		gABLLoggerObj<<INFO<<"Log File Closed"<<Endl;
		gABLLoggerObj<<INFO<<"****************************************"<<Endl;
		return -1;
	}

	//! Calling the Start process of the SessionLayer
	gABLLoggerObj<<DEBUG<<"Starting the Session Layer"<<Endl;
	lReturn = lSesLayerObj.Start();
	
	if (0 == lReturn)
	{
		//! Waiting for the SessionLayer thread to exit
		pthread_join(lSesLayerObj.threadID, NULL);
	}
	
	//! Removing the Process Stop Checking File which will be created by the SessionLayer during exit
	remove(GProcessStopCheckFileName);
	gABLLoggerObj<<INFO<<"Removed the Stop Signal File"<<Endl;
	gABLLoggerObj<<INFO<<"Log File Closed"<<Endl;
        gABLLoggerObj<<INFO<<"****************************************"<<Endl;
	return 0;
}//int main(int argc, char* argv[])
