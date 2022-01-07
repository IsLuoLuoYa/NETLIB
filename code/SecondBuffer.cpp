#include <thread>
#include <string.h>
#include "SecondBuffer.h"
#include "Log.h"
#include "NetMsgStruct.h"
CSecondBuffer::CSecondBuffer() :MdPBuffer(nullptr), MdBufferLen(-1), Mdtail(0)
{
	MdBufferLen = DEFAULTBUFFERLEN;
	MdPBuffer = new char[MdBufferLen] {};
}

CSecondBuffer::CSecondBuffer(int bufferlen) :MdPBuffer(nullptr), MdBufferLen(-1), Mdtail(0)
{
	if (bufferlen <= 0)
	{
		LogFormatMsgAndSubmit(std::this_thread::get_id() , FATAL_FairySun, "Socket Second Buffer LenValue Error");
		ErrorWaitExit();
	}

	MdBufferLen = bufferlen;
	MdPBuffer = new char[MdBufferLen] {};
}

CSecondBuffer::~CSecondBuffer()
{
	if (MdPBuffer)
		delete[] MdPBuffer;
	MdPBuffer = nullptr;
}

char* CSecondBuffer::MfGetBufferP()
{
	return MdPBuffer;
}

bool CSecondBuffer::MfDataToBuffer(const char* data, int len)
{
	std::lock_guard<std::mutex> lk(MdMtx);
	if (Mdtail + len <= MdBufferLen)
	{
		memcpy(MdPBuffer + Mdtail, data, len);
		Mdtail += len;
		return true;
	}
	return false;
}

int CSecondBuffer::MfBufferToSocket(SOCKET sock)
{
	int ret = 0;
	std::unique_lock<std::mutex> lk(MdMtx, std::defer_lock);
	if (lk.try_lock())			// 锁定缓冲区
	{
		if (Mdtail > 0)			// 有数据时
		{
			if (MfSend(sock, MdPBuffer, Mdtail, &ret))			// ret值可用时
			{
				if (ret <= 0)									// 如果该值小于等于0，代表socket出错 或者 对端关闭，原样返回ret值
					return ret;
				Mdtail -= ret;									// 否则就是成功写入socket缓冲区，更新值
				memcpy(MdPBuffer, MdPBuffer + ret, Mdtail);		// 缓冲区中未被发送的数据移动到缓冲区起始位置
			}
		}
	}
	// 只有在成功锁定后 && 缓冲区有数据 && send返回值可用 同时成立的情况下
	// 才返回真实的ret，否则就是返回INT32_MAX，来表示没有出错，但是不代表接受了数据，只被调用者用于判断是否出错
	return INT32_MAX;
}

int CSecondBuffer::MfSocketToBuffer(SOCKET sock)
{
	// 没有出错时返回的值都是大于等于0的，但是返回值是INT32_MAX时，没有出错，但是也没有成功压入数据
	int ret = 0;
	std::unique_lock<std::mutex> lk(MdMtx, std::defer_lock);
	if (lk.try_lock())		// 锁定缓冲区
	{
		if (MdBufferLen - Mdtail > 0)		// 缓冲区又空间
		{
			if (MfRecv(sock, MdPBuffer + Mdtail, MdBufferLen - Mdtail, &ret))	// ret值可用时
			{
				if (ret <= 0)													// 如果该值小于等于0，代表socket出错 或者 对端关闭，原样返回ret值
					return ret;
				Mdtail += ret;													// 否则就是成功写入socket缓冲区，更新值
				return ret;
			}
		}
	}
	// 只有在成功锁定后 && 剩余空间大于0 && recv返回值可用 同时成立的情况下
	// 才返回真实的ret，否则就是返回INT32_MAX，来表示没有出错，但是不代表接受了数据，只被调用者用于判断是否出错
	return INT32_MAX;
}

bool CSecondBuffer::MfHasMsg()
{
	static int MSG_HEAD_LEN = sizeof(CNetMsgHead);
	if (Mdtail >= MSG_HEAD_LEN)
		return Mdtail >= ((CNetMsgHead*)MdPBuffer)->MdLen;
	return false;
}

void CSecondBuffer::MfPopFrontMsg()
{
	if (MfHasMsg())
		MfBufferPopData( ((CNetMsgHead*)MdPBuffer)->MdLen );
}

void CSecondBuffer::MfBufferPopData(int len)
{
	std::lock_guard<std::mutex> lk(MdMtx);
	int n = Mdtail - len;
	//printf("tail:%d, len:%d\n", Mdtail, len);
	if (n >= 0)
	{
		memcpy(MdPBuffer, MdPBuffer + len, n);
		Mdtail = n;
	}
}

bool CSecondBuffer::MfSend(SOCKET sock, const char* buf, int len, int * ret)
{
	*ret = (int)send(sock, buf, len, 0);
	if (0 <= *ret)					// 大于等于0时，要么对端正确关闭，要么发送成功，都使返回值可用，返回true
		return true;
	else							// 否则处理errno返回0，发送缓冲区满发送失败，被系统调用打断
	{
		if (0 == errno)					
			return false;
		if (EWOULDBLOCK == errno)	// 非阻塞模式，socket发送缓冲区已满的情况，这种情况下，不是错误，返回false，ret值不应该被用于更新tail
			return false;
		if (EINTR == errno)			// 同样，EINTR不是错误，返回false，表示返回值不可用，ret值不应该被用于更新tail
		{
			LogFormatMsgAndSubmit(std::this_thread::get_id(), WARN_FairySun, "SOCKET <%5d> SEND be interrupted, errno is EINTR", sock);
			return false;
		}

		// 这种情况下，返回值小于0，错误，ret值是send的返回值，返回false，ret值不可用，表示未解析的错误
		//LogFormatMsgAndSubmit(std::this_thread::get_id(), ERROR_FairySun, "SOCKET <%5d> SEND RETURN VAL:<%d>, errno(%d) : %s", sock, *ret, errno, strerror(errno));
		return false;
	}
}

bool CSecondBuffer::MfRecv(SOCKET sock, void* buf, int len,int* ret)
{
#ifndef WIN32
	*ret = (int)recv(sock, buf, len, 0);
#else
	*ret = recv(sock, (char *)buf, len, 0);
#endif // !WIN32

	if (0 <= *ret)				// 大于等于0时，要么错误正确接收，要么对端关闭，返回值都可用，返回true
		return true;
	else						// 否则处理recv被系统调用打断，非阻塞接收缓冲区空，以及errno为0的情况
	{
		if (0 == errno)					
			return false;
		if (EWOULDBLOCK == errno)		// 非阻塞模式，socket接收缓冲区空的情况，这种情况下，不是错误，返回false，ret值不应该被用于更新tail或head
			return false;
		if (EINTR == errno)				// 同样，EINTR不是错误，返回false，返回值不可用，ret值不应该被用于更新tail或head
		{
			LogFormatMsgAndSubmit(std::this_thread::get_id(), WARN_FairySun, "SOCKET <%5d> RECV be interrupted, errno is EINTR", sock);
			return false;
		}

		// 这种情况下，返回值小于0，错误，ret值是send的返回值，返回false，ret值不可用，表示未解析的错误
		//LogFormatMsgAndSubmit(std::this_thread::get_id(), ERROR_FairySun, "SOCKET <%5d> RECV RETURN VAL:<%5d>, errno(%d) : %s", sock, *ret, errno, strerror(errno));
		return false;
	}
}
