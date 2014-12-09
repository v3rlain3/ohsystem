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

#include "ghost.h"
#include "util.h"
#include "language.h"
#include "socket.h"
#include "commandpacket.h"
#include "bnet.h"
#include "map.h"
#include "gameplayer.h"
#include "gameprotocol.h"
#include "gcbiprotocol.h"
#include "gpsprotocol.h"
#include "game_base.h"
#include "ghost.h"
#include "ghostdb.h"

//
// CPotentialPlayer
//

CPotentialPlayer :: CPotentialPlayer( CGameProtocol *nProtocol, CBaseGame *nGame, CTCPSocket *nSocket ) : m_Protocol( nProtocol ), m_Game( nGame ), m_Socket( nSocket ), m_DeleteMe( false ), m_Error( false ), m_IncomingJoinPlayer( NULL ), m_IncomingGarenaUser( NULL ), m_Banned( false )
{
	if (nSocket)
		m_CachedIP = nSocket->GetIPString();
	else
		m_CachedIP = string();
}

CPotentialPlayer :: ~CPotentialPlayer( )
{
    if( m_Socket )
        delete m_Socket;

    while( !m_Packets.empty( ) )
    {
        delete m_Packets.front( );
        m_Packets.pop( );
    }

    delete m_IncomingJoinPlayer;
    delete m_IncomingGarenaUser;
}

BYTEARRAY CPotentialPlayer :: GetGarenaIP( )
{
    if( m_IncomingGarenaUser == NULL ) {
        return UTIL_CreateByteArray( (uint32_t) 0, true );
    } else {
        return UTIL_CreateByteArray( m_IncomingGarenaUser->GetIP( ), true );
    }
}

BYTEARRAY CPotentialPlayer :: GetExternalIP( )
{
    unsigned char Zeros[] = { 0, 0, 0, 0 };

    if( m_Socket ) {
        if( m_IncomingGarenaUser != NULL )
            return GetGarenaIP( );
        else
            return m_Socket->GetIP( );
    }

    return UTIL_CreateByteArray( Zeros, 4 );
}

string CPotentialPlayer :: GetExternalIPString( )
{
    if( m_Socket ) {
		string IPString = string();
        if( m_IncomingGarenaUser != NULL ) {
            BYTEARRAY GarenaIP = GetGarenaIP( );
			IPString = UTIL_ToString(GarenaIP[0]) + "." + UTIL_ToString(GarenaIP[1]) + "." + UTIL_ToString(GarenaIP[2]) + "." + UTIL_ToString(GarenaIP[3]);
        } else {
			IPString = m_Socket->GetIPString( );
        }

		if (!IPString.empty() && IPString != "0.0.0.0")
			m_CachedIP = IPString;
			return IPString;
    }

	return m_CachedIP;
}

bool CPotentialPlayer :: Update( void *fd )
{
    if( m_DeleteMe )
        return true;

    if( !m_Socket )
        return false;

    m_Socket->DoRecv( (fd_set *)fd );
    ExtractPackets( );
    ProcessPackets( );

    // don't call DoSend here because some other players may not have updated yet and may generate a packet for this player
    // also m_Socket may have been set to NULL during ProcessPackets but we're banking on the fact that m_DeleteMe has been set to true as well so it'll short circuit before dereferencing

    return m_DeleteMe || m_Error || m_Socket->HasError( ) || !m_Socket->GetConnected( );
}

void CPotentialPlayer :: ExtractPackets( )
{
    if( !m_Socket )
        return;

    // extract as many packets as possible from the socket's receive buffer and put them in the m_Packets queue

    string *RecvBuffer = m_Socket->GetBytes( );
    BYTEARRAY Bytes = UTIL_CreateByteArray( (unsigned char *)RecvBuffer->c_str( ), RecvBuffer->size( ) );

    // a packet is at least 4 bytes so loop as long as the buffer contains 4 bytes

    while( Bytes.size( ) >= 4 )
    {
        if( Bytes[0] == W3GS_HEADER_CONSTANT || Bytes[0] == GPS_HEADER_CONSTANT || Bytes[0] == GCBI_HEADER_CONSTANT )
        {
            // bytes 2 and 3 contain the length of the packet

            uint16_t Length = UTIL_ByteArrayToUInt16( Bytes, false, 2 );

            if( Length >= 4 )
            {
                if( Bytes.size( ) >= Length )
                {
                    m_Packets.push( new CCommandPacket( Bytes[0], Bytes[1], BYTEARRAY( Bytes.begin( ), Bytes.begin( ) + Length ) ) );
                    *RecvBuffer = RecvBuffer->substr( Length );
                    Bytes = BYTEARRAY( Bytes.begin( ) + Length, Bytes.end( ) );
                }
                else
                    return;
            }
            else
            {
                m_Error = true;
                m_ErrorString = "received invalid packet from player (bad length)";
                return;
            }
        }
        else
        {
            m_Error = true;
            m_ErrorString = "received invalid packet from player (bad header constant)";
            return;
        }
    }
}

void CPotentialPlayer :: ProcessPackets( )
{
    if( !m_Socket )
        return;

    // process all the received packets in the m_Packets queue

    while( !m_Packets.empty( ) )
    {
        CCommandPacket *Packet = m_Packets.front( );
        m_Packets.pop( );

        if( Packet->GetPacketType( ) == W3GS_HEADER_CONSTANT )
        {
            // the only packet we care about as a potential player is W3GS_REQJOIN, ignore everything else

            switch( Packet->GetID( ) )
            {
            case CGameProtocol :: W3GS_REQJOIN:
                delete m_IncomingJoinPlayer;
                m_IncomingJoinPlayer = m_Protocol->RECEIVE_W3GS_REQJOIN( Packet->GetData( ) );

                if( m_IncomingJoinPlayer && !m_Banned)
                    m_Game->EventPlayerJoined( this, m_IncomingJoinPlayer );

                // don't continue looping because there may be more packets waiting and this parent class doesn't handle them
                // EventPlayerJoined creates the new player, NULLs the socket, and sets the delete flag on this object so it'll be deleted shortly
                // any unprocessed packets will be copied to the new CGamePlayer in the constructor or discarded if we get deleted because the game is full

                delete Packet;
                return;
            }
        }

        else if( Packet->GetPacketType( ) == GCBI_HEADER_CONSTANT )
        {
            if( Packet->GetID( ) == CGCBIProtocol :: GCBI_INIT )
            {
                delete m_IncomingGarenaUser;
                m_IncomingGarenaUser = m_Game->m_GHost->m_GCBIProtocol->RECEIVE_GCBI_INIT( Packet->GetData( ) );
                string RoomID = UTIL_ToString(m_IncomingGarenaUser->GetRoomID( ));
                m_RoomName = m_Game->m_GHost->GetRoomName( string( RoomID.begin( ), RoomID.end( ) ) );
                CONSOLE_Print( "[GCBI] Garena user detected; userid=" + UTIL_ToString( m_IncomingGarenaUser->GetUserID( ) ) + ", roomid=" + RoomID + ", RoomName=" + m_RoomName + ", experience=" + UTIL_ToString( m_IncomingGarenaUser->GetUserExp( ) ) + ", country=" + m_IncomingGarenaUser->GetCountryCode( ) );
            }
        }

        delete Packet;
    }
}

void CPotentialPlayer :: Send( BYTEARRAY data )
{
    if( m_Socket )
        m_Socket->PutBytes( data );
}

//
// CGamePlayer
//

CGamePlayer :: CGamePlayer( CGameProtocol *nProtocol, CBaseGame *nGame, CTCPSocket *nSocket, unsigned char nPID, string nJoinedRealm, string nName, BYTEARRAY nInternalIP, bool nReserved ) : CPotentialPlayer( nProtocol, nGame, nSocket ),
    m_PID( nPID ), m_Name( nName ), m_InternalIP( nInternalIP ), m_JoinedRealm( nJoinedRealm ), m_TotalPacketsSent( 0 ), m_TotalPacketsReceived( 0 ), m_Left( 0 ), m_LeftCode( PLAYERLEAVE_LOBBY ), m_LoginAttempts( 0 ), m_SyncCounter( 0 ), m_JoinTime( GetTime( ) ),
    m_LastMapPartSent( 0 ), m_LastMapPartAcked( 0 ), m_StartedDownloadingTicks( 0 ), m_FinishedLoadingTicks( 0 ), m_StartedLaggingTicks( 0 ), m_StatsSentTime( 0 ), m_StatsDotASentTime( 0 ), m_LastGProxyWaitNoticeSentTime( 0 ), m_Score( -100000.0 ), m_WinPerc( 0.0 ), m_LeavePerc( 0.0 ), m_Games( 0 ),
    m_LoggedIn( false ), m_Spoofed( false ), m_PasswordProt( 0 ), m_Registered( false ), m_Reserved( nReserved ), m_WhoisShouldBeSent( false ), m_WhoisSent( false ), m_DownloadAllowed( false ), m_DownloadStarted( false ), m_DownloadFinished( false ), m_FinishedLoading( false ), m_Lagging( false ),
    m_DropVote( false ), m_KickVote( false ), m_Muted( false ), m_LeftMessageSent( false ), m_GProxy( false ), m_GProxyDisconnectNoticeSent( false ), m_GProxyReconnectKey( rand( ) ), m_LastGProxyAckTime( 0 ), m_Autoban( false ), m_Locked( false ), m_ForfeitVote( false ), m_DrawVote( false ), m_TimeActive( 0 ), m_UsedPause( false ), m_PauseTried( 0 ), m_CLetter( "??" ), m_Country( "unknown" ), m_TheThingAmount( 0 ), m_TheThing( "" ), m_TheThingType( 0 ), m_Count( 0 ), m_Silence( false ), m_HasLeft( false ),
    m_AFKMarked( false ), m_SafeDrop( false ), m_FeedLevel( 0 ), m_VKTimes( 0 ), m_HighPingTimes( 0 ), m_AnnounceTime( GetTime( ) ), m_Level( 0 ), m_LevelName( "unknown" ), m_StartVoted( false ), m_GlobalChatMuted( false ), m_InsultM( "" ), m_DownloadTicks( 0 ), m_Checked( 0 ), m_VotedForInterruption( false ),
    m_VotedMode( 0 ), m_NoLag( false ), m_ActionCounter( 0 ), m_FirstPartOfMinute( 0 ), m_SecondPartOfMinute( 0 ), m_ThirdPartOfMinute( 0 ), m_FourthPartOfMinute( 0 ), m_FifthPartOfMinute( 0 ), m_SixthPartOfMinute( 0 ), m_AFKWarnings( 0 ), m_LastAfkWarn( 0 ), m_EXP( 0 ), m_ID( 0 ), m_VotedForBalance(false), m_Reputation( 0.00 ), m_PlayerLanguage( "en" ), m_PenalityLevel( 0 ),
	m_RequestedSwap(false), m_SwapTarget(255), m_LastAttackCommandToFountain(GetTime()), m_FFLevel(0), m_Cookies( 0 ), m_DenyLimit( m_Game->m_GHost->m_DenyLimit ), m_SwapLimit( m_Game->m_GHost->m_SwapLimit )
{

}

CGamePlayer :: CGamePlayer( CPotentialPlayer *potential, unsigned char nPID, string nJoinedRealm, string nName, BYTEARRAY nInternalIP, bool nReserved ) : CPotentialPlayer( potential->m_Protocol, potential->m_Game, potential->GetSocket( ) ),
    m_PID( nPID ), m_Name( nName ), m_InternalIP( nInternalIP ), m_JoinedRealm( nJoinedRealm ), m_TotalPacketsSent( 0 ), m_TotalPacketsReceived( 1 ), m_Left( 0 ), m_LeftCode( PLAYERLEAVE_LOBBY ), m_LoginAttempts( 0 ), m_SyncCounter( 0 ), m_JoinTime( GetTime( ) ),
    m_LastMapPartSent( 0 ), m_LastMapPartAcked( 0 ), m_StartedDownloadingTicks( 0 ), m_FinishedLoadingTicks( 0 ), m_StartedLaggingTicks( 0 ), m_StatsSentTime( 0 ), m_StatsDotASentTime( 0 ), m_LastGProxyWaitNoticeSentTime( 0 ), m_Score( -100000.0 ), m_WinPerc( 0.0 ), m_LeavePerc( 0.0 ), m_Games( 0 ),
    m_LoggedIn( false ), m_Spoofed( false ), m_PasswordProt( 0 ), m_Registered( false ), m_Reserved( nReserved ), m_WhoisShouldBeSent( false ), m_WhoisSent( false ), m_DownloadAllowed( false ), m_DownloadStarted( false ), m_DownloadFinished( false ), m_FinishedLoading( false ), m_Lagging( false ),
    m_DropVote( false ), m_KickVote( false ), m_Muted( false ), m_LeftMessageSent( false ), m_GProxy( false ), m_GProxyDisconnectNoticeSent( false ), m_GProxyReconnectKey( rand( ) ), m_LastGProxyAckTime( 0 ), m_Autoban( false ), m_Locked( false ), m_ForfeitVote( false ), m_DrawVote( false ), m_TimeActive( 0 ), m_UsedPause( false ), m_PauseTried( 0 ), m_CLetter( "??" ), m_Country( "unknown" ), m_TheThingAmount( 0 ), m_TheThing( "" ), m_TheThingType( 0 ), m_Count( 0 ), m_Silence( false ), m_HasLeft( false ),
    m_AFKMarked( false ), m_SafeDrop( false ), m_FeedLevel( 0 ), m_VKTimes( 0 ), m_HighPingTimes( 0 ), m_AnnounceTime( GetTime( ) ), m_Level( 0 ), m_LevelName( "unknown" ), m_StartVoted( false ), m_GlobalChatMuted( false ), m_InsultM( "" ), m_DownloadTicks( 0 ), m_Checked( 0 ), m_VotedForInterruption( false ),
    m_VotedMode( 0 ), m_NoLag( false ), m_ActionCounter( 0 ), m_FirstPartOfMinute( 0 ), m_SecondPartOfMinute( 0 ), m_ThirdPartOfMinute( 0 ), m_FourthPartOfMinute( 0 ), m_FifthPartOfMinute( 0 ), m_SixthPartOfMinute( 0 ), m_AFKWarnings( 0 ), m_LastAfkWarn( 0 ), m_EXP( 0 ), m_ID( 0 ), m_VotedForBalance(false), m_Reputation( 0.00 ), m_PlayerLanguage( "en" ), m_PenalityLevel( 0 ),
	m_RequestedSwap(false), m_SwapTarget(255), m_LastAttackCommandToFountain(GetTime()), m_FFLevel(0), m_Cookies( 0 ), m_DenyLimit( m_Game->m_GHost->m_DenyLimit ), m_SwapLimit( m_Game->m_GHost->m_SwapLimit )
{
    // todotodo: properly copy queued packets to the new player, this just discards them
    // this isn't a big problem because official Warcraft III clients don't send any packets after the join request until they receive a response
    // m_Packets = potential->GetPackets( );


    // hackhack: we initialize m_TotalPacketsReceived to 1 because the CPotentialPlayer must have received a W3GS_REQJOIN before this class was created
    // to fix this we could move the packet counters to CPotentialPlayer and copy them here
    // note: we must make sure we never send a packet to a CPotentialPlayer otherwise the send counter will be incorrect too! what a mess this is...
    // that said, the packet counters are only used for managing GProxy++ reconnections
}

CGamePlayer :: ~CGamePlayer( )
{

}

string CGamePlayer :: GetNameTerminated( )
{
    // if the player's name contains an unterminated colour code add the colour terminator to the end of their name
    // this is useful because it allows you to print the player's name in a longer message which doesn't colour all the subsequent text

    string LowerName = m_Name;
    transform( LowerName.begin( ), LowerName.end( ), LowerName.begin( ), ::tolower );
    string :: size_type Start = LowerName.find( "|c" );
    string :: size_type End = LowerName.find( "|r" );

    if( Start != string :: npos && ( End == string :: npos || End < Start ) )
        return m_Name + "|r";
    else
        return m_Name;
}

uint32_t CGamePlayer :: GetPing( bool LCPing )
{
    // just average all the pings in the vector, nothing fancy

    if( m_Pings.empty( ) )
        return 0;

    uint32_t AvgPing = 0;

    for( unsigned int i = 0; i < m_Pings.size( ); ++i )
        AvgPing += m_Pings[i];

    AvgPing /= m_Pings.size( );

    if( LCPing )
        return AvgPing / 2;
    else
        return AvgPing;
}

bool CGamePlayer :: GetIsIgnoring( string username )
{
    transform( username.begin( ), username.end( ), username.begin( ), ::tolower );

    for( vector<string> :: iterator i = m_IgnoreList.begin( ); i != m_IgnoreList.end( ); ++i )
    {
        if( (*i) == username )
            return true;
    }

    return false;
}

void CGamePlayer :: Ignore( string username )
{
    if( GetIsIgnoring( username ) ) return;
    transform( username.begin( ), username.end( ), username.begin( ), ::tolower );

    m_IgnoreList.push_back( username );
}

void CGamePlayer :: UnIgnore( string username )
{
    transform( username.begin( ), username.end( ), username.begin( ), ::tolower );

    for( vector<string> :: iterator i = m_IgnoreList.begin( ); i != m_IgnoreList.end( ); )
    {
        if( (*i) == username )
        {
            m_IgnoreList.erase( i );
            continue;
        }

        ++i;
    }
}

bool CGamePlayer :: Update( void *fd )
{
    // wait 4 seconds after joining before sending the /whois or /w
    // if we send the /whois too early battle.net may not have caught up with where the player is and return erroneous results

    if( m_WhoisShouldBeSent && !m_Spoofed && !m_WhoisSent && !m_JoinedRealm.empty( ) && GetTime( ) - m_JoinTime >= 4 )
    {
        // todotodo: we could get kicked from battle.net for sending a command with invalid characters, do some basic checking

        for( vector<CBNET *> :: iterator i = m_Game->m_GHost->m_BNETs.begin( ); i != m_Game->m_GHost->m_BNETs.end( ); ++i )
        {
            if( (*i)->GetServer( ) == m_JoinedRealm )
            {
                if( m_Game->GetGameState( ) == GAME_PUBLIC )
                {
                    if( (*i)->GetPasswordHashType( ) == "pvpgn" )
                        (*i)->QueueChatCommand( "/whereis " + m_Name );
                    else
                        (*i)->QueueChatCommand( "/whois " + m_Name );
                }
                else if( m_Game->GetGameState( ) == GAME_PRIVATE )
                    (*i)->QueueChatCommand( m_Game->m_GHost->m_Language->SpoofCheckByReplying( ), m_Name, true );
            }
        }

        m_WhoisSent = true;
    }

    // check for socket timeouts
    // if we don't receive anything from a player for 30 seconds we can assume they've dropped
    // this works because in the lobby we send pings every 5 seconds and expect a response to each one
    // and in the game the Warcraft 3 client sends keepalives frequently (at least once per second it looks like)

    if( m_Socket && GetTime( ) - m_Socket->GetLastRecv( ) >= 30 )
        m_Game->EventPlayerDisconnectTimedOut( this );

    // make sure we're not waiting too long for the first MAPSIZE packet

    /*    	if( m_ConnectionState == 1 && GetTicks( ) - m_ConnectionTime > 5000 && !m_Game->GetGameLoaded() && !m_Game->GetGameLoading() )
        {
            CONSOLE_Print( "[DENY] Kicking player: MAPSIZE not received within five seconds" );
            m_DeleteMe = true;
                SetLeftReason( "MAPSIZE not received within five seconds" );
                SetLeftCode( PLAYERLEAVE_LOBBY );
                m_Game->OpenSlot( m_Game->GetSIDFromPID( GetPID( ) ), false );
            }
    */
    // disconnect if the player is downloading too slowly

    // make this a bit dynamically, first 10 KB/s is a bit too low, increasing to 100KB/s
    //decreasing the checktime to 5 seconds
    // adding an actual playercheck how many players are ingame, if there less than 1 open slots we deny users with a download rate under 500KB/s
    if( m_DownloadStarted && !m_DownloadFinished && !m_Game->GetGameLoaded() && !m_Game->GetGameLoading() && GetLastMapPartSent( ) > 0 && m_Game->m_GHost->m_KickSlowDownloader )
    {
        uint32_t downloadingTime = GetTicks( ) - m_StartedDownloadingTicks;

        if( downloadingTime > 5000 && m_Level < 1)
        {
            if( GetLastMapPartAcked( ) / downloadingTime < 500 && m_Game->GetSlotsOccupied( ) <= 1 )
            {
                m_DeleteMe = true;
                SetLeftReason( "download speed too low" );
                SetLeftCode( PLAYERLEAVE_LOBBY );
                m_Game->SendAllChat( m_Game->m_GHost->m_Language->UserWasKickedForSlowDownloadRate( m_Name ) );
                m_Game->OpenSlot( m_Game->GetSIDFromPID( GetPID( ) ), false );
            }
            else if( GetLastMapPartAcked( ) / downloadingTime < 100 )
            {
                m_DeleteMe = true;
                SetLeftReason( "download speed too low" );
                SetLeftCode( PLAYERLEAVE_LOBBY );
                m_Game->SendAllChat( m_Game->m_GHost->m_Language->UserWasKickedForSlowDownloadRate( m_Name ) );
                m_Game->OpenSlot( m_Game->GetSIDFromPID( GetPID( ) ), false );
            }
        }
    }

    // unmute player
    if( GetMuted( ) && m_MutedAuto && GetTicks( ) - m_MutedTicks > 30000  )
    {
        SetMuted( false );
        m_Game->SendChat( m_PID, "["+m_Game->m_GHost->m_BotManagerName+"] "+m_Game->m_GHost->m_Language->UserWasAutomaticallyUnmuted( ) );
        m_MuteMessages.clear( );
    }

    // GProxy++ acks

    if( m_GProxy && GetTime( ) - m_LastGProxyAckTime >= 10 )
    {
        if( m_Socket )
            m_Socket->PutBytes( m_Game->m_GHost->m_GPSProtocol->SEND_GPSS_ACK( m_TotalPacketsReceived ) );

        m_LastGProxyAckTime = GetTime( );
    }

    // base class update

    CPotentialPlayer :: Update( fd );
    bool Deleting;

    if( m_GProxy && m_Game->GetGameLoaded( ) )
        Deleting = m_DeleteMe || m_Error;
    else
        Deleting = m_DeleteMe || m_Error || m_Socket->HasError( ) || !m_Socket->GetConnected( );

    // try to find out why we're requesting deletion
    // in cases other than the ones covered here m_LeftReason should have been set when m_DeleteMe was set

    if( m_Error )
    {
        m_Game->EventPlayerDisconnectPlayerError( this );
        m_Socket->Reset( );
        return Deleting;
    }

    if( m_Socket )
    {
        if( m_Socket->HasError( ) )
        {
            m_Game->EventPlayerDisconnectSocketError( this );
            m_Socket->Reset( );
        }
        else if( !m_Socket->GetConnected( ) )
        {
            m_Game->EventPlayerDisconnectConnectionClosed( this );
            m_Socket->Reset( );
        }
    }

    return Deleting;
}

void CGamePlayer :: ExtractPackets( )
{
    if( !m_Socket )
        return;

    // extract as many packets as possible from the socket's receive buffer and put them in the m_Packets queue

    string *RecvBuffer = m_Socket->GetBytes( );
    BYTEARRAY Bytes = UTIL_CreateByteArray( (unsigned char *)RecvBuffer->c_str( ), RecvBuffer->size( ) );

    // a packet is at least 4 bytes so loop as long as the buffer contains 4 bytes

    while( Bytes.size( ) >= 4 )
    {
        if( Bytes[0] == W3GS_HEADER_CONSTANT || Bytes[0] == GPS_HEADER_CONSTANT || Bytes[0] == GCBI_HEADER_CONSTANT )
        {
            // bytes 2 and 3 contain the length of the packet

            uint16_t Length = UTIL_ByteArrayToUInt16( Bytes, false, 2 );

            if( Length >= 4 )
            {
                if( Bytes.size( ) >= Length )
                {
                    m_Packets.push( new CCommandPacket( Bytes[0], Bytes[1], BYTEARRAY( Bytes.begin( ), Bytes.begin( ) + Length ) ) );

                    if( Bytes[0] == W3GS_HEADER_CONSTANT )
                        ++m_TotalPacketsReceived;

                    *RecvBuffer = RecvBuffer->substr( Length );
                    Bytes = BYTEARRAY( Bytes.begin( ) + Length, Bytes.end( ) );
                }
                else
                    return;
            }
            else
            {
                m_Error = true;
                m_ErrorString = "received invalid packet from player (bad length)";
                return;
            }
        }
        else
        {
            m_Error = true;
            m_ErrorString = "received invalid packet from player (bad header constant)";
            return;
        }
    }
}

void CGamePlayer :: ProcessPackets( )
{
    if( !m_Socket )
        return;

    CIncomingAction *Action = NULL;
    CIncomingChatPlayer *ChatPlayer = NULL;
    CIncomingMapSize *MapSize = NULL;
    bool HasMap = false;
    uint32_t CheckSum = 0;
    uint32_t Pong = 0;

    // process all the received packets in the m_Packets queue

    while( !m_Packets.empty( ) )
    {
        CCommandPacket *Packet = m_Packets.front( );
        m_Packets.pop( );

        if( Packet->GetPacketType( ) == W3GS_HEADER_CONSTANT )
        {
            switch( Packet->GetID( ) )
            {
            case CGameProtocol :: W3GS_LEAVEGAME:
                m_Game->EventPlayerLeft( this, m_Protocol->RECEIVE_W3GS_LEAVEGAME( Packet->GetData( ) ) );
                break;

            case CGameProtocol :: W3GS_GAMELOADED_SELF:
                if( m_Protocol->RECEIVE_W3GS_GAMELOADED_SELF( Packet->GetData( ) ) )
                {
                    if( !m_FinishedLoading && m_Game->GetGameLoading( ) )
                    {
                        m_FinishedLoading = true;
                        m_FinishedLoadingTicks = GetTicks( );
                        m_Game->EventPlayerLoaded( this );
                    }
                    else
                    {
                        // we received two W3GS_GAMELOADED_SELF packets from this player!
                    }
                }

                break;

            case CGameProtocol :: W3GS_OUTGOING_ACTION:
                Action = m_Protocol->RECEIVE_W3GS_OUTGOING_ACTION( Packet->GetData( ), m_PID );

                if( Action )
                {
                    // don't delete Action here because the game is going to store it in a queue and delete it later
                    m_Game->EventPlayerAction( this, Action );
                }

                break;

            case CGameProtocol :: W3GS_OUTGOING_KEEPALIVE:
                CheckSum = m_Protocol->RECEIVE_W3GS_OUTGOING_KEEPALIVE( Packet->GetData( ) );
                m_CheckSums.push( CheckSum );
                ++m_SyncCounter;
                m_Game->EventPlayerKeepAlive( this, CheckSum );
                break;

            case CGameProtocol :: W3GS_CHAT_TO_HOST:
                ChatPlayer = m_Protocol->RECEIVE_W3GS_CHAT_TO_HOST( Packet->GetData( ) );

                if( ChatPlayer )
                {
                    // determine if we should auto-mute this player
                    if( ChatPlayer->GetType( ) == CIncomingChatPlayer :: CTH_MESSAGE || ChatPlayer->GetType( ) == CIncomingChatPlayer :: CTH_MESSAGEEXTRA )
                    {
                        if( m_Level <= 1 &&! GetMuted( ) )
                        {
                            m_MuteMessages.push_back( GetTicks( ) );

                            if( m_MuteMessages.size( ) > 7 )
                                m_MuteMessages.erase( m_MuteMessages.begin( ) );

                            uint32_t RecentCount = 0;
                            for( unsigned int i = 0; i < m_MuteMessages.size( ); ++i )
                            {
                                if( GetTicks( ) - m_MuteMessages[i] < 5000 )
                                {
                                    RecentCount++;
                                }
                            }

                            if( m_Game->m_GHost->m_AutoMuteSpammer && RecentCount >= 7 )
                            {
                                m_Count++;
                                if(  m_Count == 1 )
                                {
                                    SetMuted( true );
                                    m_MutedAuto = true;
                                    m_Game->SendChat( m_PID, "["+m_Game->m_GHost->m_BotManagerName+"] "+m_Game->m_GHost->m_Language->SpamWarning( ) );
                                    m_MuteMessages.clear( );
                                    m_Game->SendAllChat( "["+m_Game->m_GHost->m_BotManagerName+"] " + m_Game->m_GHost->m_Language->UserWasMutedForReason( m_Name, "spamming" ) );
                                }
                                if( m_Count == 2 )
                                {
                                    m_Game->SendAllChat( m_Game->m_GHost->m_Language->UserIgnoerNotify( m_Name ) );
                                    m_Game->SendChat( m_PID, "["+m_Game->m_GHost->m_BotManagerName+"] "+m_Game->m_GHost->m_Language->SpamWarning2( ) );
                                    SetMuted( true );
                                    m_Game->m_Pairedpenps.push_back( Pairedpenp( string(), m_Game->m_GHost->m_DB->Threadedpenp( m_Name, "Spam" , m_Game->m_GHost->m_BotManagerName, 1, "add" ) ) );
                                    m_MutedAuto = true;
                                    m_Game->SendAllChat( "["+m_Game->m_GHost->m_BotManagerName+"] " + m_Game->m_GHost->m_Language->UserWasMutedForReason( m_Name, "spamming" ) );
                                }
                                if( m_Count == 3 )
                                {
				    m_Game->BanPlayerByPenality( m_Name, GetExternalIPString( ), m_Game->m_GHost->m_BotManagerName, m_PenalityLevel, "Spam" );
                                    SetMuted( true );
                                    m_Game->SendAllChat( "["+m_Game->m_GHost->m_BotManagerName+"] " + m_Game->m_GHost->m_Language->UserWasMutedForReason( m_Name, "spamming" ) );

                                }
                            }

                            //we adding this condition not in the next condition to avoid a jump into ghost.cpp to check if the message was a flame message or not
                            if(m_Game->m_GHost->m_FlameCheck)
                            {
                                //now check for flamers
                                if( m_Game->m_GHost->FlameCheck( ChatPlayer->GetMessage( ) ) )
                                {
                                    m_FlameMessages.push_back( GetTicks( ) );

                                    if( m_FlameMessages.size( ) > 10 )
                                        m_FlameMessages.erase( m_FlameMessages.begin( ) );

                                    RecentCount = 0;

                                    for( unsigned int i = 0; i < m_FlameMessages.size( ); ++i )
                                    {
                                        if( GetTicks( ) - m_FlameMessages[i] < 60000 )
                                            RecentCount++;
                                    }

                                    if( RecentCount == 1 )
                                    {
                                        m_Game->SendChat( m_PID, "["+m_Game->m_GHost->m_BotManagerName+"] "+m_Game->m_GHost->m_Language->FlameWarn ());
                                    }

                                    if( RecentCount == 2 )
                                    {
                                        m_Game->SendChat( m_PID, "["+m_Game->m_GHost->m_BotManagerName+"] "+m_Game->m_GHost->m_Language->FlameWarn2 () );
                                        SetMuted( true );
                                        m_MutedAuto = true;
                                        m_Game->SendAllChat( "["+m_Game->m_GHost->m_BotManagerName+"] " + m_Game->m_GHost->m_Language->UserWasMutedForReason( m_Name, "flaming" ) );
                                    }

                                    if( RecentCount == 3 )
                                    {
                                        m_Game->SendChat( m_PID, m_Game->m_GHost->m_Language->FlameWarn3 () );
                                        SetMuted( true );
                                        m_Game->m_Pairedpenps.push_back( Pairedpenp( string(), m_Game->m_GHost->m_DB->Threadedpenp( m_Name, "Flame/Insult" , m_Game->m_GHost->m_BotManagerName, 1, "add" ) ) );
                                        m_MutedAuto = true;
                                        m_Game->SendAllChat( "["+m_Game->m_GHost->m_BotManagerName+"] " + m_Game->m_GHost->m_Language->UserWasMutedForReason( m_Name, "flaming" ) );
                                    }

                                    if( RecentCount == 4 )
                                    {
					m_Game->BanPlayerByPenality( m_Name, GetExternalIPString( ), m_Game->m_GHost->m_BotManagerName, m_PenalityLevel, "Flame/Insult" );
                                        SetMuted( true );
                                        m_Game->SendAllChat( "["+m_Game->m_GHost->m_BotManagerName+"] " + m_Game->m_GHost->m_Language->UserWasMutedForReason( m_Name, "flaming" ) );

                                    }
                                    if( RecentCount == 5 )
                                    {
                                        //some people simple dont understand the ban policy.
                                        m_Game->BanPlayerByPenality( m_Name, GetExternalIPString( ), m_Game->m_GHost->m_BotManagerName, m_PenalityLevel, "Flame/Insult" );
					SetMuted( true );
                                        m_Game->SendAllChat( "["+m_Game->m_GHost->m_BotManagerName+"] " + m_Game->m_GHost->m_Language->UserWasMutedForReason( m_Name, "flaming" ) );

                                    }
                                }
                            }
                        }
                    }
                    m_Game->EventPlayerChatToHost( this, ChatPlayer );
                }
                delete ChatPlayer;
                ChatPlayer = NULL;
                break;

            case CGameProtocol :: W3GS_DROPREQ:
                // todotodo: no idea what's in this packet

                if( !m_DropVote )
                {
                    m_DropVote = true;
                    m_Game->EventPlayerDropRequest( this );
                }

                break;

            case CGameProtocol :: W3GS_MAPSIZE:
                MapSize = m_Protocol->RECEIVE_W3GS_MAPSIZE( Packet->GetData( ), m_Game->m_GHost->m_Map->GetMapSize( ) );

                if( MapSize )
                    m_Game->EventPlayerMapSize( this, MapSize );

                delete MapSize;
                MapSize = NULL;
                break;

            case CGameProtocol :: W3GS_PONG_TO_HOST:
                Pong = m_Protocol->RECEIVE_W3GS_PONG_TO_HOST( Packet->GetData( ) );

                // we discard pong values of 1
                // the client sends one of these when connecting plus we return 1 on error to kill two birds with one stone

                if( Pong != 1 )
                {
                    // we also discard pong values when we're downloading because they're almost certainly inaccurate
                    // this statement also gives the player a 5 second grace period after downloading the map to allow queued (i.e. delayed) ping packets to be ignored

                    if( !m_DownloadStarted || ( m_DownloadFinished && GetTime( ) - m_FinishedDownloadingTime >= 5 ) )
                    {
                        // we also discard pong values when anyone else is downloading if we're configured to

                        if( m_Game->m_GHost->m_PingDuringDownloads || !m_Game->IsDownloading( ) )
                        {
                            m_Pings.push_back( GetTicks( ) - Pong );

                            if( m_Pings.size( ) > 20 )
                                m_Pings.erase( m_Pings.begin( ) );
                        }
                    }
                }

                m_Game->EventPlayerPongToHost( this, Pong );
                break;
            }
        }
        else if( Packet->GetPacketType( ) == GPS_HEADER_CONSTANT )
        {
            BYTEARRAY Data = Packet->GetData( );

            if( Packet->GetID( ) == CGPSProtocol :: GPS_INIT )
            {
                if( m_Game->m_GHost->m_Reconnect )
                {
                    m_GProxy = true;
                    m_Socket->PutBytes( m_Game->m_GHost->m_GPSProtocol->SEND_GPSS_INIT( m_Game->m_GHost->m_ReconnectPort, m_PID, m_GProxyReconnectKey, m_Game->GetGProxyEmptyActions( ) ) );
                    //CONSOLE_Print( "[GAME: " + m_Game->GetGameName( ) + "] player [" + m_Name + "] is using GProxy++" );
                }
                else
                {
                    // todotodo: send notice that we're not permitting reconnects
                    // note: GProxy++ should never send a GPS_INIT message if bot_reconnect = 0 because we don't advertise the game with invalid map dimensions
                    // but it would be nice to cover this case anyway
                }
            }
            else if( Packet->GetID( ) == CGPSProtocol :: GPS_RECONNECT )
            {
                // this is handled in ghost.cpp
            }
            else if( Packet->GetID( ) == CGPSProtocol :: GPS_ACK && Data.size( ) == 8 )
            {
                uint32_t LastPacket = UTIL_ByteArrayToUInt32( Data, false, 4 );
                uint32_t PacketsAlreadyUnqueued = m_TotalPacketsSent - m_GProxyBuffer.size( );

                if( LastPacket > PacketsAlreadyUnqueued )
                {
                    uint32_t PacketsToUnqueue = LastPacket - PacketsAlreadyUnqueued;

                    if( PacketsToUnqueue > m_GProxyBuffer.size( ) )
                        PacketsToUnqueue = m_GProxyBuffer.size( );

                    while( PacketsToUnqueue > 0 )
                    {
                        m_GProxyBuffer.pop( );
                        --PacketsToUnqueue;
                    }
                }
            }
        }

        delete Packet;
    }
}

void CGamePlayer :: Send( BYTEARRAY data )
{
    // must start counting packet total from beginning of connection
    // but we can avoid buffering packets until we know the client is using GProxy++ since that'll be determined before the game starts
    // this prevents us from buffering packets for non-GProxy++ clients

    ++m_TotalPacketsSent;

    if( m_GProxy && m_Game->GetGameLoaded( ) )
        m_GProxyBuffer.push( data );

    CPotentialPlayer :: Send( data );
}

void CGamePlayer :: EventGProxyReconnect( CTCPSocket *NewSocket, uint32_t LastPacket )
{
    delete m_Socket;
    m_Socket = NewSocket;
    m_Socket->PutBytes( m_Game->m_GHost->m_GPSProtocol->SEND_GPSS_RECONNECT( m_TotalPacketsReceived ) );

    uint32_t PacketsAlreadyUnqueued = m_TotalPacketsSent - m_GProxyBuffer.size( );

    if( LastPacket > PacketsAlreadyUnqueued )
    {
        uint32_t PacketsToUnqueue = LastPacket - PacketsAlreadyUnqueued;

        if( PacketsToUnqueue > m_GProxyBuffer.size( ) )
            PacketsToUnqueue = m_GProxyBuffer.size( );

        while( PacketsToUnqueue > 0 )
        {
            m_GProxyBuffer.pop( );
            --PacketsToUnqueue;
        }
    }

    // send remaining packets from buffer, preserve buffer

    queue<BYTEARRAY> TempBuffer;

    while( !m_GProxyBuffer.empty( ) )
    {
        m_Socket->PutBytes( m_GProxyBuffer.front( ) );
        TempBuffer.push( m_GProxyBuffer.front( ) );
        m_GProxyBuffer.pop( );
    }

    m_GProxyBuffer = TempBuffer;
    m_GProxyDisconnectNoticeSent = false;
    m_Game->SendAllChat( m_Game->m_GHost->m_Language->PlayerReconnectedWithGProxy( m_Name ) );
	m_CachedIP = m_Socket->GetIPString();
}
