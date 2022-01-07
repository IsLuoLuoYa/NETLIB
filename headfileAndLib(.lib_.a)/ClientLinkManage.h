#pragma once
#include <map>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include "NetMsgStruct.h"
#include "ThreadManage.h"
#include "Time.h"
#include "ThreadPool.h"

class CClientLink;
struct CNetMsgHead;
class CTimer;

class CClientLinkManage
{
private:
	std::map<std::string, CClientLink*>		MdClientLinkList;
	std::shared_mutex						MdClientLinkListMtx;
	std::atomic<int>						MdIsStart = 0;			// �շ��߳��Ƿ�������������ʱ������������ӣ���Ϊ������ڹ����������߳���Σ�յ�
	Barrier*								MdBarrier;				// ���ڴ�������ǰ���շ��߳�����
	int										MdHeartSendInterval;	// ��������ʱ��������λ��
	CNetMsgHead*							MdDefautHeartPacket;	// Ĭ�ϵ�����������
	CTimer*									MdHeartTime;			// ������ʱ������
	CThreadPool								MdThreadPool;
public:
	CClientLinkManage(int HeartSendInterval = 3);
	~CClientLinkManage();
	void MfStart();													// �����շ��߳�
	int MfCreateAddLink(std::string Linkname, const char* ip, unsigned short port);		// �����Ҫ�����µ����ӣ���newһ��ClientLink��ͬʱ��¼���ӵ�Ŀ�꣬Ȼ������б��ǣ�����client����
	void MfCloseLink(std::string Linkname);							// �ر�ĳ������
	bool MfSendData(std::string str, const char* data, int len);	// �������ݣ����뻺����
	const char* MfGetRecvBufferP(std::string name);					// ���ؽ��ջ�������ָ�룬����ֱ�Ӷ����������
	bool MfHasMsg(std::string name);								// �жϻ������Ƿ�������
	void MfPopFrontMsg(std::string name);							// ���������ݰ�������
	bool MfLinkIsSurvive(std::string name);							// ĳ�������Ƿ����
private:
	void MfSendThread();											// �����̣߳�ѭ������send���շ��̸߳����Ƿ���ñ�־ȷ����Ϊ
	void MfRecvThread();											// �����̣߳�ѭ������recv���շ��̸߳����Ƿ���ñ�־ȷ����Ϊ
};