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

#define ERROR_STRING_MAX_SIZE 128

typedef void* HANDLE;
typedef unsigned short WORD;
typedef unsigned int DWORD;
typedef int BOOL;

typedef struct DeviceData
{
  HANDLE handle;
  WORD nodeId;
}
DeviceData;

HANDLE VCS_OpenDevice( char*, char*, char*, char*, DWORD* );
BOOL VCS_CloseDevice( HANDLE, DWORD* );
BOOL VCS_GetProtocolStackSettings( HANDLE, DWORD*, DWORD*, DWORD* );
BOOL VCS_SetProtocolStackSettings( HANDLE, DWORD, DWORD, DWORD* );
BOOL VCS_GetPositionIs( HANDLE, WORD, long* , DWORD* );
BOOL VCS_GetVelocityIs( HANDLE, WORD, long*, DWORD* );
BOOL VCS_GetVelocityIsAveraged( HANDLE, WORD, long*, DWORD* );
BOOL VCS_GetCurrentIs( HANDLE, WORD, short*, DWORD* );
BOOL VCS_GetCurrentIsAveraged( HANDLE, WORD, short*, DWORD* );
BOOL VCS_ActivatePositionMode( HANDLE, WORD, DWORD* );
BOOL VCS_SetPositionMust( HANDLE, WORD, long, DWORD* );
BOOL VCS_ActivateVelocityMode( HANDLE, WORD, DWORD* );
BOOL VCS_SetVelocityMust( HANDLE, WORD, long, DWORD* );
BOOL VCS_ActivateCurrentMode( HANDLE, WORD, DWORD* );
BOOL VCS_SetCurrentMust( HANDLE, WORD, short, DWORD* );
BOOL VCS_GetErrorInfo( DWORD, char*, WORD );

void PrintError( DWORD errorCode )
{
  char errorInfo[ ERROR_STRING_MAX_SIZE ];
  VCS_GetErrorInfo( errorCode, errorInfo, ERROR_STRING_MAX_SIZE );
  fprintf( stderr, "error: %s", errorInfo );
}


DECLARE_MODULE_INTERFACE( SIGNAL_IO_INTERFACE );

long int InitDevice( const char* configuration )
{  
  char* deviceName = strtok( (char*) configuration, " " );
  char* protocolName = strtok( NULL, " " );
  char* interfaceName = strtok( NULL, " " );
  char* portName = strtok( NULL, " " );
  unsigned short nodeId = (unsigned short) strtoul( strtok( NULL, " " ), NULL, 0 );
  
  DWORD errorCode;
  HANDLE deviceHandle = VCS_OpenDevice( deviceName, protocolName, interfaceName, portName, &errorCode );
  if( deviceHandle == NULL ) 
  {
    PrintError( errorCode );
    return SIGNAL_IO_DEVICE_INVALID_ID;
  }
  
  unsigned int timeout;
  unsigned int baudrate;
  if( VCS_GetProtocolStackSettings( deviceHandle, &baudrate, &timeout, &errorCode ) != 0 )
  {
    baudrate = (unsigned int) strtoul( strtok( NULL, " " ), NULL, 0 );
    if( VCS_SetProtocolStackSettings( deviceHandle, baudrate, timeout, &errorCode ) == 0 )
    {
      PrintError( errorCode );
      return SIGNAL_IO_DEVICE_INVALID_ID;
    }
  }
  
  DeviceData* newDevice = (DeviceData*) malloc( sizeof(DeviceData) );
  newDevice->handle = deviceHandle;
  newDevice->nodeId = nodeId;
  
  return (long int) newDevice;
}

void EndDevice( long int deviceID )
{
  if( deviceID == SIGNAL_IO_DEVICE_INVALID_ID ) return;
  
  DeviceData* device = (DeviceData*) deviceID;
  
  DWORD errorCode;
  if( VCS_CloseDevice( device->handle, &errorCode ) != 0 ) 
    PrintError( errorCode );
  
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
  
  long value;
  DWORD errorCode;
  if( channel == 0 ) VCS_GetPositionIs( device->handle, device->nodeId, &value, &errorCode );
  else if( channel == 1 ) VCS_GetVelocityIs( device->handle, device->nodeId, &value, &errorCode );
  else if( channel == 2 ) VCS_GetCurrentIsAveraged( device->handle, device->nodeId, (short*) &value, &errorCode );
  
  *ref_value = (double) value;
  
  return 1;
}

bool HasError( long int deviceID )
{
  return false;
}

void Reset( long int deviceID )
{
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
  
  DWORD errorCode;
  if( channel == 0 ) VCS_SetPositionMust( device->handle, device->nodeId, (long) value, &errorCode );
  else if( channel == 1 ) VCS_SetVelocityMust( device->handle, device->nodeId, (long) value, &errorCode );
  else if( channel == 2 ) VCS_SetCurrentMust( device->handle, device->nodeId, (short) value, &errorCode );
  
  return true;
}

bool AcquireOutputChannel( long int deviceID, unsigned int channel )
{
  if( deviceID == SIGNAL_IO_DEVICE_INVALID_ID ) return false;
  
  if( channel > 2 ) return false;
  
  return true;
}

void ReleaseOutputChannel( long int deviceID, unsigned int channel )
{
  return;
} 
