#include "MessageIdentifiers.h"
#include "RakPeerInterface.h"
#include "BitStream.h"
#include <thread>
#include <chrono>
#include <iostream>
#include <map>
#include <mutex>

static unsigned int SERVER_PORT = 65000;
static unsigned int CLIENT_PORT = 65001;
static unsigned int MAX_CONNECTIONS = 3;

enum NetworkState
{
	NS_Init = 0,
	NS_PendingStart,
	NS_Started,
	NS_Lobby,
	NS_Pending,
	NS_CharacterSelection,
	NS_PREGAME,
	NS_GAME,
	NS_SERVER_GAME,
};

bool isServer = false;
bool isRunning = true;
bool Turn = false;

int areReady = 0;
int playCount = 1;
int currentTurn = 1;

RakNet::RakPeerInterface *g_rakPeerInterface = nullptr;
RakNet::SystemAddress g_serverAddress;

std::mutex g_networkState_mutex;
NetworkState g_networkState = NS_Init;

enum {
	ID_THEGAME_LOBBY_READY = ID_USER_PACKET_ENUM,
	ID_PLAYER_READY,
	ID_PLAYER_IN_LOBBY,
	ID_THEGAME_LOBBY_READY_ALL,
	ID_PLAYER_CLASS,
	ID_PLAYER_CHARACTER,
	ID_THEGAME_START,
	ID_TURN,
	ID_NOT_TURN,
	ID_PLAYER_HEALTH_S,
	ID_PLAYER_HEALTH,
	ID_PLAYER_HEALTH_P,
	ID_PLAYER_ATTACK,
	ID_PLAYER_HEAL,
	ID_PLAYER_HEAL_C,
	ID_PLAYER_HEAL_P,
	ID_PLAYER_ATTACK_C,
	ID_PLAYER_ATTACK_P,
};

enum PlayerClass {
	empty = 0,
	Mage,
	Rogue,
	Warrior,
};

struct SPlayer
{
	std::string name;
	RakNet::SystemAddress address;
	PlayerClass P_class;
	float health;
	float attack;
	float heal;
	int Player;
};

std::map<unsigned long, SPlayer> m_players;

//server
void OnIncomingConnection(RakNet::Packet* packet)
{
	//must be server in order to recieve connection
	assert(isServer);
	m_players.insert(std::make_pair(RakNet::RakNetGUID::ToUint32(packet->guid), SPlayer()));

	std::cout << "Total Players: " << m_players.size() << std::endl;
}

//client
void OnConnectionAccepted(RakNet::Packet* packet)
{
	//server should not ne connecting to anybody, 
	//clients connect to server
	assert(!isServer);

	g_networkState = NS_Lobby;
	g_serverAddress = packet->systemAddress;
}
//server
void TurnCycle() {
	for (std::map<unsigned long, SPlayer>::iterator it = m_players.begin(); it != m_players.end(); ++it) {
		if (it->second.Player == currentTurn) {
			RakNet::BitStream bs;
			bs.Write((RakNet::MessageID)ID_TURN);
			bool Pturn = true;
			bs.Write(Pturn);

			g_rakPeerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, it->second.address, false);

			RakNet::BitStream wbs;
			wbs.Write((RakNet::MessageID)ID_NOT_TURN);
			bool OPturn = false;
			wbs.Write(OPturn);
			RakNet::RakString name = it->second.name.c_str();
			wbs.Write(name);

			g_rakPeerInterface->Send(&wbs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, it->second.address, true);
		}
	}
}
//server
void PlayerTurnAttack(RakNet::Packet* packet) {
	unsigned long guid = RakNet::RakNetGUID::ToUint32(packet->guid);
	std::map<unsigned long, SPlayer>::iterator it = m_players.find(guid);
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString name;
	bs.Read(name);
	
	std::string userName = name;
	std::cout << "Attack" << std::endl;
	
	SPlayer& player = it->second;

	for (std::map<unsigned long, SPlayer>::iterator it = m_players.begin(); it != m_players.end(); ++it) {
		if (userName == it->second.name) {

			it->second.health = it->second.health - player.attack;
		}
	}

	RakNet::BitStream wbs;
	wbs.Write((RakNet::MessageID)ID_PLAYER_ATTACK_C);
	RakNet::RakString Ename = player.name.c_str();
	wbs.Write(Ename);
	PlayerClass Eclass = player.P_class;
	wbs.Write(Eclass);
	int Phealth = player.health;
	wbs.Write(Phealth);

	for (auto const& x : m_players)
	{
		if (guid == x.first) {
			RakNet::BitStream bs;
			bs.Write((RakNet::MessageID)ID_PLAYER_ATTACK_P);
			int Phealth = player.health;
			bs.Write(Phealth);

			g_rakPeerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, x.second.address, false);
			continue;
		}
		g_rakPeerInterface->Send(&wbs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, x.second.address, false);
	}

	currentTurn++;
	TurnCycle();
	if (currentTurn == 3) {
		currentTurn = 0;
	}
}
//server
void PlayerTurnHeal(RakNet::Packet* packet) {
	unsigned long guid = RakNet::RakNetGUID::ToUint32(packet->guid);
	std::map<unsigned long, SPlayer>::iterator it = m_players.find(guid);

	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);

	std::cout << "Heal" << std::endl;

	SPlayer& player = it->second;

	if (player.P_class == Warrior) {
		if (player.health >= 17) {
			player.health = 20;
		}
		else {
			player.health = player.health + player.heal;
		}
	}
	if (player.P_class == Rogue) {
		if (player.health >= 14) {
			player.health = 15;
		}
		else {
			player.health = player.health + player.heal;
		}
	}
	if (player.P_class == Mage) {
		if (player.health >= 5) {
			player.health = 10;
		}
		else {
			player.health = player.health + player.heal;
		}
	}
	
	RakNet::BitStream wbs;
	wbs.Write((RakNet::MessageID)ID_PLAYER_HEAL_C);
	RakNet::RakString name = player.name.c_str();
	wbs.Write(name);
	PlayerClass Pclass = player.P_class;
	wbs.Write(Pclass);
	int Phealth = player.health;
	wbs.Write(Phealth);

	for (auto const& x : m_players)
	{
		if (guid == x.first) {
			RakNet::BitStream bs;
			bs.Write((RakNet::MessageID)ID_PLAYER_HEAL_P);
			int Phealth = player.health;
			bs.Write(Phealth);

			g_rakPeerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, x.second.address, false);
			continue;
		}
		g_rakPeerInterface->Send(&wbs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, x.second.address, false);
	}
	
	currentTurn++;
	TurnCycle();
	if (currentTurn == 3) {
		currentTurn = 0;
	}
}
//client
void PlayerHealMsg(RakNet::Packet* packet) {
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString userName;
	bs.Read(userName);
	PlayerClass Pclass;
	bs.Read(Pclass);
	int health;
	bs.Read(health);

	if (Pclass == Warrior) {
		std::cout << userName << " the warrior used heal. Health: " << health << std::endl;
	}
	if (Pclass == Rogue) {
		std::cout << userName << " the rogue used heal. Health: " << health << std::endl;
	}
	if (Pclass == Mage) {
		std::cout << userName << " the mage used heal. Health: " << health << std::endl;
	}
}
//client
void PlayerHealMsgP(RakNet::Packet* packet) {
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	int health;
	bs.Read(health);

	std::cout << "You used heal. Health: " << health << std::endl;
}
//client
void PlayerAttackMsg(RakNet::Packet* packet) {

}
void PlayerAttackMsgP(RakNet::Packet* packet) {

}
//server
void OnLobbyReady(RakNet::Packet* packet)
{
	unsigned long guid = RakNet::RakNetGUID::ToUint32(packet->guid);
	std::map<unsigned long, SPlayer>::iterator it = m_players.find(guid);
	//somehow player didn't connect but now is in lobby ready
	assert(it != m_players.end());

	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString userName;
	bs.Read(userName);

	SPlayer& player = it->second;
	player.name = userName;
	player.address = packet->systemAddress;
	player.Player = playCount;
	playCount++;
	std::cout << player.name.c_str() << " is ready!" << std::endl;
	for (std::map<unsigned long, SPlayer>::iterator it = m_players.begin(); it != m_players.end(); ++it) {

		if (guid == it->first) {
			continue;
		}
		SPlayer& player = it->second;
		RakNet::BitStream wbs;
		wbs.Write((RakNet::MessageID)ID_PLAYER_IN_LOBBY);
		RakNet::RakString name(player.name.c_str());
		wbs.Write(name);

		g_rakPeerInterface->Send(&wbs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false);
	}
	RakNet::BitStream wbs;
	wbs.Write((RakNet::MessageID)ID_PLAYER_READY);
	RakNet::RakString name(player.name.c_str());
	wbs.Write(name);
	g_rakPeerInterface->Send(&wbs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, true);

	areReady++;
	if (areReady == 3) {
			NetworkState ns = NS_CharacterSelection;
			RakNet::BitStream bs;
			bs.Write((RakNet::MessageID)ID_THEGAME_LOBBY_READY_ALL);
			bs.Write(ns);

			g_rakPeerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, true);
			areReady = 0;
	}
}
//server
void OnClassSelect(RakNet::Packet* packet) {
	unsigned long guid = RakNet::RakNetGUID::ToUint32(packet->guid);
	std::map<unsigned long, SPlayer>::iterator it = m_players.find(guid);

	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString Pclass;
	bs.Read(Pclass);

	SPlayer& player = it->second;
	if (strcmp(Pclass, "mage") == 0) {
		player.P_class = Mage;
		player.health = 10.0f;
		player.attack = 5.0f;
		player.heal = 6.0f;
		std::cout << player.name.c_str() << " The Mage is ready!" << std::endl;
	}
	else if (strcmp(Pclass, "warrior") == 0) {
		player.P_class = Warrior;
		player.health = 20.0f;
		player.attack = 4.0f;
		player.heal = 4.0f;
		std::cout << player.name.c_str() << " The Warrior is ready!" << std::endl;
	}
	else if (strcmp(Pclass, "rogue") == 0) {
		player.P_class = Rogue;
		player.health = 15.0f;
		player.attack = 8.0f;
		player.heal = 2.0f;
		std::cout << player.name.c_str() << " The Rogue is ready!" << std::endl;
	}
	areReady++;
	if (areReady == 3) {

		for (std::map<unsigned long, SPlayer>::iterator it = m_players.begin(); it != m_players.end(); ++it) {

			SPlayer& player = it->second;
			RakNet::BitStream wbs2;
			wbs2.Write((RakNet::MessageID)ID_PLAYER_CHARACTER);
			RakNet::RakString name(player.name.c_str());
			wbs2.Write(name);
			PlayerClass PClass = player.P_class;
			wbs2.Write(PClass);
			for (auto const& x : m_players)
			{
				if (x.second.P_class == PClass) {
					continue;
				}
				g_rakPeerInterface->Send(&wbs2, HIGH_PRIORITY, RELIABLE_ORDERED, 0, x.second.address, false);
			}
		}

		NetworkState ns = NS_PREGAME;
		RakNet::BitStream bs;
		bs.Write((RakNet::MessageID)ID_THEGAME_LOBBY_READY_ALL);
		bs.Write(ns);

		g_rakPeerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, true);
		g_networkState = NS_SERVER_GAME;
		std::this_thread::sleep_for(std::chrono::seconds(5));
		TurnCycle();
		
	}
	
}
//client
void OnClassReady(RakNet::Packet* packet) {
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString userName;
	bs.Read(userName);
	PlayerClass Pclass;
	bs.Read(Pclass);

	if (Pclass == Warrior) {
		std::cout << userName << " picked warrior." << std::endl;
	}
	else if (Pclass == Rogue) {
		std::cout << userName << " picked rogue." << std::endl;
	}
	else if (Pclass == Mage) {
		std::cout << userName << " picked mage." << std::endl;
	}
}
//client
void OnLobbyReadyAll(RakNet::Packet* packet) {
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	NetworkState ns;
	bs.Read(ns);

	g_networkState = ns;
}
//client
void DisplayPlayerReady(RakNet::Packet* packet) {
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString userName;
	bs.Read(userName);

	std::cout << userName << " Has Joined." << std::endl;
}
//client
void DisplayPlayerInLobby(RakNet::Packet* packet) {
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString userName;
	bs.Read(userName);

	std::cout << userName << " is in the lobby." << std::endl;
}

//client
void OnPlayerTurn(RakNet::Packet* packet) {
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	bool Pturn;
	bs.Read(Pturn);

	Turn = Pturn;

	std::cout << std::endl << "It's your turn." << std::endl << "Enter your command." << std::endl;
}
//client
void OnNotPlayerTurn(RakNet::Packet* packet) {
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	bool Pturn;
	bs.Read(Pturn);
	RakNet::RakString userName;
	bs.Read(userName);

	Turn = Pturn;

	std::cout << std::endl << "It's " << userName << "'s turn." << std::endl;
}
//server
void DisplayHealthServer(RakNet::Packet* packet) {
	unsigned long guid = RakNet::RakNetGUID::ToUint32(packet->guid);
	std::map<unsigned long, SPlayer>::iterator it = m_players.find(guid);

	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);

	for (std::map<unsigned long, SPlayer>::iterator it = m_players.begin(); it != m_players.end(); ++it) {

		SPlayer& player = it->second;

		if (guid == it->first) {
			RakNet::BitStream wbs2;
			wbs2.Write((RakNet::MessageID)ID_PLAYER_HEALTH_P);
			float health = player.health;
			wbs2.Write(health);
			g_rakPeerInterface->Send(&wbs2, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false);
			continue;
		}

		RakNet::BitStream wbs2;
		wbs2.Write((RakNet::MessageID)ID_PLAYER_HEALTH);
		RakNet::RakString name(player.name.c_str());
		wbs2.Write(name);
		float health = player.health;
		wbs2.Write(health);
		g_rakPeerInterface->Send(&wbs2, HIGH_PRIORITY, RELIABLE_ORDERED, 0, packet->systemAddress, false);
	}
}
//client
void DisplayHealth(RakNet::Packet* packet) {
	

	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	RakNet::RakString userName;
	bs.Read(userName);
	float health;
	bs.Read(health);

	std::cout << userName << "'s health: " << health << std::endl;
}
//client
void DisplayHealthP(RakNet::Packet* packet) {
	RakNet::BitStream bs(packet->data, packet->length, false);
	RakNet::MessageID messageId;
	bs.Read(messageId);
	float health;
	bs.Read(health);

	std::cout << "Your health: " << health << std::endl;
}

unsigned char GetPacketIdentifier(RakNet::Packet *packet)
{
	if (packet == nullptr)
		return 255;

	if ((unsigned char)packet->data[0] == ID_TIMESTAMP)
	{
		RakAssert(packet->length > sizeof(RakNet::MessageID) + sizeof(RakNet::Time));
		return (unsigned char)packet->data[sizeof(RakNet::MessageID) + sizeof(RakNet::Time)];
	}
	else
		return (unsigned char)packet->data[0];
}


void InputHandler()
{
	while (isRunning)
	{
		char userInput[255];
		char userInput2[255];
		if (g_networkState == NS_Init)
		{
			std::cout << "press (s) for server (c) for client" << std::endl;
			std::cin >> userInput;
			isServer = (userInput[0] == 's');
			g_networkState = NS_PendingStart;
		}
		else if (g_networkState == NS_Lobby)
		{
			std::cout << std::endl << "Enter your name to play or type quit to leave" << std::endl;
			std::cin >> userInput;
			//quitting is not acceptable in our game, create a crash to teach lesson
			assert(strcmp(userInput, "quit"));

			RakNet::BitStream bs;
			bs.Write((RakNet::MessageID)ID_THEGAME_LOBBY_READY);
			RakNet::RakString name(userInput);
			bs.Write(name);

			std::cout << std::endl << "Enter 'ready' when you are prepared to start" << std::endl;
			std::cin >> userInput;
			if (strcmp(userInput, "ready") == 0) {
				std::cout << std::endl << "Waiting for other players to ready up..." << std::endl << std::endl;
				g_networkState = NS_Pending;
				g_rakPeerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false);
			}
			
		}
		else if (g_networkState == NS_CharacterSelection)
		{
			std::cout << std::endl << "Select your class:" << std::endl
			<< "'warrior' Health: ***  Attack: *  Heal: **" << std::endl
			<< "'rogue' Health: **  Attack: ***  Heal: *" << std::endl
			<< "'mage' Health: *  Attack: **  Heal: ***" << std::endl;
			std::cin >> userInput;

			if (strcmp(userInput, "warrior") == 0 || strcmp(userInput, "rogue") == 0 || strcmp(userInput, "mage") == 0) {
				RakNet::BitStream bs;
				bs.Write((RakNet::MessageID)ID_PLAYER_CLASS);
				RakNet::RakString Pclass(userInput);
				bs.Write(Pclass);

				
				std::cout << std::endl << "Waiting for other players to select their class..." << std::endl;
				g_networkState = NS_Pending;
				g_rakPeerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false);
			}
			

		}
		else if (g_networkState == NS_PREGAME) {
			std::cout << std::endl << "When it is your turn you may use the following commands:"
			<< std::endl << "Type 'attack', you will be prompted for your target." 
			<< std::endl << "Type 'heal' to restore some of you health." << std::endl;
			std::cout << "Type 'stat' at any time for Player health" << std::endl;
			std::cout << "Let the fight begin!!!" << std::endl;
			g_networkState = NS_GAME;
		}
		else if (g_networkState == NS_GAME) {
			std::cin >> userInput;
			if (Turn == false) {
				if (strcmp(userInput, "stat") == 0) {
					RakNet::BitStream bs;
					bs.Write((RakNet::MessageID)ID_PLAYER_HEALTH_S);

					g_rakPeerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false);
				}
				if (strcmp(userInput, "attack") == 0) {
					std::cout << "It's not your turn, wait for your turn to attack." << std::endl;
				}
				if (strcmp(userInput, "heal") == 0) {
					std::cout << "It's not your turn, wait for your turn to heal." << std::endl;
				}
			}
			if (Turn == true) {
				if (strcmp(userInput, "stat") == 0) {
					RakNet::BitStream bs;
					bs.Write((RakNet::MessageID)ID_PLAYER_HEALTH_S);

					g_rakPeerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false);
				}
				if (strcmp(userInput, "attack") == 0) {
					std::cout << "Enter the name of the player you want to attack" << std::endl;
					std::cin >> userInput2;
					RakNet::BitStream bs;
					bs.Write((RakNet::MessageID)ID_PLAYER_ATTACK);
					RakNet::RakString name = userInput2;
					bs.Write(name);

					g_rakPeerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false);
				}
				if (strcmp(userInput, "heal") == 0) {
					RakNet::BitStream bs;
					bs.Write((RakNet::MessageID)ID_PLAYER_HEAL);

					g_rakPeerInterface->Send(&bs, HIGH_PRIORITY, RELIABLE_ORDERED, 0, g_serverAddress, false);
				}
			}
		}
		std::this_thread::sleep_for(std::chrono::microseconds(100));
	}
}

bool HandleLowLevelPackets(RakNet::Packet* packet)
{
	bool isHandled = true;
	// We got a packet, get the identifier with our handy function
	unsigned char packetIdentifier = GetPacketIdentifier(packet);

	// Check if this is a network message packet
	switch (packetIdentifier)
	{
	case ID_DISCONNECTION_NOTIFICATION:
		// Connection lost normally
		printf("ID_DISCONNECTION_NOTIFICATION\n");
		break;
	case ID_ALREADY_CONNECTED:
		// Connection lost normally
		printf("ID_ALREADY_CONNECTED with guid %" PRINTF_64_BIT_MODIFIER "u\n", packet->guid);
		break;
	case ID_INCOMPATIBLE_PROTOCOL_VERSION:
		printf("ID_INCOMPATIBLE_PROTOCOL_VERSION\n");
		break;
	case ID_REMOTE_DISCONNECTION_NOTIFICATION: // Server telling the clients of another client disconnecting gracefully.  You can manually broadcast this in a peer to peer enviroment if you want.
		printf("ID_REMOTE_DISCONNECTION_NOTIFICATION\n");
		break;
	case ID_REMOTE_CONNECTION_LOST: // Server telling the clients of another client disconnecting forcefully.  You can manually broadcast this in a peer to peer enviroment if you want.
		printf("ID_REMOTE_CONNECTION_LOST\n");
		break;
	case ID_NEW_INCOMING_CONNECTION:
		OnIncomingConnection(packet);
		printf("ID_NEW_INCOMING_CONNECTION\n");
		break;
	case ID_REMOTE_NEW_INCOMING_CONNECTION: // Server telling the clients of another client connecting.  You can manually broadcast this in a peer to peer enviroment if you want.
		OnIncomingConnection(packet);
		printf("ID_REMOTE_NEW_INCOMING_CONNECTION\n");
		break;
	case ID_CONNECTION_BANNED: // Banned from this server
		printf("We are banned from this server.\n");
		break;
	case ID_CONNECTION_ATTEMPT_FAILED:
		printf("Connection attempt failed\n");
		break;
	case ID_NO_FREE_INCOMING_CONNECTIONS:
		// Sorry, the server is full.  I don't do anything here but
		// A real app should tell the user
		printf("ID_NO_FREE_INCOMING_CONNECTIONS\n");
		break;

	case ID_INVALID_PASSWORD:
		printf("ID_INVALID_PASSWORD\n");
		break;

	case ID_CONNECTION_LOST:
		// Couldn't deliver a reliable packet - i.e. the other system was abnormally
		// terminated
		printf("ID_CONNECTION_LOST\n");
		break;

	case ID_CONNECTION_REQUEST_ACCEPTED:
		// This tells the client they have connected
		printf("ID_CONNECTION_REQUEST_ACCEPTED to %s with GUID %s\n", packet->systemAddress.ToString(true), packet->guid.ToString());
		printf("My external address is %s\n", g_rakPeerInterface->GetExternalID(packet->systemAddress).ToString(true));
		OnConnectionAccepted(packet);
		break;
	case ID_CONNECTED_PING:
	case ID_UNCONNECTED_PING:
		printf("Ping from %s\n", packet->systemAddress.ToString(true));
		break;
	default:
		isHandled = false;
		break;
	}
	return isHandled;
}

void PacketHandler()
{
	while (isRunning)
	{
		for (RakNet::Packet* packet = g_rakPeerInterface->Receive(); packet != nullptr; g_rakPeerInterface->DeallocatePacket(packet), packet = g_rakPeerInterface->Receive())
		{
			if (!HandleLowLevelPackets(packet))
			{
				//our game specific packets
				unsigned char packetIdentifier = GetPacketIdentifier(packet);
				switch (packetIdentifier)
				{
				case ID_THEGAME_LOBBY_READY:
					OnLobbyReady(packet);
					break;
				case ID_THEGAME_LOBBY_READY_ALL:
					OnLobbyReadyAll(packet);
					break;
				case ID_PLAYER_READY:
					DisplayPlayerReady(packet);
					break;
				case ID_PLAYER_IN_LOBBY:
					DisplayPlayerInLobby(packet);
					break;
				case ID_PLAYER_CLASS:
					OnClassSelect(packet);
					break;
				case ID_PLAYER_CHARACTER:
					OnClassReady(packet);
					break;
				case ID_TURN:
					OnPlayerTurn(packet);
					break;
				case ID_NOT_TURN:
					OnNotPlayerTurn(packet);
					break;
				case ID_PLAYER_HEALTH_S:
					DisplayHealthServer(packet);
					break;
				case ID_PLAYER_HEALTH:
					DisplayHealth(packet);
					break;
				case ID_PLAYER_HEALTH_P:
					DisplayHealthP(packet);
					break;
				case ID_PLAYER_ATTACK:
					PlayerTurnAttack(packet);
					break;
				case ID_PLAYER_HEAL:
					PlayerTurnHeal(packet);
					break;
				case ID_PLAYER_HEAL_C:
					PlayerHealMsg(packet);
					break;
				case ID_PLAYER_HEAL_P:
					PlayerHealMsgP(packet);
					break;
				case ID_PLAYER_ATTACK_C:
					break;
					PlayerAttackMsg(packet);
				case ID_PLAYER_ATTACK_P:
					PlayerAttackMsgP(packet);
					break;
				default:
					break;
				}
			}
		}

		std::this_thread::sleep_for(std::chrono::microseconds(1000));
	}


}

int main()
{
	g_rakPeerInterface = RakNet::RakPeerInterface::GetInstance();

	std::thread inputHandler(InputHandler);
	std::thread packetHandler(PacketHandler);

	while (isRunning)
	{
		if (g_networkState == NS_PendingStart)
		{
			if (isServer)
			{
				RakNet::SocketDescriptor socketDescriptors[1];
				socketDescriptors[0].port = SERVER_PORT;
				socketDescriptors[0].socketFamily = AF_INET; // Test out IPV4

				bool isSuccess = g_rakPeerInterface->Startup(MAX_CONNECTIONS, socketDescriptors, 1) == RakNet::RAKNET_STARTED;
				assert(isSuccess);
				//ensures we are server
				g_rakPeerInterface->SetMaximumIncomingConnections(MAX_CONNECTIONS);
				std::cout << "server started" << std::endl;
				g_networkState = NS_Started;
			}
			//client
			else
			{
				RakNet::SocketDescriptor socketDescriptor(CLIENT_PORT, 0);
				socketDescriptor.socketFamily = AF_INET;

				while (RakNet::IRNS2_Berkley::IsPortInUse(socketDescriptor.port, socketDescriptor.hostAddress, socketDescriptor.socketFamily, SOCK_DGRAM) == true)
					socketDescriptor.port++;

				RakNet::StartupResult result = g_rakPeerInterface->Startup(8, &socketDescriptor, 1);
				assert(result == RakNet::RAKNET_STARTED);

				g_networkState = NS_Started;

				g_rakPeerInterface->SetOccasionalPing(true);
				//"127.0.0.1" = local host = your machines address
				RakNet::ConnectionAttemptResult car = g_rakPeerInterface->Connect("192.168.0.21", SERVER_PORT, nullptr, 0);
				RakAssert(car == RakNet::CONNECTION_ATTEMPT_STARTED);
				std::cout << "client attempted connection..." << std::endl;
				
			}
		}

		if (g_networkState == NS_SERVER_GAME) {
			
		}
	}

	//std::cout << "press q and then return to exit" << std::endl;
	//std::cin >> userInput;

	inputHandler.join();
	packetHandler.join();
	return 0;
}