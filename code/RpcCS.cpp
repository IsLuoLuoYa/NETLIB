#include "RpcCS.h"
#include "Log.h"
#include "Socket.h"
#include  <memory>
#include <future>

CRpcServer::CRpcServer(int HeartBeatTime, int ServiceMaxPeoples, int DisposeThreadNums) :
	CServiceNoBlock(HeartBeatTime, ServiceMaxPeoples, DisposeThreadNums)
{

}

CRpcServer::~CRpcServer()
{

}

void CRpcServer::MfStart(const char* ip, unsigned short port, int threadNums)
{
	MdThreadPool.MfStart(threadNums+1);						// �̳߳�����
	MdThreadPool.MfEnqueue(&CRpcServer::MfCallRetrun, this);// ����Ӧ�ͻ���д�ؽ�����̶߳����̳߳�ִ��
	return CServiceNoBlock::Mf_NoBlock_Start(ip, port);		// �����շ���������
}

int CRpcServer::MfRegCall(int CallNo, RemoteProc Call, int ArgLen, int RetLen)
{
	{
		std::lock_guard<std::shared_mutex> write_lock(MdCallListMutex);
		if (MdCallList.find(CallNo) != MdCallList.end())
		{
			printf("RempteProcReg CallNo <%d> already existed!\n", CallNo);
			LogFormatMsgAndSubmit(std::this_thread::get_id(), ERROR_FairySun, "RempteProcReg CallNo <%d> already existed!\n", CallNo);
			return -1;
		}
		MdCallList[CallNo] = std::make_tuple(Call, ArgLen, RetLen);
	}
	return 0;
}

int CRpcServer::MfRemoveCall(int CallNo)
{
	{
		std::lock_guard<std::shared_mutex> write_lock(MdCallListMutex);
		auto it = MdCallList.find(CallNo);
		if (it != MdCallList.end())
			MdCallList.erase(it);
	}
	return 0;
}

void CRpcServer::MfCallRetrun()
{
	while (!MdProcRet.empty() || !MdThreadPool.MfIsStop())
	{
		RpcMsg ret;
		// forѭ���� ִ����ɵ�Զ�̵��� ��CSocketObj*�ᱻ����ö��У�������ͳһ��MdProcRet���Ƴ�
		std::vector<CSocketObj*> removelist;

		// ����MdProcRet�������һ�����̿���ȡ�ý�����Ͱѽ�����ض�Ӧ��socket
		for (auto it = MdProcRet.begin(); it != MdProcRet.end(); ++it)
		{
			if (std::future_status::ready == std::get<1>(it->second).wait_for(std::chrono::milliseconds(1)))		// ����������
			{
				int procNo = std::get<0>(it->second);
				int procRetSize = std::get<2>(MdCallList[procNo]);
				ret.MdLen = (int)(sizeof(CNetMsgHead) + procRetSize);
				ret.MdCmd = procNo;
				std::shared_ptr<char[]> data = std::get<1>(it->second).get();
				it->first->MfDataToBuffer((char*)&ret, sizeof(CNetMsgHead));	// ��д��ͷ
				if(nullptr != data.get())
					it->first->MfDataToBuffer(data.get(), procRetSize);				// ��д����
				removelist.push_back(it->first);
			}
		}

		// ��ִ����ɵ�Զ�̵���ͳһ�Ƴ�
		{
			std::unique_lock<std::shared_mutex> write_lock(MdProcRetMutex);
			for (auto it = removelist.begin(); it != removelist.end(); ++it)
			{
				if (MdProcRet.find(*it) != MdProcRet.end())
					MdProcRet.erase(*it);
			}
		}
				
		std::this_thread::sleep_for(std::chrono::seconds(1));	// ��ͣһ���ִֹ�й���
	}
}

void CRpcServer::MfVNetMsgDisposeFun(SOCKET sock, CSocketObj* cli, CNetMsgHead* msg, std::thread::id& threadid)
{
	// ע���б���û���ҵ���Ӧ�Ĺ��̺ţ�����һ��Ϊ0����Ϣ����ʾ�Ҳ�����Ӧ����
	int flag = false;
	RpcMsg ret;
	{
		std::shared_lock<std::shared_mutex> read_lock(MdCallListMutex);
		if (MdCallList.find(msg->MdCmd) != MdCallList.end())
			flag = true;
	}
	if (flag == false)
	{
		ret.MdLen = sizeof(RpcMsg);
		ret.MdCmd = RpcProc_NOON;
		cli->MfDataToBuffer((char*)&ret, ret.MdLen);
	}
	else		// ��������ҵ��˶�Ӧ�Ĺ���,����ͻ��˶���Ͷ�Ӧ��future���Ž������
	{
		int arglen = std::get<1>(MdCallList[msg->MdCmd]);
		std::shared_ptr<char[]> buf(new char[arglen]);
		memcpy(buf.get(), ((char*)msg) + sizeof(CNetMsgHead), arglen);
		{
			std::unique_lock<std::shared_mutex> write_lock(MdProcRetMutex);
			MdProcRet[cli] = std::make_tuple
			(
				msg->MdCmd,
				MdThreadPool.MfEnqueue(std::get<0>(MdCallList[msg->MdCmd]), buf)
			);
		}
	}
}

CRpcClient::CRpcClient():
	CClientLinkManage()
{

}

CRpcClient::~CRpcClient()
{

}

void CRpcClient::MfStart()
{
	CClientLinkManage::MfStart();
}

int CRpcClient::MfConnectRpcServer(std::string Linkname, const char* ip, unsigned short port)
{
	return CClientLinkManage::MfCreateAddLink(Linkname, ip, port);
}

void CRpcClient::MfCloseRpclink(std::string Linkname)
{
	CClientLinkManage::MfCloseLink(Linkname);
}

std::shared_ptr<char[]> CRpcClient::MfRemote(std::string Linkname, int CallNo, void* data, int DataSize)
{
	RpcMsg msg;
	msg.MdLen = (int)(sizeof(CNetMsgHead) + DataSize);
	msg.MdCmd = CallNo;
	CClientLinkManage::MfSendData(Linkname, (char*)&msg, sizeof(CNetMsgHead));	// ��д��ͷ
	CClientLinkManage::MfSendData(Linkname, (char*)data, DataSize);				// ��д����	
	while (1)
	{
		if (!CClientLinkManage::MfHasMsg(Linkname))		// ���û������
		{
			std::this_thread::sleep_for(std::chrono::seconds(1));
			continue;
		}
		else
		{
			const char * buff = CClientLinkManage::MfGetRecvBufferP(Linkname);
			if (  ((RpcMsg*)buff)->MdCmd == RpcProc_NOON  )						// �յ���Ϊ0��callno
				return nullptr;
			else if (  ((RpcMsg*)buff)->MdCmd != CallNo  )						// �յ���callno�ͷ���ȥ��callno��һ��
				return nullptr;
			else
			{
				int retsize = (int)(((RpcMsg*)buff)->MdLen - sizeof(CNetMsgHead));		// ���㷵�ص����ݳ���
				char* ret = new char[retsize];									// ����ռ�
				memcpy(ret, ((char*)buff) + sizeof(CNetMsgHead) , retsize);			// ���Ƶ�������Ŀռ�
				CClientLinkManage::MfPopFrontMsg(Linkname);						// ����������Ϣ����
				return std::shared_ptr<char[]>(ret);							// ת��������ָ�뷵��
			}
		}
	}
}

void CRpcClient::MfVNetMsgDisposeFun(SOCKET sock, CSocketObj* cli, CNetMsgHead* msg, std::thread::id& threadid)
{

}

