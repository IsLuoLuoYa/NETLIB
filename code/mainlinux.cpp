#include "BaseControl.h"
#include "test.h"

int main()
{
	FairySunOfNetBaseStart();
	
		CTestSevice S1(6, 1000, 5);
		S1.Mf_Epoll_Start(0, 4567);
		//S1.Mf_NoBlock_Start(0, 4567);
	
	getchar();
	FairySunOfNetBaseOver();
	return 0;
}




//---------------------------------线程池测试
//#include "AllHeadFile.h"
//#include "ThreadPool.h"
//
//int testF1(int count, int i)
//{
//	printf("i第%d次，i=%d\n", count, i);
//	std::this_thread::sleep_for(std::chrono::seconds(i));
//	return i;
//}
//
//float testF2(int count, int i)
//{
//	printf("f第%d次，i=%d\n", count, i);
//	std::this_thread::sleep_for(std::chrono::seconds(i));
//	return static_cast<float>(i);
//}
//
//int main()
//{
//	CThreadPool pool;
//	pool.MfStart(3);
//	std::vector< std::future<int> > results1;
//	std::vector< std::future<float> > results2;
//	for (int i = 0; i < 5; ++i)
//	{
//		results1.emplace_back(pool.MfEnqueue(testF1, i + 1, rand() % 5));
//		results2.emplace_back(pool.MfEnqueue(testF2, i + 1, rand() % 5));
//	}
//	std::this_thread::sleep_for(std::chrono::seconds(15));
//
//	printf("111111111111\n");
//	for (auto&& result : results1)
//		printf("%d\n", result.get());
//	printf("222222222222\n");
//	for (auto&& result : results2)
//		printf("%f\n", result.get());
//	std::this_thread::sleep_for(std::chrono::seconds(1000));
//	return 0;
//}


//---------------------------------Rpc测试
//#include "BaseControl.h"
//#include "RpcCS.h"
//
//	std::shared_ptr<char[]> testf1(std::shared_ptr<char[]> a)
//	{
//		printf("test11111\n");
//		char* ret = new char[sizeof(int)];
//		int s = *((int*)a.get()) + 1;
//		*(int*)ret = s;
//		std::this_thread::sleep_for(std::chrono::seconds(5));
//		return std::shared_ptr<char[]>(ret);
//	}
//	std::shared_ptr<char[]> testf2(std::shared_ptr<char[]> a)
//	{
//		printf("test22222\n");
//		char* ret = new char[sizeof(int)];
//		*(int*)ret = *(int*)a.get() + 2;
//		return std::shared_ptr<char[]>(ret);
//	}
//
//	struct CMyNetAddr
//	{
//		int		MdServerNameSize;
//		char	MdServerName[64];
//		int		MdIpSize;
//		char	MdIp[20];
//		u_short MdPort;
//	};
//
//	std::shared_ptr<char[]> serverlist(std::shared_ptr<char[]> a)
//	{
//		CMyNetAddr* temp = (CMyNetAddr*)a.get();
//		printf("111\n");
//		printf("%s, %s, %d\n", temp->MdServerName, temp->MdIp, temp->MdPort);
//		printf("222\n");
//		return a;
//	}
//
//	int main()
//	{
//		FairySunOfNetBaseStart();
//		
//
//			CRpcServer s;
//			s.MfStart(0, 4567);
//			//s.MfRegCall(1, testf1, sizeof(int), sizeof(int));
//			//s.MfRegCall(2, testf2, sizeof(int), sizeof(int));
//			//s.MfRegCall(2, testf2, sizeof(int), sizeof(int));
//			//s.MfRemoveCall(1);
//			//s.MfRemoveCall(2);
//			s.MfRegCall(1, serverlist, sizeof(CMyNetAddr), sizeof(CMyNetAddr));
//
//			s.~CRpcServer();
//		
//
//		getchar();
//		FairySunOfNetBaseOver();
//		return 0;
//	}