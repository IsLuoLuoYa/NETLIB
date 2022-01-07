#pragma once

struct CNetMsgHead
{
	int MdLen;		// 该包总长度
	int MdCmd;		// 该包执行的操作
	CNetMsgHead()
	{
		MdLen = sizeof(CNetMsgHead);
		MdCmd = -1;				// 该值为-1时默认为心跳包
	}
};