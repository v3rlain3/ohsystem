/*
*
* We spent a lot of time writing this code, so show some respect:
* - Do not remove this copyright notice anywhere (bot, website etc.)
* - We do not provide support to those who removed copyright notice
*
* OHSystem is free software: You can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* You can contact the developers on: admin@ohsystem.net
* or join us directly here: http://forum.ohsystem.net/
*
* Visit us also on http://ohsystem.net/ and keep track always of the latest
* features and changes.
*
*
* This is modified from GHOST++: http://ghostplusplus.googlecode.com/
*/

using namespace std;

#include "ghost.h"
#include "socket.h"
#include "ohconnect.h"
#include "util.h"
#include "commandpacket.h"
#include "game_base.h"

#ifdef WIN32
 #include <cstdint>
#endif

//
// OHConnect
//
OHConnect :: OHConnect( CGHost *nGHost, CBaseGame *nGame, string nIP, uint32_t nPort ) : m_GHost( nGHost ), m_Game(nGame), m_Connected( false ), IP( nIP ), Port( nPort ), m_FirstConnect(false), m_LastSendTime(0), m_Socket(new CTCPClient( )), m_Handshake(false), m_ClientID(0), LastPingTime( GetTime() ), m_Room("1"), m_RoomName("OHC ROOM 1"), LastConnectionAttemp( 0 )
{

}
OHConnect :: ~OHConnect( )
{
  delete m_Socket;
}

bool OHConnect :: Update( void *fd, void *send_fd )
{
  if( m_Socket->HasError( ) )
  {
    CONSOLE_Print("[OHConnect] Socket error, reseting socket");
    m_Socket->Reset( );
    m_Handshake = false;
  }

  if( !m_Socket->GetConnecting( ) && !m_Socket->GetConnected( ) )
  {
     CONSOLE_Print("[OHConnect] Socket is not connected, reseting socket");
     m_Socket->Reset( );
     m_Handshake = false;
  }

  if( m_Socket->GetConnected( ))
  {
    if(GetTime( ) - LastPingTime > 15 || LastPingTime == 0 ) {
      sendData(OHCHeader::PONG, string());
      LastPingTime = GetTime( );
    }
    m_Socket->DoRecvPlain((fd_set *)fd);
    ExtractPackets( );  
    m_Socket->DoSendPlain( (fd_set *)send_fd);
  }

  if( m_Socket->GetConnecting( ) ) {
    if( m_Socket->CheckConnect( ) ) {
      CONSOLE_Print("[OHConnect] Successfully connected to socket");
      CONSOLE_Print("[OHConnect] Sending handshake");
string toSend="GET / HTTP/1.1\r\nHost: 5.45.181.151:6973\r\nUpgrade: websocket\r\nConnection: Keep-Alive, Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nOrigin: http://ohconnect.net\r\nSec-Websocket-Protocol: chat, superchat\r\nSec-WebSocket-Version: 13\r\n\r\n";
      m_Socket->PutBytes(toSend);
      m_Socket->DoSendPlain( (fd_set *)send_fd);
    }
  }

  if( !m_Socket->GetConnecting( ) && !m_Socket->GetConnected( ) && ( GetTime( ) - LastConnectionAttemp > 15 || LastConnectionAttemp == 0 ) ) {
    CONSOLE_Print("[OHConnect] Server isn't connected, init a new connection");
    Connect( );
    LastConnectionAttemp = GetTime( );
  }

  return true;
}

unsigned int OHConnect :: SetFD( void *fd, void *send_fd, int *nfds )
{
  unsigned int NumFDs = 0;

  if( !m_Socket->HasError( ) && m_Socket->GetConnected( ) ) {
    m_Socket->SetFD( (fd_set *)fd, (fd_set *)send_fd, nfds );
    ++NumFDs;
  }
  return NumFDs;
}
void OHConnect :: ProcessPackets( )
{

}

void OHConnect :: ExtractPackets( )
{
  string *RecvBuffer = m_Socket->GetBytes( );
  string Buffer = *RecvBuffer;
  // a packet is at least 4 bytes so loop as long as the buffer contains 4 bytes
  if( Buffer.size( ) > 0 )
  {
      uint32_t length = Buffer.size();
      if(length > 0 ) {
         string message = Buffer.substr(0, length);
         ProcessEvent(message);
         *RecvBuffer = RecvBuffer->substr(length);
      }
  }
}
bool invalidChar (char c) 
{  
    return !(c>=0 && c <128);   
} 

void OHConnect :: ProcessEvent( string msg ){
  msg.erase(remove_if(msg.begin(),msg.end(), invalidChar), msg.end());
  if(msg.find("Switching Protocols")!=std::string::npos) {
    CONSOLE_Print("[OHConnect] handshake complete");  
    joinRoom(string(), string());
    sendData(OHCHeader::PONG, std::string());
    m_Handshake = true;
    return;
  }
  size_t hasType = msg.find("type");
  size_t length = msg.size();
  /* has a type definition */
  size_t pop = msg.find("{");
  if(pop>length) {
    return;
  }
  msg = msg.substr(pop);
  string fullmsg = msg;
  size_t systemMessage = msg.find("\"type\":\"system\"");
  if(msg.find("<img")!=std::string::npos && systemMessage != 1) {
    CONSOLE_Print("[OHConnect] Recieved a packet with image tag, ignoring.");
    return;
  }

  if(hasType!=std::string::npos) {
    /* type */
    string date, name, message, color, room, roomname;
    int pos = msg.find("type");
    if(pos!=std::string::npos) {
      msg = msg.substr(pos+7);
      int pos2 = msg.find_first_of("\"");
      string type = msg.substr(0, pos2);
      msg = msg.substr(pos2);
      if(type != "id" ) {
        /* datetime */
        pos = msg.find("datetime");
        if(pos!=std::string::npos) {
          msg = msg.substr(pos+11);
          pos2 = msg.find_first_of("\"");
          date = msg.substr(0, pos2);
          msg = msg.substr(pos2);
        } else { date = "unknown"; }
        /* name */
        pos = msg.find("name");
        if(pos!=std::string::npos) {
          msg = msg.substr(pos+7);
          pos2 = msg.find_first_of("\"");
          name = msg.substr(0, pos2);
          msg = msg.substr(pos2);
        } else { name="unknown"; }
        /* message */
        pos = msg.find("message");
        if(pos!=std::string::npos) {
          msg = msg.substr(pos+10);
          pos2 = msg.find_first_of("\"");
          message = msg.substr(0, pos2);
          msg = msg.substr(pos2);
        } else { message = "empty"; }
        /* color */
        pos = msg.find("color");
        if(pos!=std::string::npos) {
          msg = msg.substr(pos+8);
          pos2 = msg.find_first_of("\"");
          color = msg.substr(0, pos2);
          msg = msg.substr(pos2);
        } else { color = "#000"; }
        /* room */
        pos = msg.find("room");
        if(pos!=std::string::npos) {
          msg = msg.substr(pos+7);
          pos2 = msg.find_first_of("\"");
          room = msg.substr(0, pos2);
          msg = msg.substr(pos2);
        } else { room ="1"; }
        /* roomname */
        pos = msg.find("roomname");
        if(pos!=std::string::npos) {
          msg = msg.substr(pos+11);
          pos2 = msg.find_first_of("\"");
          roomname = msg.substr(0, pos2);
          msg = msg.substr(pos2);
        } else { roomname = "OHC ROOM 1"; }
	if( type=="usermsg" && room == m_Room ) {
          CONSOLE_Print("[OHCONNECT: "+roomname+"] "+name+": "+message);

	  if(m_Game && name != "OHC Bot") {
            m_Game->SendAllChat("[OHConnect] "+name+": "+message);
          }
        }

        if(type=="system") {
          if(fullmsg.find("has joined") && room == m_Room ) {
            CONSOLE_Print("[OHCONNECT: "+roomname+"] "+name+" has joined the room");
          }    
        }
      }
    }
  }
}

string OHConnect :: wrapMessage( string message ) {
CONSOLE_Print(message);
    std::string msg = "{'type':'usermsg', 'datetime':'', 'user': 'OHC Bot', 'text':'"+message+"', 'color':'#FF0808', 'l': '10', 'room': '"+m_Room+"', 'roomname':'"+m_RoomName+"', 'pw':'";
    if(m_GHost) { 
      msg += m_GHost->m_OHCPass;
    }
    msg += "'}";
    CONSOLE_Print(msg);
    return msg;
}

void OHConnect :: Connect( )
{
  /* connect */
  m_Socket = new CTCPClient();
  m_Socket->Connect("", IP, Port);

  if(m_Socket->HasError( )) {
    //the socket has an error
    CONSOLE_Print("[OHConnect] disconnected from OHConnect due socket error");
    CONSOLE_Print("[OHConnect] "+m_Socket->GetErrorString( ));
  }
  else {
    m_Connected = true;
    CONSOLE_Print("[OHConnect] Starting connection to socket");
  }
}

void OHConnect :: sendData(OHCHeader::opcode_type type, string message) {
  if( message.find("[OHConnect]") != -1) {
    return;
  }

  const uint8_t masking_key[4] = { 0x12, 0x34, 0x56, 0x78 };
  std::vector<uint8_t> header;
  std::vector<uint8_t> txbuf;

  bool useMask = true;
  uint64_t message_size = message.size();

  header.assign(2 + (message_size >= 126 ? 2 : 0) + (message_size >= 65536 ? 6 : 0) + (useMask ? 4 : 0), 0);
  header[0] = 0x80 | type;

  if (false) { }
  else if (message_size < 126) {
    header[1] = (message_size & 0xff) | (useMask ? 0x80 : 0);
    if (useMask) {
      header[2] = masking_key[0];
      header[3] = masking_key[1];
      header[4] = masking_key[2];
      header[5] = masking_key[3];
    }
  }
  else if (message_size < 65536) {
    header[1] = 126 | (useMask ? 0x80 : 0);
    header[2] = (message_size >> 8) & 0xff;
    header[3] = (message_size >> 0) & 0xff;
    if (useMask) {
      header[4] = masking_key[0];
      header[5] = masking_key[1];
      header[6] = masking_key[2];
      header[7] = masking_key[3];
    }
  }
  else {
    header[1] = 127 | (useMask ? 0x80 : 0);
    header[2] = (message_size >> 56) & 0xff;
    header[3] = (message_size >> 48) & 0xff;
    header[4] = (message_size >> 40) & 0xff;
    header[5] = (message_size >> 32) & 0xff;
    header[6] = (message_size >> 24) & 0xff;
    header[7] = (message_size >> 16) & 0xff;
    header[8] = (message_size >> 8) & 0xff;
    header[9] = (message_size >> 0) & 0xff;
    if (useMask) {
      header[10] = masking_key[0];
      header[11] = masking_key[1];
      header[12] = masking_key[2];
      header[13] = masking_key[3];
    }
  }

  txbuf.insert(txbuf.end(), header.begin(), header.end());
  txbuf.insert(txbuf.end(), message.begin(), message.end());
  if (useMask) {
    for (size_t i = 0; i != message.size(); ++i) { 
      *(txbuf.end() - message.size() + i) ^= masking_key[i&0x3]; 
    }
  }

  m_Socket->PutBytes(txbuf);
}

void OHConnect :: recvData( ) {
  OHCHeader ws;
  if(rxbuf.size()<2) { return; }

  const uint8_t * data = (uint8_t *) &rxbuf[0]; 
  ws.fin = (data[0] & 0x80) == 0x80;
  ws.opcode = (OHCHeader::opcode_type) (data[0] & 0x0f);
  ws.mask = (data[1] & 0x80) == 0x80;
  ws.N0 = (data[1] & 0x7f);
  ws.header_size = 2 + (ws.N0 == 126? 2 : 0) + (ws.N0 == 127? 6 : 0) + (ws.mask? 4 : 0);

  if (rxbuf.size() < ws.header_size) { return; }
  int i;
  if (ws.N0 < 126) {
    ws.N = ws.N0;
    i = 2;
  }
  else if (ws.N0 == 126) {
    ws.N = 0;
    ws.N |= ((uint64_t) data[2]) << 8;
    ws.N |= ((uint64_t) data[3]) << 0;
    i = 4;
  }
  else if (ws.N0 == 127) {
    ws.N = 0;
    ws.N |= ((uint64_t) data[2]) << 56;
    ws.N |= ((uint64_t) data[3]) << 48;
    ws.N |= ((uint64_t) data[4]) << 40;
    ws.N |= ((uint64_t) data[5]) << 32;
    ws.N |= ((uint64_t) data[6]) << 24;
    ws.N |= ((uint64_t) data[7]) << 16;
    ws.N |= ((uint64_t) data[8]) << 8;
    ws.N |= ((uint64_t) data[9]) << 0;
    i = 10;
  } 
  if (ws.mask) {
    ws.masking_key[0] = ((uint8_t) data[i+0]) << 0;
    ws.masking_key[1] = ((uint8_t) data[i+1]) << 0;
    ws.masking_key[2] = ((uint8_t) data[i+2]) << 0;
    ws.masking_key[3] = ((uint8_t) data[i+3]) << 0;
  }
  else {
    ws.masking_key[0] = 0;
    ws.masking_key[1] = 0;
    ws.masking_key[2] = 0;
    ws.masking_key[3] = 0;
  }
  if (rxbuf.size() < ws.header_size+ws.N) { return; }
  if (false) { }
  else if (ws.opcode == OHCHeader::TEXT_FRAME && ws.fin) {
    if (ws.mask) { 
      for (size_t i = 0; i != ws.N; ++i) { 
        rxbuf[i+ws.header_size] ^= ws.masking_key[i&0x3]; 
      } 
    }
    std::string d(rxbuf.begin()+ws.header_size, rxbuf.begin()+ws.header_size+(size_t)ws.N);
    ProcessEvent(d);
  }
  else if (ws.opcode == OHCHeader::PING) {
    if (ws.mask) { 
      for (size_t i = 0; i != ws.N; ++i) { 
        rxbuf[i+ws.header_size] ^= ws.masking_key[i&0x3];
      } 
    }
    std::string d(rxbuf.begin()+ws.header_size, rxbuf.begin()+ws.header_size+(size_t)ws.N);
    sendData(OHCHeader::PONG, d);
  }
  else if (ws.opcode == OHCHeader::PONG) { }
  else if (ws.opcode == OHCHeader::CLOSE) { }

  rxbuf.erase(rxbuf.begin(), rxbuf.begin() + ws.header_size+(size_t)ws.N);
}

void OHConnect :: joinRoom(string room, string roomname) {
  if(!room.empty()&&!roomname.empty()) {
    m_Room = room; m_RoomName = roomname;
  }
  string joinMessage = "{'user':'OHC Bot','newuser':'OHC Bot','message':'','ip':'','room':'"+m_Room+"', 'roomname': '"+m_RoomName+"', 'pid': '', 'color':'#FF0808', 'l': '10', 'pw': '"+m_GHost->m_OHCPass+"'}";
  sendData(OHCHeader::TEXT_FRAME, joinMessage);
}
