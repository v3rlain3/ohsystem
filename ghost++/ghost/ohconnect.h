/**
* Copyright [2013-2014] [OHsystem]
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

#ifndef OHConnect_H
#define OHConnect_H

//
// OHCONNECT
//

class CTCPClient;
class CBaseGame;
class CCommandPacket;
struct OHCHeader {
  unsigned header_size;
  bool fin;
  bool mask;
  enum opcode_type {
    CONTINUTATION = 0x0,
    TEXT_FRAME = 0x1,
    BINARY_FRAME = 0x2,
    CLOSE = 8,
    PING = 9,
    PONG = 0xa
  } opcode;
  uint64_t N0;
  uint64_t N;
  uint8_t masking_key[4];
};

class OHConnect
{
public:
    CGHost *m_GHost;
    CBaseGame *m_Game;
protected:
    string m_Room;
    string m_RoomName;
    bool m_Connected;
    string IP;
    uint32_t Port;
    queue<CCommandPacket *> m_Packets;
    bool m_FirstConnect;
    uint32_t m_LastSendTime;
    bool m_Handshake;
    uint32_t m_ClientID;
    std::vector<uint8_t> rxbuf;
    uint32_t LastPingTime;
    uint32_t LastConnectionAttemp;
private:
    CTCPClient *m_Socket; // the connection to ohconnect

public:
    OHConnect( CGHost *nGHost, CBaseGame *nGame, string ip, uint32_t port );

    virtual ~OHConnect( );

    /* processing functions */
    unsigned int SetFD( void *fd, void *send_fd, int *nfds );
    bool Update( void *fd, void *send_fd );
    void ExtractPackets( );
    void ProcessPackets( );
    void Connect( );
    void ProcessEvent( string message );
    void sendData(OHCHeader::opcode_type type, string message);
    void recvData();
    string wrapMessage( string message );
    void joinRoom(string room, string roomname);
    inline bool getConnected( ) { return m_Socket->GetConnected( ); }
};

#endif
