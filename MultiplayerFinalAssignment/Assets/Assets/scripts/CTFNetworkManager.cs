using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Networking;
using UnityEngine.UI;



public class CTFNetworkManager : NetworkManager
{
	public Canvas start;
	public Button host;
	public Button join;

	public void StartupHost(){
		SetPort ();
		NetworkManager.singleton.StartHost ();
	}
	public void JoinGame(){
		SetPort ();
		NetworkManager.singleton.StartClient ();
	}
		
	void SetPort(){
		NetworkManager.singleton.networkPort = 7777;
	}
    public override void OnServerConnect(NetworkConnection conn)
    {
        base.OnServerConnect(conn);
        Debug.Log("OnServerConnect");
        Debug.Log("OnServerConnect NumPlayers" + this.numPlayers);
		start.enabled = false;
		host.enabled = false;
		join.enabled = false;
    }

    public override void OnClientConnect(NetworkConnection conn)
    {
        base.OnClientConnect(conn);
        Debug.Log("OnClientConnect NumPlayers" + this.numPlayers);
		start.enabled = false;
		host.enabled = false;
		join.enabled = false;
    }

    public override void OnClientDisconnect(NetworkConnection conn)
    {
        base.OnClientDisconnect(conn);
        Debug.Log("OnClientDisconnect NumPlayers" + this.numPlayers);
    }

    //public override void OnServerAddPlayer(NetworkConnection conn, short playerControllerId, NetworkReader extraMessageReader)
    //{
    //m_gameManager.ActivePlayers = this.numPlayers;
    //}
}
