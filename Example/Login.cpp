#include "Login.h"
#include "../ApplyProto.h"
#include "../static_base/Log.h"
#include "../static_base/Socket.h"
#include <string.h>


CLogInServer::CLogInServer()
{
	MdTp.MfStart();
	LogFormatMsgAndSubmit(std::this_thread::get_id(), INFO_FairySun, "LogInServer data base link start");
}

CLogInServer::~CLogInServer()
{
	mysql_close(&MdMysql);
	LogFormatMsgAndSubmit(std::this_thread::get_id(), INFO_FairySun, "LogInServer data base link release" );
}

void CLogInServer::MfStart(const char* ip, unsigned short port, const char* DataBaseIp, const char* DataBaseUser, const char* DataBasePass, const char* DataBaseName, const char* ip2, unsigned short port2, const char* ip3, unsigned short port3)
{
	CServiceNoBlock::Mf_NoBlock_Start(ip, port);
	MdRpcServer.MfStart(ip2, port2);
	MdRpcServer.MfRegCall(RpcServerList, std::bind(&CLogInServer::MfRegServerAddr, this, std::placeholders::_1), sizeof(CMyNetAddr), 0);

	mysql_init(&MdMysql);

	// ���ӳɹ�
	if (&MdMysql == mysql_real_connect(&MdMysql, DataBaseIp, DataBaseUser, DataBasePass, DataBaseName, 0, NULL, 0))
	{
		LogFormatMsgAndSubmit(std::this_thread::get_id(), INFO_FairySun, "LogInServer data base <%s> connect success", DataBaseName);
		bool a = 1;
		mysql_options(&MdMysql, MYSQL_OPT_RECONNECT, (const void*)&a);		// ���ÿ��Զ�����
		
		// ���������ַ�
		mysql_query(&MdMysql, "set names utf8");
	}
	else	// ����ʧ��
	{
		LogFormatMsgAndSubmit(std::this_thread::get_id(), FATAL_FairySun, "LogInServer data base <%s> connect fail", DataBaseName);
		ErrorWaitExit();
	}

	LogFormatMsgAndSubmit(std::this_thread::get_id(), FATAL_FairySun, "connect Rpc Servering...<%s:%d>", ip3, port3);
	MdRpcClient.MfStart();
	int i = 0;
	while (SOCKET_ERROR == MdRpcClient.MfConnectRpcServer(MdJoinHall, ip3, port3))
	{
		std::this_thread::sleep_for(std::chrono::seconds(5));
		if (i++ > 3)
		{
			LogFormatMsgAndSubmit(std::this_thread::get_id(), FATAL_FairySun, "connect Rpc Server<%s> fail", MdJoinHall.c_str());
			//printf("connect Rpc Server<%s> fail", MdJoinHall.c_str());
			ErrorWaitExit();
		}
	}
		
}

void CLogInServer::MfVNetMsgDisposeFun(SOCKET sock, CSocketObj* cli, CNetMsgHead* msg, std::thread::id& threadid)
{
	switch (msg->MdCmd)
	{
	case CMD_Login:
		Mf_Login(sock, cli, msg, threadid);
		break;
			
	case CMD_PullServerListToClent:
		Mf_PullServerListToClent(sock, cli, msg, threadid);
		break;
	}
}


std::shared_ptr<char[]> CLogInServer::MfRegServerAddr(std::shared_ptr<char[]> a)
{
	CMyNetAddr temp;
	CMyNetAddr* ClientAddr = (CMyNetAddr*)(a.get()); 
	temp.MdServerNameSize = ClientAddr->MdServerNameSize;
	temp.MdIpSize = ClientAddr->MdIpSize;
	strncpy(temp.MdServerName, ClientAddr->MdServerName, temp.MdServerNameSize);
	strncpy(temp.MdIp, ClientAddr->MdIp, temp.MdIpSize);
	temp.MdPort = ClientAddr->MdPort;
	{
		std::lock_guard<std::mutex> lock(MdServerListMtx);
		MdServerList[std::string(temp.MdServerName)] = temp;
	}
	LogFormatMsgAndSubmit(std::this_thread::get_id(), INFO_FairySun, "ServerListReg name:<%s> addr:<%s%d>", temp.MdServerName, temp.MdIp, temp.MdPort);
	return nullptr;
}

void CLogInServer::Mf_Login(SOCKET sock, CSocketObj* cli, CNetMsgHead* msg, std::thread::id& threadid)
{
	mysql_ping(&MdMysql);			// ������ݿ�Ͽ���������
	std::string sql("select * from tab_usr where account='");
	CLogin_Reply_Msg ret;
	CLogin_Msg* mymsg = (CLogin_Msg*)msg;
	// ������Ϣ��һЩ����������
	ret.MdAccountSize = mymsg->MdAccountSize;
	strncpy(ret.MdAccount, mymsg->MdAccount, ret.MdAccountSize);
	ret.MdResult = 0;

	sql += std::string(mymsg->MdAccount);
	sql += "';";
	if (0 == mysql_query(&MdMysql, sql.c_str()))	// ��ѯ�Ƿ�ɹ�
	{
		MYSQL_RES* result = mysql_store_result(&MdMysql);
		if (1 == mysql_num_rows(result))			// �ж��Ƿ�������˺�
		{
			MYSQL_ROW row = mysql_fetch_row(result);
			// ��¼��Ϣ
			if (0 == strncmp(row[2], mymsg->MdPassWord, 20))
			{
				LogFormatMsgAndSubmit(std::this_thread::get_id(), INFO_FairySun, "account <%s> login success", mymsg->MdAccount);
				strncpy(ret.MdName, row[3], 37);
				//printf("account<%d>:%s password<%d>:%s\n", mymsg->MdAccountSize, mymsg->MdAccount, mymsg->MdpasswordSize, mymsg->MdPassWord);
				ret.MdResult = 1;	// ��ʾ�ɹ�

				CAccount temp;
				strncpy((char*)&temp, mymsg->MdAccount, 20);
				//MdRpcClient.MfRemote(MdJoinHall, RpcJoinHall, (void*)(&temp), sizeof(CAccount));
				MdTp.MfEnqueue(&CRpcClient::MfRemote, &MdRpcClient, MdJoinHall, RpcJoinHall, (void*)(&temp), sizeof(CAccount));
			}
			else
			{
				LogFormatMsgAndSubmit(std::this_thread::get_id(), INFO_FairySun, "account<%s>: password error", mymsg->MdAccount);
				ret.MdResult = 2;
			}
		}
		else		// ���ø��˺Ų�����
			ret.MdResult = 3;
		mysql_free_result(result);		// �ͷŽ����
	}
	cli->MfDataToBuffer((const char*)&ret, ret.MdLen);		//������Ϣ
}

void CLogInServer::Mf_PullServerListToClent(SOCKET sock, CSocketObj* cli, CNetMsgHead* msg, std::thread::id& threadid)
{
	CPullServerListToClent_Reply_Msg ret;
	{
		std::lock_guard<std::mutex> lock(MdServerListMtx);
		ret.MdSums = MdServerList.size();
		if (0 == ret.MdSums)
		{
			ret.MdCurrent = 0;
			cli->MfDataToBuffer((const char*)&ret, ret.MdLen);
		}
		int i = 1;
		for (auto it : MdServerList)
		{
			ret.MdCurrent = i++;
			memcpy(&(ret.MdAddr), &(it.second), sizeof(CMyNetAddr));
			cli->MfDataToBuffer((const char*)&ret, ret.MdLen);
		}
	}
}