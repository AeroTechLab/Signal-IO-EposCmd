////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (c) 2019 Leonardo Consoni <leonardjc@protonmail.com>            //
//                                                                            //
//  This file is part of Signal-IO-EposCmd.                                   //
//                                                                            //
//  Signal-IO-EposCmd is free software: you can redistribute it and/or modify //
//  it under the terms of the GNU Lesser General Public License as published  //
//  by the Free Software Foundation, either version 3 of the License, or      //
//  (at your option) any later version.                                       //
//                                                                            //
//  Signal-IO-EposCmd is distributed in the hope that it will be useful,      //
//  but WITHOUT ANY WARRANTY; without even the implied warranty of            //
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the              //
//  GNU Lesser General Public License for more details.                       //
//                                                                            //
//  You should have received a copy of the GNU Lesser General Public License  //
//  along with Signal-IO-EposCmd. If not, see <http://www.gnu.org/licenses/>. //
//                                                                            //
//////////////////////////////////////////////////////////////////////////////// 

#include "interface/signal_io.h"

#include "epos/Definitions.h"

#include <stdio.h>
#include <string.h>

#include <list>
#include <thread>

#define ERROR_STRING_MAX_SIZE 128

typedef void* HANDLE;
typedef unsigned short WORD;
typedef unsigned int DWORD;
typedef int BOOL;

typedef struct DeviceData
{
  HANDLE handle;
  WORD nodeId;
  double inputValues[ 3 ];
  double outputValues[ 3 ];
  BOOL readStatus, writeStatus;
  DWORD readErrorCode, writeErrorCode;
}
DeviceData;

std::thread readingThread;
std::list<DeviceData*> runningDevices;
volatile bool isRunning = false;

void PrintError( DWORD errorCode )
{
  char errorInfo[ ERROR_STRING_MAX_SIZE ];
  VCS_GetErrorInfo( errorCode, errorInfo, ERROR_STRING_MAX_SIZE );
  fprintf( stderr, "error: %s\n", errorInfo );
}

static void AsyncTransfer( void );


DECLARE_MODULE_INTERFACE( SIGNAL_IO_INTERFACE );

// String on the form "<device>:<protocol>:<interface>:<port>:<node_id>:<baudrate>"
// Configuration Options:
// -Devices: EPOS, EPOS2, EPOS4
// -Protocols: MAXON_RS232, MAXON SERIAL V2, CANopen
// -Interfaces: RS232, USB, IXXAT_*, Kvaser_*, NI_*, Vector_*
// -Ports: COM1, COM2, ... USB0, USB1, ... CAN0, CAN1, ...
// -Node IDs: 1, 2, 3, 4, ...
// -Baudrates: Interface dependent
long int InitDevice( const char* configuration )
{  
  char* deviceName = strtok( (char*) configuration, ":" );
  char* protocolName = strtok( NULL, ":" );
  char* interfaceName = strtok( NULL, ":" );
  char* portName = strtok( NULL, ":" );
  unsigned short nodeId = (unsigned short) strtoul( strtok( NULL, ":" ), NULL, 0 );
  unsigned int baudrate = (unsigned int) strtoul( strtok( NULL, ":" ), NULL, 0 );
  
  DWORD errorCode;
  HANDLE deviceHandle = VCS_OpenDevice( deviceName, protocolName, interfaceName, portName, &errorCode );
  if( deviceHandle == NULL ) 
  {
    PrintError( errorCode );
    return SIGNAL_IO_DEVICE_INVALID_ID;
  }
  
  unsigned int timeout;
  unsigned int defaultBaudrate;
  if( VCS_GetProtocolStackSettings( deviceHandle, &defaultBaudrate, &timeout, &errorCode ) != 0 )
  {
    if( VCS_SetProtocolStackSettings( deviceHandle, baudrate, timeout, &errorCode ) == 0 )
    {
      PrintError( errorCode );
      return SIGNAL_IO_DEVICE_INVALID_ID;
    }
  }
  
  DeviceData* newDevice = new DeviceData;
  newDevice->handle = deviceHandle;
  newDevice->nodeId = nodeId;
  
  if( runningDevices.empty() ) readingThread = std::thread( AsyncTransfer );
  runningDevices.push_back( newDevice );
  
  return (long int) newDevice;
}

void EndDevice( long int deviceID )
{
  if( deviceID == SIGNAL_IO_DEVICE_INVALID_ID ) return;
  
  DeviceData* device = (DeviceData*) deviceID;
  
  DWORD errorCode;
  if( VCS_CloseDevice( device->handle, &errorCode ) == 0 ) 
    PrintError( errorCode );
  
  runningDevices.remove( device );
  
  if( runningDevices.empty() )
  {
    isRunning = false;
    readingThread.join();
  }
  
  delete device;
  
  return;
}

size_t GetMaxInputSamplesNumber( long int deviceID )
{
  return 1;
}

size_t Read( long int deviceID, unsigned int channel, double* ref_value )
{
  *ref_value = 0.0;
  
  if( deviceID == SIGNAL_IO_DEVICE_INVALID_ID ) return 0;
  
  DeviceData* device = (DeviceData*) deviceID;
  
  if( device->readStatus == 0 ) 
  {
    PrintError( device->readErrorCode );
    return 0;
  }
  
  *ref_value = device->inputValues[ channel ];
  
  return 1;
}

bool HasError( long int deviceID )
{
  if( deviceID == SIGNAL_IO_DEVICE_INVALID_ID ) return true;

  DeviceData* device = (DeviceData*) deviceID;
  
  WORD state;
  DWORD errorCode;
  if( VCS_GetState( device->handle, device->nodeId, &state, &errorCode ) == 0 )
    PrintError( errorCode );
  
  if( state == ST_FAULT ) return true;
  
  return false;
}

void Reset( long int deviceID )
{
  if( deviceID == SIGNAL_IO_DEVICE_INVALID_ID ) return;

  DeviceData* device = (DeviceData*) deviceID;
  
  DWORD errorCode;
  if( VCS_ClearFault( device->handle, device->nodeId, &errorCode ) == 0 )
    PrintError( errorCode );

  return;
}

bool CheckInputChannel( long int deviceID, unsigned int channel )
{
  if( deviceID == SIGNAL_IO_DEVICE_INVALID_ID ) return false;
  
  if( channel > 2 ) return false;
  
  return true;
}

bool Write( long int deviceID, unsigned int channel, double value )
{
  if( deviceID == SIGNAL_IO_DEVICE_INVALID_ID ) return false;
  
  DeviceData* device = (DeviceData*) deviceID;
  
  //device->outputValues[ channel ] = value;

  BOOL status = 0;
  DWORD errorCode;
  if( channel == 0 ) status = VCS_SetPositionMust( device->handle, device->nodeId, (long) value, &errorCode );
  else if( channel == 1 ) status = VCS_SetVelocityMust( device->handle, device->nodeId, (long) value, &errorCode );
  else if( channel == 2 ) status = VCS_SetCurrentMust( device->handle, device->nodeId, (short) value, &errorCode );

  //if( device->writeStatus == 0 ) 
  if( status == 0 )
  {
    //PrintError( device->writeErrorCode );
    PrintError( errorCode );
    return false;
  }
  
  return true;
}

bool AcquireOutputChannel( long int deviceID, unsigned int channel )
{
  if( deviceID == SIGNAL_IO_DEVICE_INVALID_ID ) return false;
  
  if( channel > 2 ) return false;
  
  DeviceData* device = (DeviceData*) deviceID;

  short mode = OMD_POSITION_MODE;
  if( channel == 1 ) mode = OMD_VELOCITY_MODE;
  if( channel == 2 ) mode = OMD_CURRENT_MODE;
  
  DWORD errorCode;
  if( VCS_SetEnableState( device->handle, device->nodeId, &errorCode ) == 0 )
    PrintError( errorCode );
  if( VCS_SetOperationMode( device->handle, device->nodeId, mode, &errorCode ) == 0 )
    PrintError( errorCode );

  return true;
}

void ReleaseOutputChannel( long int deviceID, unsigned int channel )
{
  if( deviceID == SIGNAL_IO_DEVICE_INVALID_ID ) return;

  if( channel > 2 ) return;

  DeviceData* device = (DeviceData*) deviceID;

  DWORD errorCode;
  if( VCS_SetDisableState( device->handle, device->nodeId, &errorCode ) == 0 )
    PrintError( errorCode );
  
  return;
} 

static void AsyncTransfer( void )
{  
  int iValue = 0;
  short sValue = 0;
  
  isRunning = true;
  
  while( isRunning )
  {
    for( DeviceData* device : runningDevices )
    {
      device->readStatus = VCS_GetPositionIs( device->handle, device->nodeId, &iValue, &(device->readErrorCode) );
      device->inputValues[ 0 ] = (double) iValue;
      device->readStatus = VCS_GetVelocityIs( device->handle, device->nodeId, &iValue, &(device->readErrorCode) );
      device->inputValues[ 1 ] = (double) iValue;
      device->readStatus = VCS_GetCurrentIsAveraged( device->handle, device->nodeId, &sValue, &(device->readErrorCode) );
      device->inputValues[ 2 ] = (double) sValue;
      
      if( device->readStatus == 0 ) VCS_ClearFault( device->handle, device->nodeId, &(device->readErrorCode) );
      
      //iValue = (long) device->outputValues[ 0 ];
      //device->writeStatus = VCS_SetPositionMust( device->handle, device->nodeId, iValue, &(device->writeErrorCode) );
      //iValue = (long) device->outputValues[ 1 ];
      //device->writeStatus = VCS_SetVelocityMust( device->handle, device->nodeId, iValue, &(device->writeErrorCode) );
      //sValue = (short) device->outputValues[ 2 ];
      //device->writeStatus = VCS_SetCurrentMust( device->handle, device->nodeId, sValue, &(device->writeErrorCode) );
    }
  }
  
  return;
}
