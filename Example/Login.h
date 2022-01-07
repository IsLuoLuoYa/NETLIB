#include <mysql/mysql.h>
#include <string>
#include <map>
#include <mutex>
#include <shared_mutex>
#include "../static_base/Service.h"
#include "../static_base/NetMsgStruct.h"
#include "../static_base/ThreadPool.h"
#include "../static_base/RpcCS.h"
#include "../ApplyProto.h"

class CLogInServer : private CServiceNoBlock
{
private:
	MYSQL MdMysql;		// ���ݿ����Ӷ���
	std::map<std::string, CMyNetAddr> MdServerList;
	std::mutex MdServerListMtx;
	std::string MdJoinHall = "JoinHall";
	CThreadPool MdTp;
	CRpcServer MdRpcServer;
	CRpcClient MdRpcClient;
public:
	CLogInServer();
	~CLogInServer();
	void MfStart
	(	const char* ip, unsigned short port,		// ��ǰ����󶨵ĵ�ַ��
		const char* DataBaseIp, const char* DataBaseUser, const char* DataBasePass, const char * DataBaseName,
		const char* ip2, unsigned short port2,		// Rpc����󶨵ĵ�ַ
		const char* ip3, unsigned short port3		// Rpc�ͻ�Ҫ���ӵĵ�ַ
	);
private:
	virtual void MfVNetMsgDisposeFun(SOCKET sock, CSocketObj* cli, CNetMsgHead* msg, std::thread::id& threadid);
	std::shared_ptr<char[]> MfRegServerAddr(std::shared_ptr<char[]> a) ;
	void Mf_Login(SOCKET sock, CSocketObj* cli, CNetMsgHead* msg, std::thread::id& threadid);
	void Mf_PullServerListToClent(SOCKET sock, CSocketObj* cli, CNetMsgHead* msg, std::thread::id& threadid);
};