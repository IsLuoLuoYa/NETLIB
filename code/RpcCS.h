#pragma once
#include <map>
#include "NetMsgStruct.h"
#include "Service.h"
#include "ClientLinkManage.h"
#include "ThreadPool.h"
// ����Զ�̵��õĺ���Ӧ����ѭ�˽ӿ�
// ����ֵ��һ������ָ�룬ָ��һ��char[]
// ������һ������ָ�룬ָ��һ��char[]
// ʹ������ָ�������ֱ��ʹ��char*������Ϊ�ܶ�ط����ǿ��̴߳��ݵģ������ڵ����е��ڴ����
// �����Ƿ���ֵ���ǲ���������void�Ļ������Զ���ṹ����
typedef std::function
<
	std::shared_ptr<char[]> (std::shared_ptr<char[]>)
> RemoteProc;

enum RpcNetMsgCmd
{
	RpcProc_NOON = 0	// ��CNetMsgHead::MdCmdΪ��ֵ��
	// �����Զ���Ĺ��̺Ŷ�Ӧ�ô���0
};

struct RpcMsg :public CNetMsgHead
{
	// CNetMsgHead::MdLen �ó�Ա���ɴ����������ĳ���
	// CNetMsgHead::MdCmd �ó�Ա���ٴ���ĳ������������ֱ�Ӵ���Ҫ���õ��Ǹ����̺�CallNo
	void* MdData;	// ����ָ��
					// ��server��,�յ��ýṹMdData��ʾ���������͸ýṹMdData��ʾ����ֵ
					// ��client��,�յ��ýṹMdData��ʾ����ֵ�����͸ýṹMdData��ʾ����
					// ����ʱ�����øó�Ա������ֱ��д���ݵ�������
					// �ӻ�����ȡ��ʱʹ�������Ա
	RpcMsg()
	{
		MdData = nullptr;
		MdCmd = 0;
	}
};

class CRpcServer :private CServiceNoBlock
{
private:
	CThreadPool											MdThreadPool;		// ִ�й���ʱ���̳߳�
	std::map<int, std::tuple<RemoteProc, int, int>>		MdCallList;			// ע��Ĺ��̱��ֱ��������̺š����̺����������ĳ��ȡ�����ֵ�ĳ���
	std::shared_mutex									MdCallListMutex;	// �ñ�Ļ���Ԫ
	std::map<CSocketObj*, std::tuple<int, std::future<std::shared_ptr<char[]>>>>	MdProcRet;		// ���̷����̳߳�ִ��ʱ�᷵��һ��std::future<void*>���Ա�����첽ȡ�øù��̵ķ���ֵ
																									// �ֱ������ͻ������ӡ�ִ�еĹ��̺š�����ȡ�÷���ֵ��future����
	std::shared_mutex																MdProcRetMutex; // ���̽��������
																			
public:
	CRpcServer(int HeartBeatTime = 300, int ServiceMaxPeoples = 100, int DisposeThreadNums = 1);
	virtual ~CRpcServer();
	void MfStart(const char* ip, unsigned short port, int threadNums = 3);	// �����շ��̺߳��̳߳�,threadNums�����̳߳��߳�����
	int MfRegCall(int CallNo, RemoteProc Call, int ArgLen, int RetLen);		// ע��һ������
	int MfRemoveCall(int CallNo);											// �Ƴ�һ������
private:
	void MfCallRetrun();
	virtual void MfVNetMsgDisposeFun(SOCKET sock, CSocketObj* cli, CNetMsgHead* msg, std::thread::id& threadid);
};

class CRpcClient :private CClientLinkManage
{
private:
public:
	CRpcClient();
	virtual ~CRpcClient();
	void MfStart();																					// �����շ��߳�
	int MfConnectRpcServer(std::string Linkname, const char* ip, unsigned short port);				// ����Rpc����
	void MfCloseRpclink(std::string Linkname);														// �رպ�Rpc������
	std::shared_ptr<char[]> MfRemote(std::string Linkname, int CallNo, void* data, int DataSize);	// Զ�̹��̵��ã������ȴ�
private:
	virtual void MfVNetMsgDisposeFun(SOCKET sock, CSocketObj* cli, CNetMsgHead* msg, std::thread::id& threadid);
};